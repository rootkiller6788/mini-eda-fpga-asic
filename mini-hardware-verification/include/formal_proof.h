/**
 * formal_proof.h - Formal Verification Engine
 *
 * Implements:
 *   Bounded Model Checking (BMC) per Biere et al. (1999) / Clarke et al.
 *   SAT-based property checking via DPLL/CDCL integration
 *   k-induction for unbounded proofs (Sheeran et al., FMCAD 2000)
 *   Temporal logic: LTL and CTL model checking
 *
 * L1: struct definitions for BMC, k-induction, property specification
 * L2: SAT solving, temporal logic
 * L3: BMC unrolling engine
 * L4: Church-Turing thesis, SAT/NP-completeness (Cook-Levin theorem)
 * L5: DPLL/CDCL SAT algorithm, BMC unrolling algorithm
 * L8: k-induction for completeness
 *
 * Course: CMU 15-414 (Bug Catching), UT ECE 382V, MIT 6.375 (Formal Methods)
 */

#ifndef FORMAL_PROOF_H
#define FORMAL_PROOF_H

#include "hw_verify.h"
#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 * L1: Core Type Definitions
 * ======================================================================== */

/* --- SAT literal: variable index + sign --- */
typedef struct {
    int32_t var;       /* 1-indexed variable; 0 = unused */
    bool    sign;      /* false = positive, true = negated */
} sat_literal_t;

/* --- SAT clause: disjunction of literals --- */
#define MAX_CLAUSE_LITS 256
typedef struct {
    sat_literal_t lits[MAX_CLAUSE_LITS];
    uint32_t      num_lits;
    bool          is_learned;    /* CDCL learned clause */
    uint32_t      activity;      /* VSIDS heuristic score */
} sat_clause_t;

/* --- SAT variable assignment --- */
typedef enum {
    SAT_UNASSIGNED = 0,
    SAT_TRUE       = 1,
    SAT_FALSE      = 2,
} sat_assign_t;

/* --- SAT solver statistics --- */
typedef struct {
    uint64_t decisions;
    uint64_t propagations;
    uint64_t conflicts;
    uint64_t restarts;
    uint64_t learned_clauses;
    double   solve_time_ms;
} sat_stats_t;

/* --- SAT Solver instance (CDCL: Conflict-Driven Clause Learning) --- */
typedef struct sat_solver {
    uint32_t      num_vars;
    uint32_t      num_clauses;
    uint32_t      capacity_clauses;
    sat_clause_t *clauses;
    sat_assign_t *assignments;    /* indexed by var */
    uint32_t     *decision_level; /* when each var was assigned */
    uint32_t      current_level;
    uint32_t     *reason;         /* clause index that implied this var */
    sat_clause_t  conflict_clause;
    bool          has_conflict;
    sat_stats_t   stats;
} sat_solver_t;

/* ========================================================================
 * L1: Temporal Logic Types (LTL & CTL)
 * ======================================================================== */

/* LTL operators */
typedef enum {
    LTL_TRUE    = 0,
    LTL_ATOM    = 1,   /* atomic proposition */
    LTL_NOT     = 2,
    LTL_AND     = 3,
    LTL_OR      = 4,
    LTL_IMPLIES = 5,
    LTL_NEXT    = 6,   /* X phi */
    LTL_GLOBALLY = 7,  /* G phi */
    LTL_FINALLY  = 8,  /* F phi */
    LTL_UNTIL    = 9,  /* phi U psi */
    LTL_RELEASE  = 10, /* phi R psi */
} ltl_op_t;

/* LTL formula represented as a syntax tree */
typedef struct ltl_formula {
    ltl_op_t          op;
    int32_t           atom_id;      /* for LTL_ATOM */
    struct ltl_formula *left;
    struct ltl_formula *right;
} ltl_formula_t;

/* CTL path quantifiers */
typedef enum {
    CTL_EX = 0,  /* exists a path */
    CTL_AX = 1,  /* for all paths */
    CTL_EF = 2,
    CTL_AF = 3,
    CTL_EG = 4,
    CTL_AG = 5,
    CTL_EU = 6,
    CTL_AU = 7,
} ctl_op_t;

/* ========================================================================
 * L1: Transition System / Kripke Structure
 * ======================================================================== */

typedef struct hv_state {
    uint32_t      id;
    bool         *props;         /* atomic propositions, indexed by prop_id */
    uint32_t      num_props;
    char          label[64];
} hv_state_t;

typedef struct hv_transition {
    uint32_t      from_state;
    uint32_t      to_state;
} hv_transition_t;

typedef struct hv_transition_system {
    hv_state_t       *states;
    uint32_t          num_states;
    uint32_t          capacity_states;
    hv_transition_t  *transitions;
    uint32_t          num_transitions;
    uint32_t          capacity_transitions;
    uint32_t          initial_state;
    char             *prop_names[64];
    uint32_t          num_props;
} hv_transition_system_t;

/* ========================================================================
 * L2 & L3: Bounded Model Checking (BMC) Engine
 * ======================================================================== */

typedef enum {
    BMC_PROPERTY_HOLD   = 0,
    BMC_PROPERTY_FAIL   = 1,   /* counterexample found */
    BMC_PROPERTY_UNKNOWN = 2,  /* bound too small */
    BMC_PROPERTY_ERROR  = 3,
} bmc_result_t;

/* BMC counterexample trace */
typedef struct bmc_counterexample {
    uint32_t  *state_ids;    /* sequence of state IDs */
    uint32_t   length;
    char       description[512];
} bmc_counterexample_t;

/* BMC engine configuration */
typedef struct bmc_config {
    uint32_t    max_bound;       /* max unrolling depth k */
    uint32_t    timeout_ms;
    bool        use_incremental; /* incremental SAT solving */
    bool        use_k_induction;
    bool        verbose;
} bmc_config_t;

/* BMC engine */
typedef struct bmc_engine {
    hv_transition_system_t *system;
    ltl_formula_t          *property;
    bmc_config_t            config;
    sat_solver_t           *solver;
    bmc_result_t            last_result;
    bmc_counterexample_t   *cex;
    uint64_t                total_checks;
    uint64_t                sat_calls;
    double                  total_time_ms;
} bmc_engine_t;

/* ========================================================================
 * L8: k-Induction Engine (Sheeran et al., FMCAD 2000)
 * ======================================================================== */

typedef enum {
    KIND_BASE    = 0,  /* base case checking: P(0) */
    KIND_STEP    = 1,  /* step case checking: P(k) -> P(k+1) */
    KIND_COMPLETE = 2,
} k_ind_step_t;

typedef struct k_induction_engine {
    bmc_engine_t       *bmc;
    uint32_t             current_k;
    uint32_t             max_k;
    k_ind_step_t         step;
    bmc_counterexample_t *invariant_cex;
    bool                 proved;
} k_induction_engine_t;

/* ========================================================================
 * L2: API - SAT Solver
 * ======================================================================== */

sat_solver_t    *sat_solver_create(uint32_t num_vars, uint32_t capacity_clauses);
void             sat_solver_destroy(sat_solver_t *s);
int32_t          sat_solver_new_var(sat_solver_t *s);
void             sat_solver_add_clause(sat_solver_t *s, sat_literal_t *lits,
                                        uint32_t n);
bool             sat_solver_solve(sat_solver_t *s); /* true = SAT */
sat_assign_t     sat_solver_get_value(sat_solver_t *s, int32_t var);
void             sat_solver_reset(sat_solver_t *s);
void             sat_solver_print_stats(const sat_solver_t *s, FILE *fp);

/* ========================================================================
 * L2: API - LTL / CTL Formula
 * ======================================================================== */

ltl_formula_t   *ltl_atom(int32_t atom_id);
ltl_formula_t   *ltl_not(ltl_formula_t *phi);
ltl_formula_t   *ltl_and(ltl_formula_t *a, ltl_formula_t *b);
ltl_formula_t   *ltl_or(ltl_formula_t *a, ltl_formula_t *b);
ltl_formula_t   *ltl_implies(ltl_formula_t *a, ltl_formula_t *b);
ltl_formula_t   *ltl_next(ltl_formula_t *phi);
ltl_formula_t   *ltl_globally(ltl_formula_t *phi);
ltl_formula_t   *ltl_finally(ltl_formula_t *phi);
ltl_formula_t   *ltl_until(ltl_formula_t *a, ltl_formula_t *b);
ltl_formula_t   *ltl_release(ltl_formula_t *a, ltl_formula_t *b);
void             ltl_formula_destroy(ltl_formula_t *f);
void             ltl_formula_print(const ltl_formula_t *f, FILE *fp);

/* ========================================================================
 * L2 & L3: API - Transition System
 * ======================================================================== */

hv_transition_system_t *hv_ts_create(uint32_t num_props);
void             hv_ts_destroy(hv_transition_system_t *ts);
uint32_t         hv_ts_add_state(hv_transition_system_t *ts, const char *label);
void             hv_ts_set_prop(hv_transition_system_t *ts, uint32_t state_id,
                                 uint32_t prop_id, bool value);
void             hv_ts_add_transition(hv_transition_system_t *ts,
                                       uint32_t from, uint32_t to);
void             hv_ts_set_initial(hv_transition_system_t *ts, uint32_t state);
const hv_state_t *hv_ts_get_state(const hv_transition_system_t *ts, uint32_t id);
void             hv_ts_print(const hv_transition_system_t *ts, FILE *fp);

/* ========================================================================
 * L3 & L5: API - BMC Engine
 * ======================================================================== */

bmc_engine_t    *bmc_engine_create(hv_transition_system_t *sys,
                                    ltl_formula_t *prop,
                                    bmc_config_t config);
void             bmc_engine_destroy(bmc_engine_t *bmc);
bmc_result_t     bmc_check(bmc_engine_t *bmc);
const bmc_counterexample_t *bmc_get_counterexample(const bmc_engine_t *bmc);
void             bmc_print_result(const bmc_engine_t *bmc, FILE *fp);

/* ========================================================================
 * L8: API - k-Induction
 * ======================================================================== */

k_induction_engine_t *k_ind_create(hv_transition_system_t *sys,
                                    ltl_formula_t *prop,
                                    uint32_t max_k);
void             k_ind_destroy(k_induction_engine_t *k);
bool             k_ind_prove(k_induction_engine_t *k);
void             k_ind_print_result(const k_induction_engine_t *k, FILE *fp);

/* ========================================================================
 * L4: Cook-Levin Theorem - Encode BMC problem as SAT
 * See formal_proof.c for the reduction implementation
 * ======================================================================== */

/* Encode a k-step unrolling of the transition system with property P
 * into a SAT instance. Returns true if encoding succeeded. */
bool             bmc_encode_to_sat(bmc_engine_t *bmc, uint32_t k);

/* Encode a k-induction step as SAT: base or step */
bool             k_ind_encode_step(k_induction_engine_t *k, uint32_t depth);

#endif /* FORMAL_PROOF_H */
