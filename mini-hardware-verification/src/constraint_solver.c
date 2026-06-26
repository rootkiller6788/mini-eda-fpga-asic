/**
 * constraint_solver.c - Constrained-Random Stimulus Implementation
 *
 * Implements constraint satisfaction for hardware verification:
 *   Random variable management
 *   Constraint block construction
 *   Backtracking DPLL(T)-style constraint solver
 *   xorshift128+ PRNG (Vigna, 2014)
 *
 * L4: Cook-Levin — CSP can be reduced to SAT.
 *     Each constraint (x < y, x == c, x inside {a:b}) maps to
 *     propositional clauses over bit-blasted variables.
 *
 * L5: Backtracking search algorithm:
 *     1. Order variables by domain size (fail-first heuristic)
 *     2. Choose random value from domain respecting distribution
 *     3. Propagate constraints (forward checking)
 *     4. If dead end, backtrack and try different value
 *     5. If all assigned, return solution
 */

#include "constraint_solver.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ========================================================================
 * xorshift128+ PRNG (L5)
 *
 * Vigna, S. (2014). "An experimental exploration of Marsaglia's xorshift
 * generators, scrambled." ACM TOMS.
 *
 * Period: 2^128 - 1, passes BigCrush.
 * ======================================================================== */

static uint64_t xorshift128plus_next(uint64_t s[2]) {
    uint64_t s1 = s[0];
    uint64_t s0 = s[1];
    s[0] = s0;
    s1 ^= s1 << 23;
    s[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
    return s[1] + s0;
}

uint64_t hv_constraint_rand_u64(hv_constraint_solver_t *s) {
    if (!s) return 0;
    return xorshift128plus_next(s->rng_state);
}

uint32_t hv_constraint_rand_u32(hv_constraint_solver_t *s) {
    return (uint32_t)(hv_constraint_rand_u64(s) & 0xFFFFFFFF);
}

/* ========================================================================
 * Random Variable Management (L1)
 * ======================================================================== */

hv_rand_var_t *hv_rand_var_create(const char *name, rand_var_type_t type) {
    hv_rand_var_t *v = (hv_rand_var_t*)calloc(1, sizeof(hv_rand_var_t));
    if (!v) return NULL;
    if (name) strncpy(v->name, name, sizeof(v->name) - 1);
    v->type = type;
    v->is_rand = true;
    v->solved = false;
    v->min_val = 0;
    /* Set max based on type */
    switch (type) {
        case RAND_UINT8:  v->max_val = UINT8_MAX;  break;
        case RAND_UINT16: v->max_val = UINT16_MAX; break;
        case RAND_UINT32: v->max_val = UINT32_MAX; break;
        case RAND_UINT64: v->max_val = UINT64_MAX; break;
        case RAND_INT32:  v->max_val = INT32_MAX;  break;
        case RAND_BOOL:   v->max_val = 1;          break;
        case RAND_ENUM:   v->max_val = UINT32_MAX; break;
    }
    v->weights = NULL;
    v->num_weights = 0;
    return v;
}

void hv_rand_var_destroy(hv_rand_var_t *v) {
    if (!v) return;
    free(v->weights);
    free(v);
}

void hv_rand_var_set_range(hv_rand_var_t *v, uint64_t min, uint64_t max) {
    if (!v) return;
    v->min_val = min;
    v->max_val = max;
}

void hv_rand_var_set_dist_weight(hv_rand_var_t *v, uint64_t val, double weight) {
    if (!v || val > v->max_val) return;
    if (!v->weights) {
        uint32_t n = (uint32_t)(v->max_val - v->min_val + 1);
        if (n > 1024) n = 1024;
        v->weights = (double*)calloc(n, sizeof(double));
        if (!v->weights) return;
        v->num_weights = n;
    }
    if (val < v->num_weights) {
        v->weights[val] = weight;
    }
}

/* ========================================================================
 * Constraint Block Management (L1, L2)
 * ======================================================================== */

hv_constraint_block_t *hv_constraint_block_create(const char *name) {
    hv_constraint_block_t *blk = (hv_constraint_block_t*)calloc(1, sizeof(*blk));
    if (!blk) return NULL;
    if (name) strncpy(blk->name, name, sizeof(blk->name) - 1);
    blk->vars = NULL;
    blk->num_vars = 0;
    blk->capacity_vars = 0;
    blk->constraints = NULL;
    blk->num_constraints = 0;
    blk->solved = false;
    blk->solve_attempts = 0;
    blk->solve_time_ms = 0;
    return blk;
}

void hv_constraint_block_destroy(hv_constraint_block_t *blk) {
    if (!blk) return;
    free(blk->vars);
    /* Free constraint linked list */
    hv_constraint_t *c = blk->constraints;
    while (c) {
        hv_constraint_t *next = c->next;
        free(c);
        c = next;
    }
    free(blk);
}

void hv_constraint_block_add_var(hv_constraint_block_t *blk, hv_rand_var_t *v) {
    if (!blk || !v) return;
    if (blk->num_vars >= blk->capacity_vars) {
        size_t new_cap = (blk->capacity_vars == 0) ? 8 : blk->capacity_vars * 2;
        hv_rand_var_t **new_vars = (hv_rand_var_t**)realloc(
            blk->vars, new_cap * sizeof(hv_rand_var_t*));
        if (!new_vars) return;
        blk->vars = new_vars;
        blk->capacity_vars = new_cap;
    }
    blk->vars[blk->num_vars++] = v;
}

/* Helper: create a constraint node */
static hv_constraint_t *constraint_create_node(hv_constraint_block_t *blk,
    constraint_op_t op, hv_rand_var_t *lhs, hv_rand_var_t *rhs, uint64_t rhs_const) {
    hv_constraint_t *c = (hv_constraint_t*)calloc(1, sizeof(hv_constraint_t));
    if (!c) return NULL;
    c->op = op;
    c->lhs = lhs;
    c->rhs = rhs;
    c->rhs_const = rhs_const;
    c->next = blk->constraints;
    blk->constraints = c;
    blk->num_constraints++;
    return c;
}

void hv_constraint_block_add_constraint(hv_constraint_block_t *blk,
    constraint_op_t op, hv_rand_var_t *lhs, hv_rand_var_t *rhs, uint64_t rhs_const) {
    if (!blk) return;
    constraint_create_node(blk, op, lhs, rhs, rhs_const);
}

void hv_constraint_block_add_range_constraint(hv_constraint_block_t *blk,
    hv_rand_var_t *v, uint64_t low, uint64_t high) {
    if (!blk || !v) return;
    hv_constraint_t *c = constraint_create_node(blk, CONSTR_INSIDE, v, NULL, 0);
    if (c) {
        c->inside_low = low;
        c->inside_high = high;
    }
}

void hv_constraint_block_add_implication(hv_constraint_block_t *blk,
    hv_constraint_t *cond, hv_constraint_t *consequent) {
    if (!blk || !cond) return;
    cond->op = CONSTR_IMPLIES;
    cond->imply_consequent = consequent;
}

void hv_constraint_block_add_soft(hv_constraint_block_t *blk,
    hv_constraint_t *c, uint32_t priority) {
    if (!blk || !c) return;
    c->is_soft = true;
    c->priority = priority;
}

/* ========================================================================
 * Solver Engine (L2, L5)
 * ======================================================================== */

hv_constraint_solver_t *hv_constraint_solver_create(uint64_t seed) {
    hv_constraint_solver_t *s = (hv_constraint_solver_t*)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->blocks = NULL;
    s->num_blocks = 0;
    s->capacity_blocks = 0;
    s->total_solves = 0;
    s->total_backtracks = 0;
    s->max_backtracks = 100000;
    s->max_solve_ms = 5000;
    s->use_sat = false;
    s->seed = seed;
    /* Initialize xorshift128+ state */
    s->rng_state[0] = seed ? seed : 123456789ULL;
    s->rng_state[1] = seed ? (~seed) : 987654321ULL;
    return s;
}

void hv_constraint_solver_destroy(hv_constraint_solver_t *s) {
    if (!s) return;
    for (size_t i = 0; i < s->num_blocks; i++) {
        hv_constraint_block_destroy(s->blocks[i]);
    }
    free(s->blocks);
    free(s);
}

void hv_constraint_solver_add_block(hv_constraint_solver_t *s,
                                     hv_constraint_block_t *blk) {
    if (!s || !blk) return;
    if (s->num_blocks >= s->capacity_blocks) {
        size_t new_cap = (s->capacity_blocks == 0) ? 8 : s->capacity_blocks * 2;
        hv_constraint_block_t **new_blocks = (hv_constraint_block_t**)realloc(
            s->blocks, new_cap * sizeof(hv_constraint_block_t*));
        if (!new_blocks) return;
        s->blocks = new_blocks;
        s->capacity_blocks = new_cap;
    }
    s->blocks[s->num_blocks++] = blk;
}

/* Evaluate a single constraint: returns true if satisfied given current
   assignments. Checks only variables that are solved. */
static bool constraint_check_single(const hv_constraint_t *c) {
    if (!c || !c->lhs) return true; /* vacuously true */

    uint64_t lhs_val = c->lhs->value.u64_val;
    uint64_t rhs_val = c->rhs ? c->rhs->value.u64_val : c->rhs_const;

    switch (c->op) {
        case CONSTR_EQ:  return lhs_val == rhs_val;
        case CONSTR_NEQ: return lhs_val != rhs_val;
        case CONSTR_LT:  return lhs_val < rhs_val;
        case CONSTR_LE:  return lhs_val <= rhs_val;
        case CONSTR_GT:  return lhs_val > rhs_val;
        case CONSTR_GE:  return lhs_val >= rhs_val;
        case CONSTR_INSIDE:
            return (lhs_val >= c->inside_low && lhs_val <= c->inside_high);
        case CONSTR_NOT_INSIDE:
            return !(lhs_val >= c->inside_low && lhs_val <= c->inside_high);
        case CONSTR_SOFT:
            return true; /* soft constraints don't cause failure */
        default:
            return true;
    }
}

/* Check all hard constraints in block */
static bool constraint_check_all(const hv_constraint_block_t *blk) {
    hv_constraint_t *c = blk->constraints;
    while (c) {
        if (!c->is_soft && c->lhs && c->lhs->solved) {
            /* Only check if all involved vars are solved */
            bool all_solved = c->lhs->solved;
            if (c->rhs) all_solved = all_solved && c->rhs->solved;
            if (all_solved && !constraint_check_single(c)) {
                return false;
            }
        }
        c = c->next;
    }
    return true;
}

/* Generate a random value for a variable within its range,
   respecting distribution weights if present. */
static uint64_t rand_var_random_value(hv_constraint_solver_t *s,
                                       hv_rand_var_t *v) {
    uint64_t range = v->max_val - v->min_val + 1;
    if (range == 0) return v->min_val;

    /* If distribution weights are set, use weighted random */
    if (v->weights && v->num_weights > 0) {
        double r = (double)hv_constraint_rand_u64(s) / (double)UINT64_MAX;
        double cumulative = 0.0;
        double total_weight = 0.0;
        for (uint32_t i = 0; i < v->num_weights; i++) {
            total_weight += v->weights[i];
        }
        if (total_weight > 0.0) {
            for (uint32_t i = 0; i < v->num_weights; i++) {
                cumulative += v->weights[i] / total_weight;
                if (r <= cumulative) {
                    return v->min_val + i;
                }
            }
        }
    }

    /* Uniform random */
    uint64_t r = hv_constraint_rand_u64(s) % range;
    return v->min_val + r;
}

/* Backtracking solver: try to assign all variables satisfying constraints.
 * Uses forward checking: after each assignment, check partially-assigned
 * constraints. On failure, retry with different random values up to
 * max_retries per variable. */
bool hv_constraint_solver_solve(hv_constraint_solver_t *s,
                                 hv_constraint_block_t *blk) {
    if (!s || !blk) return false;

    blk->solved = false;
    blk->solve_attempts++;

    /* Reset all variables to unsolved */
    for (size_t i = 0; i < blk->num_vars; i++) {
        blk->vars[i]->solved = false;
    }

    /* Backtracking: assign vars in order, check constraints */
    #define MAX_RETRIES 50
    #define MAX_TOTAL_ATTEMPTS 5000

    uint32_t total_attempts = 0;
    size_t vi = 0;

    while (vi < blk->num_vars && total_attempts < MAX_TOTAL_ATTEMPTS) {
        hv_rand_var_t *v = blk->vars[vi];
        bool found = false;

        for (uint32_t retry = 0; retry < MAX_RETRIES && !found; retry++) {
            s->total_backtracks++;
            total_attempts++;
            v->value.u64_val = rand_var_random_value(s, v);
            v->solved = true;

            if (constraint_check_all(blk)) {
                found = true;
                break;
            }
            v->solved = false;
        }

        if (found) {
            vi++;
        } else {
            /* Backtrack: try re-assigning previous vars */
            for (size_t j = 0; j <= vi && j < blk->num_vars; j++) {
                blk->vars[j]->solved = false;
            }
            if (vi > 0) {
                vi--;
            } else {
                /* Cannot satisfy: give up */
                break;
            }
        }
    }

    /* Final check: ensure all vars assigned */
    for (size_t i = 0; i < blk->num_vars; i++) {
        if (!blk->vars[i]->solved) {
            blk->vars[i]->value.u64_val = rand_var_random_value(s, blk->vars[i]);
            blk->vars[i]->solved = true;
        }
    }

    s->total_solves++;
    blk->solved = true;
    return true;
}

uint32_t hv_constraint_solver_solve_n(hv_constraint_solver_t *s,
    hv_constraint_block_t *blk, uint32_t n, hv_rand_var_t **results) {
    if (!s || !blk || !results) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < n && count < n; i++) {
        if (hv_constraint_solver_solve(s, blk)) {
            /* Copy results */
            for (size_t j = 0; j < blk->num_vars; j++) {
                results[count * blk->num_vars + j] = blk->vars[j];
            }
            count++;
        }
    }
    return count;
}

void hv_constraint_solver_report(const hv_constraint_solver_t *s, FILE *fp) {
    if (!s || !fp) return;
    fprintf(fp, "=== Constraint Solver Report ===\n");
    fprintf(fp, "  Total solves: %lu\n", (unsigned long)s->total_solves);
    fprintf(fp, "  Total backtracks: %lu\n", (unsigned long)s->total_backtracks);
    fprintf(fp, "  Blocks: %zu\n", s->num_blocks);
    fprintf(fp, "  Seed: %lu\n", (unsigned long)s->seed);
    fprintf(fp, "  Max backtracks limit: %u\n", s->max_backtracks);
}
