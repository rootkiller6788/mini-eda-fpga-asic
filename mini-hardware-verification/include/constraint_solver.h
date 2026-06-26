/**
 * constraint_solver.h - Constrained-Random Stimulus Generation
 *
 * Implements SystemVerilog-alike constraint solver:
 *   Constraint expressions (relational, distribution, implication)
 *   Randomization with constraint solving via backtracking + SAT
 *   Soft constraints (prefer but not require)
 *   Inline constraints (randomize() with {})
 *
 * L1: struct definitions for constraint, random variable
 * L2: constraint satisfaction problem (CSP) formalism
 * L4: Cook-Levin (CSP-sAT reduction), Backtracking completeness
 * L5: DPLL(T) / CDCL-based constraint solving
 * L7: AXI bus transaction generation with constraints
 *
 * Course: CMU 15-414 (SAT/SMT), UT ECE 382V (Constrained Random)
 * Ref: Yuan et al., "Constraint-Based Verification" (Springer, 2006)
 */

#ifndef CONSTRAINT_SOLVER_H
#define CONSTRAINT_SOLVER_H

#include "hw_verify.h"
#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 * L1: Random Variable
 * ======================================================================== */

typedef enum {
    RAND_UINT8   = 0,
    RAND_UINT16  = 1,
    RAND_UINT32  = 2,
    RAND_UINT64  = 3,
    RAND_INT32   = 4,
    RAND_BOOL    = 5,
    RAND_ENUM    = 6,
} rand_var_type_t;

typedef struct hv_rand_var {
    char              name[64];
    rand_var_type_t   type;
    union {
        uint8_t       u8_val;
        uint16_t      u16_val;
        uint32_t      u32_val;
        uint64_t      u64_val;
        int32_t       i32_val;
        bool          bool_val;
    } value;
    uint64_t          min_val;   /* range min */
    uint64_t          max_val;   /* range max */
    bool              is_rand;   /* rand vs randc */
    bool              solved;
    /* distribution weighting */
    double           *weights;   /* per-value weight (or NULL for uniform) */
    uint32_t          num_weights;
} hv_rand_var_t;

/* ========================================================================
 * L1: Constraint Expression Types
 * ======================================================================== */

typedef enum {
    CONSTR_EQ,           /* a == b */
    CONSTR_NEQ,          /* a != b */
    CONSTR_LT,           /* a < b */
    CONSTR_LE,           /* a <= b */
    CONSTR_GT,           /* a > b */
    CONSTR_GE,           /* a >= b */
    CONSTR_INSIDE,       /* a inside {min:max} */
    CONSTR_NOT_INSIDE,   /* !(a inside {min:max}) */
    CONSTR_DIST,         /* distribution constraint */
    CONSTR_IMPLIES,      /* a implies b */
    CONSTR_IF_ELSE,      /* if (cond) then a else b */
    CONSTR_UNIQUE,       /* all values distinct */
    CONSTR_BOOL_EXPR,    /* boolean expression tree */
    CONSTR_SOFT,         /* soft constraint (preferred) */
} constraint_op_t;

/* Forward declaration */
typedef struct hv_constraint hv_constraint_t;

/* Boolean expression node */
typedef struct hv_bool_expr {
    enum {
        BOOL_VAR,        /* reference to rand_var */
        BOOL_NOT,
        BOOL_AND,
        BOOL_OR,
        BOOL_XOR,
        BOOL_CONST,      /* constant true/false */
    } op;
    hv_rand_var_t   *var;
    bool              const_val;
    struct hv_bool_expr *left;
    struct hv_bool_expr *right;
} hv_bool_expr_t;

/* Constraint */
struct hv_constraint {
    constraint_op_t   op;
    hv_rand_var_t    *lhs;
    hv_rand_var_t    *rhs;
    uint64_t          rhs_const;
    uint64_t          inside_low;
    uint64_t          inside_high;
    hv_bool_expr_t   *bool_expr;        /* for CONSTR_BOOL_EXPR */
    hv_constraint_t  *imply_consequent; /* for CONSTR_IMPLIES */
    hv_constraint_t  *if_true;          /* for CONSTR_IF_ELSE */
    hv_constraint_t  *if_false;
    bool              is_soft;          /* soft constraint */
    uint32_t          priority;         /* lower = higher priority */
    struct hv_constraint *next;
};

/* Constraint block (collection of constraints on a set of variables) */
typedef struct hv_constraint_block {
    char              name[64];
    hv_rand_var_t   **vars;
    size_t            num_vars;
    size_t            capacity_vars;
    hv_constraint_t  *constraints;
    size_t            num_constraints;
    bool              solved;
    uint64_t          solve_attempts;
    double            solve_time_ms;
} hv_constraint_block_t;

/* ========================================================================
 * L2: Constraint Solver Engine (DPLL(T)-style)
 * ======================================================================== */

typedef struct hv_constraint_solver {
    hv_constraint_block_t **blocks;
    size_t                  num_blocks;
    size_t                  capacity_blocks;
    uint64_t                total_solves;
    uint64_t                total_backtracks;
    /* solver parameters */
    uint32_t                max_backtracks;  /* 0 = unlimited */
    uint32_t                max_solve_ms;    /* timeout per solve */
    bool                    use_sat;         /* use SAT backend for bool exprs */
    uint64_t                seed;            /* RNG seed */
    /* RNG state (xorshift128+) */
    uint64_t                rng_state[2];
} hv_constraint_solver_t;

/* ========================================================================
 * L1 & L2: API - Random Variable
 * ======================================================================== */

hv_rand_var_t   *hv_rand_var_create(const char *name, rand_var_type_t type);
void             hv_rand_var_destroy(hv_rand_var_t *v);
void             hv_rand_var_set_range(hv_rand_var_t *v, uint64_t min, uint64_t max);
void             hv_rand_var_set_dist_weight(hv_rand_var_t *v,
                     uint64_t val, double weight);

/* ========================================================================
 * L1 & L2: API - Constraint Block
 * ======================================================================== */

hv_constraint_block_t *hv_constraint_block_create(const char *name);
void             hv_constraint_block_destroy(hv_constraint_block_t *blk);
void             hv_constraint_block_add_var(hv_constraint_block_t *blk,
                                              hv_rand_var_t *v);
void             hv_constraint_block_add_constraint(hv_constraint_block_t *blk,
                     constraint_op_t op, hv_rand_var_t *lhs,
                     hv_rand_var_t *rhs, uint64_t rhs_const);
void             hv_constraint_block_add_range_constraint(
                     hv_constraint_block_t *blk, hv_rand_var_t *v,
                     uint64_t low, uint64_t high);
void             hv_constraint_block_add_implication(hv_constraint_block_t *blk,
                     hv_constraint_t *cond, hv_constraint_t *consequent);
void             hv_constraint_block_add_soft(hv_constraint_block_t *blk,
                     hv_constraint_t *c, uint32_t priority);

/* ========================================================================
 * L5: API - Solver
 * ======================================================================== */

hv_constraint_solver_t *hv_constraint_solver_create(uint64_t seed);
void             hv_constraint_solver_destroy(hv_constraint_solver_t *s);
void             hv_constraint_solver_add_block(hv_constraint_solver_t *s,
                                                 hv_constraint_block_t *blk);
/* Solve: find a random assignment satisfying all constraints.
 * Returns true on success. Implements backtracking search + SAT fallback. */
bool             hv_constraint_solver_solve(hv_constraint_solver_t *s,
                                             hv_constraint_block_t *blk);
/* Generate N distinct solutions (for randc semantics) */
uint32_t         hv_constraint_solver_solve_n(hv_constraint_solver_t *s,
                     hv_constraint_block_t *blk, uint32_t n,
                     hv_rand_var_t **results);
/* RNG: xorshift128+ (Vigna, 2014) */
uint64_t         hv_constraint_rand_u64(hv_constraint_solver_t *s);
uint32_t         hv_constraint_rand_u32(hv_constraint_solver_t *s);
/* Report */
void             hv_constraint_solver_report(const hv_constraint_solver_t *s,
                                              FILE *fp);

#endif /* CONSTRAINT_SOLVER_H */
