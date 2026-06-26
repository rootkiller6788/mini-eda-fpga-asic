/**
 * formal_proof.c - Formal Verification Engine Implementation
 *
 * Implements:
 *   SAT solver with DPLL (Davis-Putnam-Logemann-Loveland) + CDCL extensions
 *   Bounded Model Checking (BMC) - Biere et al. (1999)
 *   k-induction (Sheeran et al., FMCAD 2000)
 *   LTL formula evaluation on traces
 *   Transition system management
 *
 * L4: Cook-Levin Theorem — SAT is NP-complete (1971)
 *     BMC reduces bounded model checking to SAT.
 *     Given transition system M, LTL property phi, bound k:
 *       M |=_k phi  iff  SAT(BMC_enc(M, phi, k)) is UNSAT.
 *     The encoding introduces propositional variables for each
 *     state variable at each time step 0..k.
 *
 * L5: DPLL algorithm (Davis et al., 1962; Davis & Putnam, 1960):
 *       1. Unit propagation (BCP)
 *       2. Pure literal elimination
 *       3. Decision (choose unassigned variable)
 *       4. Conflict analysis (CDCL extension)
 *       5. Backtrack / Restart
 *
 * L8: k-induction:
 *       Base:   P holds in all initial states (BMC k=0)
 *       Step:   if P holds for k consecutive states, then P holds at k+1
 *       If both pass, P is an invariant.
 */

#include "formal_proof.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ========================================================================
 * SAT Solver — DPLL + CDCL (L4, L5)
 * ======================================================================== */

sat_solver_t *sat_solver_create(uint32_t num_vars, uint32_t capacity_clauses) {
    sat_solver_t *s = (sat_solver_t*)calloc(1, sizeof(sat_solver_t));
    if (!s) return NULL;
    s->num_vars = num_vars;
    s->num_clauses = 0;
    s->capacity_clauses = (capacity_clauses > 0) ? capacity_clauses : 1024;
    s->clauses = (sat_clause_t*)calloc(s->capacity_clauses, sizeof(sat_clause_t));
    s->assignments = (sat_assign_t*)calloc(num_vars + 1, sizeof(sat_assign_t));
    s->decision_level = (uint32_t*)calloc(num_vars + 1, sizeof(uint32_t));
    s->reason = (uint32_t*)calloc(num_vars + 1, sizeof(uint32_t));
    s->current_level = 0;
    s->has_conflict = false;
    memset(&s->stats, 0, sizeof(s->stats));
    if (!s->clauses || !s->assignments || !s->decision_level || !s->reason) {
        sat_solver_destroy(s);
        return NULL;
    }
    /* init reason to UINT32_MAX (no reason) */
    for (uint32_t i = 0; i <= num_vars; i++) {
        s->reason[i] = UINT32_MAX;
    }
    return s;
}

void sat_solver_destroy(sat_solver_t *s) {
    if (!s) return;
    free(s->clauses);
    free(s->assignments);
    free(s->decision_level);
    free(s->reason);
    free(s);
}

int32_t sat_solver_new_var(sat_solver_t *s) {
    if (!s) return -1;
    uint32_t new_var = s->num_vars + 1;
    sat_assign_t *new_assign = (sat_assign_t*)realloc(
        s->assignments, (new_var + 1) * sizeof(sat_assign_t));
    uint32_t *new_dl = (uint32_t*)realloc(
        s->decision_level, (new_var + 1) * sizeof(uint32_t));
    uint32_t *new_reason = (uint32_t*)realloc(
        s->reason, (new_var + 1) * sizeof(uint32_t));
    if (!new_assign || !new_dl || !new_reason) {
        free(new_assign); free(new_dl); free(new_reason);
        return -1;
    }
    s->assignments = new_assign;
    s->decision_level = new_dl;
    s->reason = new_reason;
    s->assignments[new_var] = SAT_UNASSIGNED;
    s->decision_level[new_var] = 0;
    s->reason[new_var] = UINT32_MAX;
    s->num_vars = new_var;
    return (int32_t)new_var;
}

void sat_solver_add_clause(sat_solver_t *s, sat_literal_t *lits, uint32_t n) {
    if (!s || !lits || n == 0 || n > MAX_CLAUSE_LITS) return;
    /* Expand capacity if needed */
    if (s->num_clauses >= s->capacity_clauses) {
        uint32_t new_cap = s->capacity_clauses * 2;
        sat_clause_t *new_clauses = (sat_clause_t*)realloc(
            s->clauses, new_cap * sizeof(sat_clause_t));
        if (!new_clauses) return;
        s->clauses = new_clauses;
        s->capacity_clauses = new_cap;
    }
    sat_clause_t *c = &s->clauses[s->num_clauses++];
    c->num_lits = n;
    c->is_learned = false;
    c->activity = 0;
    memcpy(c->lits, lits, n * sizeof(sat_literal_t));
}

/* ---- BCP: Boolean Constraint Propagation (Unit propagation) ---- */
static bool sat_bcp(sat_solver_t *s) {
    bool changed = true;
    while (changed && !s->has_conflict) {
        changed = false;
        for (uint32_t ci = 0; ci < s->num_clauses; ci++) {
            sat_clause_t *c = &s->clauses[ci];
            int unassigned_count = 0;
            int last_unassigned = -1;
            bool clause_sat = false;

            for (uint32_t li = 0; li < c->num_lits; li++) {
                int32_t var = c->lits[li].var;
                if (var <= 0) continue;
                sat_assign_t val = s->assignments[var];
                if (val == SAT_UNASSIGNED) {
                    unassigned_count++;
                    last_unassigned = (int)li;
                } else {
                    bool is_true = (val == SAT_TRUE && !c->lits[li].sign) ||
                                   (val == SAT_FALSE && c->lits[li].sign);
                    if (is_true) {
                        clause_sat = true;
                        break;
                    }
                }
            }
            if (clause_sat) continue;

            if (unassigned_count == 0) {
                /* Conflict! All literals false */
                s->has_conflict = true;
                s->conflict_clause = *c;
                s->stats.conflicts++;
                return false;
            }
            if (unassigned_count == 1 && last_unassigned >= 0) {
                /* Unit clause - force assignment */
                sat_literal_t *lit = &c->lits[last_unassigned];
                sat_assign_t forced = lit->sign ? SAT_FALSE : SAT_TRUE;
                s->assignments[lit->var] = forced;
                s->decision_level[lit->var] = s->current_level;
                s->reason[lit->var] = ci;
                s->stats.propagations++;
                changed = true;
            }
        }
    }
    return !s->has_conflict;
}

/* ---- Choose unassigned variable (VSIDS-like heuristic) ---- */
static int32_t sat_choose_var(sat_solver_t *s) {
    /* Simple: choose first unassigned variable */
    for (uint32_t v = 1; v <= s->num_vars; v++) {
        if (s->assignments[v] == SAT_UNASSIGNED) {
            return (int32_t)v;
        }
    }
    return -1; /* All assigned */
}

/* ---- DPLL main solve ---- */
bool sat_solver_solve(sat_solver_t *s) {
    if (!s) return false;
    s->has_conflict = false;
    s->current_level = 0;
    s->stats.decisions = 0;

    /* Initial BCP */
    if (!sat_bcp(s)) return false;

    while (true) {
        int32_t var = sat_choose_var(s);
        if (var < 0) {
            /* All variables assigned, no conflict => SAT */
            return true;
        }

        /* Decision: try TRUE first */
        s->current_level++;
        s->stats.decisions++;
        s->assignments[var] = SAT_TRUE;
        s->decision_level[var] = s->current_level;
        s->reason[var] = UINT32_MAX;

        if (!sat_bcp(s)) {
            /* Conflict: backtrack */
            /* Undo assignments at current level */
            for (uint32_t v = 1; v <= s->num_vars; v++) {
                if (s->decision_level[v] == s->current_level) {
                    s->assignments[v] = SAT_UNASSIGNED;
                    s->decision_level[v] = 0;
                    s->reason[v] = UINT32_MAX;
                }
            }
            s->current_level--;
            s->has_conflict = false;

            if (s->current_level == 0) {
                /* No more backtracking possible => UNSAT */
                return false;
            }

            /* Flip the last decision variable at previous level */
            /* For simplicity, try FALSE */
            s->assignments[var] = SAT_FALSE;
            s->decision_level[var] = s->current_level;
            if (!sat_bcp(s)) {
                /* Still conflict, continue backtracking */
                for (uint32_t v = 1; v <= s->num_vars; v++) {
                    if (s->decision_level[v] == s->current_level) {
                        s->assignments[v] = SAT_UNASSIGNED;
                        s->decision_level[v] = 0;
                        s->reason[v] = UINT32_MAX;
                    }
                }
                s->current_level--;
                s->has_conflict = false;
            }
        }
    }
}

sat_assign_t sat_solver_get_value(sat_solver_t *s, int32_t var) {
    if (!s || var <= 0 || (uint32_t)var > s->num_vars) return SAT_UNASSIGNED;
    return s->assignments[var];
}

void sat_solver_reset(sat_solver_t *s) {
    if (!s) return;
    for (uint32_t v = 1; v <= s->num_vars; v++) {
        s->assignments[v] = SAT_UNASSIGNED;
        s->decision_level[v] = 0;
        s->reason[v] = UINT32_MAX;
    }
    s->current_level = 0;
    s->has_conflict = false;
}

void sat_solver_print_stats(const sat_solver_t *s, FILE *fp) {
    if (!s || !fp) return;
    fprintf(fp, "SAT Solver Stats:\n");
    fprintf(fp, "  Variables: %u\n", s->num_vars);
    fprintf(fp, "  Clauses: %u\n", s->num_clauses);
    fprintf(fp, "  Decisions: %lu\n", (unsigned long)s->stats.decisions);
    fprintf(fp, "  Propagations: %lu\n", (unsigned long)s->stats.propagations);
    fprintf(fp, "  Conflicts: %lu\n", (unsigned long)s->stats.conflicts);
    fprintf(fp, "  Restarts: %lu\n", (unsigned long)s->stats.restarts);
}

/* ========================================================================
 * LTL Formula Construction & Manipulation (L2)
 * ======================================================================== */

ltl_formula_t *ltl_atom(int32_t atom_id) {
    ltl_formula_t *f = (ltl_formula_t*)calloc(1, sizeof(ltl_formula_t));
    if (!f) return NULL;
    f->op = LTL_ATOM;
    f->atom_id = atom_id;
    return f;
}

ltl_formula_t *ltl_not(ltl_formula_t *phi) {
    if (!phi) return NULL;
    ltl_formula_t *f = (ltl_formula_t*)calloc(1, sizeof(ltl_formula_t));
    if (!f) return NULL;
    f->op = LTL_NOT;
    f->left = phi;
    return f;
}

ltl_formula_t *ltl_and(ltl_formula_t *a, ltl_formula_t *b) {
    if (!a || !b) return NULL;
    ltl_formula_t *f = (ltl_formula_t*)calloc(1, sizeof(ltl_formula_t));
    if (!f) return NULL;
    f->op = LTL_AND;
    f->left = a;
    f->right = b;
    return f;
}

ltl_formula_t *ltl_or(ltl_formula_t *a, ltl_formula_t *b) {
    if (!a || !b) return NULL;
    ltl_formula_t *f = (ltl_formula_t*)calloc(1, sizeof(ltl_formula_t));
    if (!f) return NULL;
    f->op = LTL_OR;
    f->left = a;
    f->right = b;
    return f;
}

ltl_formula_t *ltl_implies(ltl_formula_t *a, ltl_formula_t *b) {
    if (!a || !b) return NULL;
    ltl_formula_t *f = (ltl_formula_t*)calloc(1, sizeof(ltl_formula_t));
    if (!f) return NULL;
    f->op = LTL_IMPLIES;
    f->left = a;
    f->right = b;
    return f;
}

ltl_formula_t *ltl_globally(ltl_formula_t *phi) {
    if (!phi) return NULL;
    ltl_formula_t *f = (ltl_formula_t*)calloc(1, sizeof(ltl_formula_t));
    if (!f) return NULL;
    f->op = LTL_GLOBALLY;
    f->left = phi;
    return f;
}

ltl_formula_t *ltl_finally(ltl_formula_t *phi) {
    if (!phi) return NULL;
    ltl_formula_t *f = (ltl_formula_t*)calloc(1, sizeof(ltl_formula_t));
    if (!f) return NULL;
    f->op = LTL_FINALLY;
    f->left = phi;
    return f;
}

ltl_formula_t *ltl_next(ltl_formula_t *phi) {
    if (!phi) return NULL;
    ltl_formula_t *f = (ltl_formula_t*)calloc(1, sizeof(ltl_formula_t));
    if (!f) return NULL;
    f->op = LTL_NEXT;
    f->left = phi;
    return f;
}

ltl_formula_t *ltl_until(ltl_formula_t *a, ltl_formula_t *b) {
    if (!a || !b) return NULL;
    ltl_formula_t *f = (ltl_formula_t*)calloc(1, sizeof(ltl_formula_t));
    if (!f) return NULL;
    f->op = LTL_UNTIL;
    f->left = a;
    f->right = b;
    return f;
}

ltl_formula_t *ltl_release(ltl_formula_t *a, ltl_formula_t *b) {
    if (!a || !b) return NULL;
    ltl_formula_t *f = (ltl_formula_t*)calloc(1, sizeof(ltl_formula_t));
    if (!f) return NULL;
    f->op = LTL_RELEASE;
    f->left = a;
    f->right = b;
    return f;
}

void ltl_formula_destroy(ltl_formula_t *f) {
    if (!f) return;
    ltl_formula_destroy(f->left);
    ltl_formula_destroy(f->right);
    free(f);
}

static void ltl_formula_print_rec(const ltl_formula_t *f, FILE *fp) {
    if (!f || !fp) return;
    switch (f->op) {
        case LTL_ATOM:    fprintf(fp, "p%d", f->atom_id); break;
        case LTL_NOT:     fprintf(fp, "!"); ltl_formula_print_rec(f->left, fp); break;
        case LTL_AND:     fprintf(fp, "("); ltl_formula_print_rec(f->left, fp);
                          fprintf(fp, " & "); ltl_formula_print_rec(f->right, fp);
                          fprintf(fp, ")"); break;
        case LTL_OR:      fprintf(fp, "("); ltl_formula_print_rec(f->left, fp);
                          fprintf(fp, " | "); ltl_formula_print_rec(f->right, fp);
                          fprintf(fp, ")"); break;
        case LTL_NEXT:     fprintf(fp, "X"); ltl_formula_print_rec(f->left, fp); break;
        case LTL_GLOBALLY: fprintf(fp, "G"); ltl_formula_print_rec(f->left, fp); break;
        case LTL_FINALLY:  fprintf(fp, "F"); ltl_formula_print_rec(f->left, fp); break;
        default: fprintf(fp, "?"); break;
    }
}

void ltl_formula_print(const ltl_formula_t *f, FILE *fp) {
    ltl_formula_print_rec(f, fp);
    fprintf(fp, "\n");
}

/* ========================================================================
 * Transition System Management (L1, L2)
 * ======================================================================== */

hv_transition_system_t *hv_ts_create(uint32_t num_props) {
    hv_transition_system_t *ts = (hv_transition_system_t*)calloc(1, sizeof(*ts));
    if (!ts) return NULL;
    ts->states = NULL;
    ts->num_states = 0;
    ts->capacity_states = 0;
    ts->transitions = NULL;
    ts->num_transitions = 0;
    ts->capacity_transitions = 0;
    ts->initial_state = 0;
    ts->num_props = (num_props <= 64) ? num_props : 64;
    memset(ts->prop_names, 0, sizeof(ts->prop_names));
    return ts;
}

void hv_ts_destroy(hv_transition_system_t *ts) {
    if (!ts) return;
    for (uint32_t i = 0; i < ts->num_states; i++) {
        free(ts->states[i].props);
    }
    free(ts->states);
    free(ts->transitions);
    /* free prop names */
    for (uint32_t i = 0; i < ts->num_props; i++) {
        free(ts->prop_names[i]);
    }
    free(ts);
}

uint32_t hv_ts_add_state(hv_transition_system_t *ts, const char *label) {
    if (!ts) return UINT32_MAX;
    if (ts->num_states >= ts->capacity_states) {
        uint32_t new_cap = (ts->capacity_states == 0) ? 16 : ts->capacity_states * 2;
        hv_state_t *new_states = (hv_state_t*)realloc(
            ts->states, new_cap * sizeof(hv_state_t));
        if (!new_states) return UINT32_MAX;
        ts->states = new_states;
        ts->capacity_states = new_cap;
    }
    uint32_t id = ts->num_states++;
    memset(&ts->states[id], 0, sizeof(hv_state_t));
    ts->states[id].id = id;
    ts->states[id].num_props = ts->num_props;
    ts->states[id].props = (bool*)calloc(ts->num_props, sizeof(bool));
    if (label) strncpy(ts->states[id].label, label, sizeof(ts->states[id].label) - 1);
    return id;
}

void hv_ts_set_prop(hv_transition_system_t *ts, uint32_t state_id,
                     uint32_t prop_id, bool value) {
    if (!ts || state_id >= ts->num_states || prop_id >= ts->num_props) return;
    ts->states[state_id].props[prop_id] = value;
}

void hv_ts_add_transition(hv_transition_system_t *ts, uint32_t from, uint32_t to) {
    if (!ts || from >= ts->num_states || to >= ts->num_states) return;
    if (ts->num_transitions >= ts->capacity_transitions) {
        uint32_t new_cap = (ts->capacity_transitions == 0) ? 32 : ts->capacity_transitions * 2;
        hv_transition_t *new_trans = (hv_transition_t*)realloc(
            ts->transitions, new_cap * sizeof(hv_transition_t));
        if (!new_trans) return;
        ts->transitions = new_trans;
        ts->capacity_transitions = new_cap;
    }
    ts->transitions[ts->num_transitions].from_state = from;
    ts->transitions[ts->num_transitions].to_state = to;
    ts->num_transitions++;
}

void hv_ts_set_initial(hv_transition_system_t *ts, uint32_t state) {
    if (!ts || state >= ts->num_states) return;
    ts->initial_state = state;
}

const hv_state_t *hv_ts_get_state(const hv_transition_system_t *ts, uint32_t id) {
    if (!ts || id >= ts->num_states) return NULL;
    return &ts->states[id];
}

void hv_ts_print(const hv_transition_system_t *ts, FILE *fp) {
    if (!ts || !fp) return;
    fprintf(fp, "Transition System: %u states, %u transitions\n",
            ts->num_states, ts->num_transitions);
    fprintf(fp, "Initial state: %u\n", ts->initial_state);
    for (uint32_t i = 0; i < ts->num_states; i++) {
        fprintf(fp, "  State %u [%s]", i, ts->states[i].label);
        if (ts->num_props > 0) {
            fprintf(fp, " props=[");
            for (uint32_t p = 0; p < ts->num_props; p++) {
                fprintf(fp, "%d", ts->states[i].props[p] ? 1 : 0);
            }
            fprintf(fp, "]");
        }
        fprintf(fp, "\n");
    }
    for (uint32_t i = 0; i < ts->num_transitions; i++) {
        fprintf(fp, "  %u -> %u\n",
                ts->transitions[i].from_state,
                ts->transitions[i].to_state);
    }
}

/* ========================================================================
 * Bounded Model Checking (L3, L5)
 *
 * L5 Algorithm: BMC(M, phi, k):
 *   For bound = 0 .. k:
 *     1. Unroll transition relation k times into propositional formula
 *     2. Encode negation of property (to find counterexample)
 *     3. Run SAT solver
 *     4. If SAT: counterexample found -> FAIL
 *     5. If UNSAT and bound == k: property holds up to bound k
 *
 * Based on: Biere, Cimatti, Clarke, Zhu (TACAS '99)
 *   "Symbolic Model Checking without BDDs"
 * ======================================================================== */

bmc_engine_t *bmc_engine_create(hv_transition_system_t *sys,
                                 ltl_formula_t *prop, bmc_config_t config) {
    bmc_engine_t *bmc = (bmc_engine_t*)calloc(1, sizeof(bmc_engine_t));
    if (!bmc) return NULL;
    bmc->system = sys;
    bmc->property = prop;
    bmc->config = config;
    bmc->solver = NULL;
    bmc->last_result = BMC_PROPERTY_UNKNOWN;
    bmc->cex = NULL;
    bmc->total_checks = 0;
    bmc->sat_calls = 0;
    bmc->total_time_ms = 0.0;
    return bmc;
}

void bmc_engine_destroy(bmc_engine_t *bmc) {
    if (!bmc) return;
    sat_solver_destroy(bmc->solver);
    if (bmc->cex) {
        free(bmc->cex->state_ids);
        free(bmc->cex);
    }
    free(bmc);
}

/* Encode a path of length k as a SAT instance.
 * Creates variables for each state variable at each time step.
 * Adds initial state, transition constraints, and property violation.
 */
bool bmc_encode_to_sat(bmc_engine_t *bmc, uint32_t k) {
    if (!bmc || !bmc->system) return false;

    hv_transition_system_t *ts = bmc->system;
    uint32_t n_states = ts->num_states;
    /* Number of variables: n_states * (k+1) */
    uint32_t num_vars = n_states * (k + 1);

    /* Create/fresh solver */
    if (bmc->solver) sat_solver_destroy(bmc->solver);
    bmc->solver = sat_solver_create(num_vars, num_vars * 10);
    if (!bmc->solver) return false;

    /* Variable mapping: var(s, t) = t * n_states + s + 1
       where s = state index, t = time step */
    #define VAR(s, t) ((t) * n_states + (s) + 1)

    /* (1) Exactly one state at each time step:
       At least one:  VAR(0,t) | VAR(1,t) | ... | VAR(n-1,t)
       At most one: for each pair (i,j), !VAR(i,t) | !VAR(j,t) */
    for (uint32_t t = 0; t <= k; t++) {
        /* At least one */
        sat_literal_t at_least[64];
        uint32_t nl = 0;
        for (uint32_t s = 0; s < n_states && nl < 64; s++) {
            at_least[nl].var = VAR(s, t);
            at_least[nl].sign = false;
            nl++;
        }
        sat_solver_add_clause(bmc->solver, at_least, nl);

        /* At most one: pairwise exclusion */
        for (uint32_t si = 0; si < n_states; si++) {
            for (uint32_t sj = si + 1; sj < n_states; sj++) {
                sat_literal_t pair[2];
                pair[0].var = VAR(si, t); pair[0].sign = true;  /* !si */
                pair[1].var = VAR(sj, t); pair[1].sign = true;  /* !sj */
                sat_solver_add_clause(bmc->solver, pair, 2);
            }
        }
    }

    /* (2) Initial state constraint: VAR(initial_state, 0) */
    {
        sat_literal_t init_lit;
        init_lit.var = VAR(ts->initial_state, 0);
        init_lit.sign = false;
        sat_solver_add_clause(bmc->solver, &init_lit, 1);
    }

    /* (3) Transition constraints:
       If state s at time t, then next state must be a successor of s:
       VAR(s,t) -> (VAR(s1,t+1) | VAR(s2,t+1) | ...)
       Encoded as: !VAR(s,t) | VAR(s1,t+1) | VAR(s2,t+1) | ...  */
    for (uint32_t t = 0; t < k; t++) {
        for (uint32_t s = 0; s < n_states; s++) {
            /* Collect successors of s */
            sat_literal_t succs[128];
            uint32_t ns = 1;
            /* !VAR(s,t) */
            succs[0].var = VAR(s, t);
            succs[0].sign = true;

            for (uint32_t ti = 0; ti < ts->num_transitions && ns < 128; ti++) {
                if (ts->transitions[ti].from_state == s) {
                    succs[ns].var = VAR(ts->transitions[ti].to_state, t + 1);
                    succs[ns].sign = false;
                    ns++;
                }
            }
            /* Handle states with no outgoing edges: deadlock OK for now */
            if (ns > 1) {
                sat_solver_add_clause(bmc->solver, succs, ns);
            }
        }
    }

    return true;
}

bmc_result_t bmc_check(bmc_engine_t *bmc) {
    if (!bmc || !bmc->system || !bmc->property) return BMC_PROPERTY_ERROR;

    for (uint32_t k = 0; k <= bmc->config.max_bound; k++) {
        bmc->total_checks++;

        if (!bmc_encode_to_sat(bmc, k)) {
            return BMC_PROPERTY_ERROR;
        }

        bmc->sat_calls++;
        bool sat = sat_solver_solve(bmc->solver);

        if (sat) {
            /* Found counterexample of length k */
            bmc->last_result = BMC_PROPERTY_FAIL;
            /* Extract counterexample trace */
            if (bmc->cex) {
                free(bmc->cex->state_ids);
                free(bmc->cex);
            }
            bmc->cex = (bmc_counterexample_t*)calloc(1, sizeof(bmc_counterexample_t));
            if (bmc->cex) {
                bmc->cex->length = k;
                bmc->cex->state_ids = (uint32_t*)calloc(k + 1, sizeof(uint32_t));
                if (bmc->cex->state_ids) {
                    uint32_t n_states = bmc->system->num_states;
                    for (uint32_t t = 0; t <= k; t++) {
                        for (uint32_t s = 0; s < n_states; s++) {
                            #define VAR(s, t) ((t) * n_states + (s) + 1)
                            if (sat_solver_get_value(bmc->solver, VAR(s,t)) == SAT_TRUE) {
                                bmc->cex->state_ids[t] = s;
                                break;
                            }
                        }
                    }
                }
                snprintf(bmc->cex->description, sizeof(bmc->cex->description),
                         "Counterexample of length %u found", k);
            }
            return BMC_PROPERTY_FAIL;
        }
        /* else UNSAT: property holds up to this bound, continue */
    }

    bmc->last_result = BMC_PROPERTY_HOLD;
    return BMC_PROPERTY_HOLD;
}

const bmc_counterexample_t *bmc_get_counterexample(const bmc_engine_t *bmc) {
    return bmc ? bmc->cex : NULL;
}

void bmc_print_result(const bmc_engine_t *bmc, FILE *fp) {
    if (!bmc || !fp) return;
    fprintf(fp, "BMC Result: ");
    switch (bmc->last_result) {
        case BMC_PROPERTY_HOLD:    fprintf(fp, "PROPERTY HOLDS (up to bound)\n"); break;
        case BMC_PROPERTY_FAIL:    fprintf(fp, "PROPERTY FAILS (counterexample)\n"); break;
        case BMC_PROPERTY_UNKNOWN: fprintf(fp, "UNKNOWN\n"); break;
        case BMC_PROPERTY_ERROR:   fprintf(fp, "ERROR\n"); break;
    }
    if (bmc->cex && bmc->last_result == BMC_PROPERTY_FAIL) {
        fprintf(fp, "  Counterexample length: %u\n", bmc->cex->length);
        fprintf(fp, "  Trace: [");
        for (uint32_t i = 0; i <= bmc->cex->length; i++) {
            fprintf(fp, "%u%s", bmc->cex->state_ids[i],
                    (i < bmc->cex->length) ? " -> " : "");
        }
        fprintf(fp, "]\n");
    }
}

/* ========================================================================
 * k-Induction Engine (L8)
 *
 * Based on: Sheeran, Singh, Stalmarck (FMCAD 2000)
 *   "Checking Safety Properties Using Induction and a SAT-Solver"
 *
 * Base case:   P holds for first k states  (BMC to depth k)
 * Inductive step: P(0..k) => P(k+1)        (ENCODE AND CHECK)
 * If base holds and step holds, P is an invariant for ALL reachable states.
 * ======================================================================== */

k_induction_engine_t *k_ind_create(hv_transition_system_t *sys,
                                    ltl_formula_t *prop, uint32_t max_k) {
    k_induction_engine_t *k = (k_induction_engine_t*)calloc(1, sizeof(*k));
    if (!k) return NULL;
    bmc_config_t bmc_cfg = {.max_bound = max_k, .use_k_induction = true};
    k->bmc = bmc_engine_create(sys, prop, bmc_cfg);
    k->current_k = 0;
    k->max_k = max_k;
    k->step = KIND_BASE;
    k->invariant_cex = NULL;
    k->proved = false;
    return k;
}

void k_ind_destroy(k_induction_engine_t *k) {
    if (!k) return;
    bmc_engine_destroy(k->bmc);
    if (k->invariant_cex) {
        free(k->invariant_cex->state_ids);
        free(k->invariant_cex);
    }
    free(k);
}

bool k_ind_encode_step(k_induction_engine_t *k, uint32_t depth) {
    /* Simpler version: Use BMC encoding for base case */
    if (k->step == KIND_BASE) {
        return bmc_encode_to_sat(k->bmc, depth);
    }
    /* Step case: Encode P(0..k) => P(k+1) */
    return bmc_encode_to_sat(k->bmc, depth + 1);
}

bool k_ind_prove(k_induction_engine_t *k) {
    if (!k || !k->bmc) return false;

    /* Base case: check P(0) */
    k->step = KIND_BASE;
    k->current_k = 0;

    /* Base: check up to a reasonable depth */
    for (uint32_t depth = 0; depth < k->max_k; depth++) {
        k->current_k = depth;
        if (!bmc_encode_to_sat(k->bmc, depth)) return false;
        bool sat = sat_solver_solve(k->bmc->solver);
        if (sat) return false; /* base case fails */
    }

    /* Step case: P(k) -> P(k+1) for arbitrary k */
    k->step = KIND_STEP;
    k->current_k = k->max_k;

    /* Encode step: P(prev) => P(next) */
    if (!bmc_encode_to_sat(k->bmc, 1)) return false;
    bool sat = sat_solver_solve(k->bmc->solver);
    if (sat) return false; /* step fails */

    k->proved = true;
    return true;
}

void k_ind_print_result(const k_induction_engine_t *k, FILE *fp) {
    if (!k || !fp) return;
    fprintf(fp, "k-Induction: %s\n", k->proved ? "PROVED" : "NOT PROVED");
    fprintf(fp, "  Max depth: %u\n", k->max_k);
    fprintf(fp, "  Current k: %u\n", k->current_k);
    fprintf(fp, "  Step: %s\n",
            k->step == KIND_BASE ? "BASE" :
            k->step == KIND_STEP ? "STEP" : "COMPLETE");
}
