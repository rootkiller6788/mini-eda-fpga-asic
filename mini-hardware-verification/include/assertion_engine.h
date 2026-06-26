/**
 * assertion_engine.h - SystemVerilog Assertions (SVA) Engine
 *
 * Implements IEEE 1800-2017 concurrent assertions subset:
 *   Immediate assertions (assert/assume/cover)
 *   Concurrent assertions over clock edges
 *   Sequences (temporal expressions)
 *   Properties (sequences + operators)
 *   Implication (|->, |=>)
 *   Repetition operators (# #, #-#, [*], [+], [=], [->])
 *
 * L1: assertion/assume/cover struct definitions
 * L2: temporal sequence evaluation on simulation trace
 * L3: assertion engine integrated with simulation kernel
 * L5: sequence evaluation algorithm (dynamic programming trace matching)
 * L8: metric-based assertion coverage (assertion density)
 *
 * Course: UT ECE 382V (Assertion-Based Verification)
 * Ref: Foster et al., "Assertion-Based Design" (Kluwer, 2004)
 */

#ifndef ASSERTION_ENGINE_H
#define ASSERTION_ENGINE_H

#include "hw_verify.h"
#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 * L1: Assertion Types
 * ======================================================================== */

typedef enum {
    ASSERT_IMMEDIATE,    /* assert(expr); - checked immediately */
    ASSERT_CONCURRENT,   /* assert property(...); - clock-synchronized */
    ASSUME,              /* assume property(...) - constrains formal */
    COVER_SEQUENCE,      /* cover property(...) - coverage */
    RESTRICT,            /* restrict property(...) - formal constraint */
} assertion_type_t;

/* ========================================================================
 * L1: Sequence Operators
 * ======================================================================== */

typedef enum {
    SEQ_ATOM,            /* boolean expression (signal comparison) */
    SEQ_DELAY,           /* ##n (fixed delay) */
    SEQ_RANGE_DELAY,     /* ##[m:n] (range delay) */
    SEQ_CONSECUTIVE_REP, /* [*n], [*m:n] consecutive repetition */
    SEQ_GOTO_REP,        /* [->n], [->m:n] goto repetition */
    SEQ_NON_CONSEC_REP,  /* [=n], [=m:n] non-consecutive repetition */
    SEQ_AND,             /* a and b */
    SEQ_OR,              /* a or b */
    SEQ_INTERSECT,       /* a intersect b (same-length and) */
    SEQ_THROUGHOUT,      /* expr throughout seq */
    SEQ_WITHIN,          /* a within b */
    SEQ_FIRST_MATCH,     /* first_match(a) */
    SEQ_END,             /* end point marker */
} seq_op_t;

/* Forward declaration */
typedef struct hv_sequence_expr hv_sequence_expr_t;

/* Boolean expression operand for sequence (compares signals) */
typedef enum {
    CMP_EQ,              /* sig == const */
    CMP_NEQ,             /* sig != const */
    CMP_LT,
    CMP_LE,
    CMP_GT,
    CMP_GE,
    CMP_BIT_AND,         /* (sig & const) != 0 */
    CMP_BIT_EQ,          /* (sig & const) == const */
} cmp_op_t;

typedef struct hv_assert_operand {
    hv_signal_t    *signal;
    cmp_op_t        cmp_op;
    uint32_t        compare_value;
    /* or reference another signal */
    hv_signal_t    *signal2;
    bool            is_sig_to_sig;
} hv_assert_operand_t;

/* Sequence expression tree node */
struct hv_sequence_expr {
    seq_op_t             op;
    /* atom */
    hv_assert_operand_t  operand;
    /* delay parameters */
    uint32_t             delay_min;
    uint32_t             delay_max;
    /* repetition parameters */
    uint32_t             rep_min;
    uint32_t             rep_max;
    /* children */
    hv_sequence_expr_t  *left;
    hv_sequence_expr_t  *right;
    /* evaluation state (for runtime matching) */
    uint32_t             match_count;
    bool                 is_matched;
};

/* ========================================================================
 * L1: Property Operators
 * ======================================================================== */

typedef enum {
    PROP_SEQ,            /* sequence as property */
    PROP_NOT,            /* not property */
    PROP_AND,
    PROP_OR,
    PROP_IMPLIES_OVERLAP,  /* sequence |-> property (overlapping) */
    PROP_IMPLIES_NONOVERLAP,/* sequence |=> property (non-overlapping) */
    PROP_DISABLE_IFF,    /* disable iff (expr) property */
    PROP_STRONG,         /* strong(sequence) - must match */
    PROP_WEAK,           /* weak(sequence) - may not match if sim ends */
} property_op_t;

typedef struct hv_property_expr {
    property_op_t      op;
    hv_sequence_expr_t *seq;         /* for PROP_SEQ, implication antecedent */
    struct hv_property_expr *prop;   /* for implication consequent */
    struct hv_property_expr *left;
    struct hv_property_expr *right;
    hv_assert_operand_t disable_cond; /* for PROP_DISABLE_IFF */
} hv_property_expr_t;

/* ========================================================================
 * L1 & L2: Assertion Instance
 * ======================================================================== */

typedef struct hv_assertion {
    char              name[128];
    assertion_type_t  type;
    hv_property_expr_t *property;
    hv_signal_t       *clock;          /* sampling clock */
    severity_t        severity;
    char              message[256];
    /* statistics */
    uint64_t          attempt_count;   /* total clock ticks checked */
    uint64_t          pass_count;
    uint64_t          fail_count;
    uint64_t          vacuous_pass;    /* antecedent never matched */
    bool              is_active;
    bool              is_vacuous;
    /* callback on fail */
    void            (*on_fail)(struct hv_assertion *a, sim_time_t time);
    /* failure details */
    sim_time_t        last_fail_time;
    uint32_t          last_fail_cycle;
    char              fail_detail[512];
} hv_assertion_t;

/* ========================================================================
 * L2: Assertion Engine (manages all assertions in a testbench)
 * ======================================================================== */

typedef struct hv_assertion_engine {
    hv_assertion_t  **assertions;
    size_t            num_assertions;
    size_t            capacity_assertions;
    /* per-bind DUT binding */
    hv_dut_t         *dut;
    /* statistics */
    uint64_t          total_attempts;
    uint64_t          total_passes;
    uint64_t          total_failures;
} hv_assertion_engine_t;

/* ========================================================================
 * L5: Sequence match state (for runtime evaluation)
 * ======================================================================== */

typedef struct hv_seq_match_state {
    hv_sequence_expr_t *seq;
    uint32_t            start_cycle;
    uint32_t            current_cycle;
    uint32_t            consecutive_count;
    bool                active;
    struct hv_seq_match_state *next;
} hv_seq_match_state_t;

/* ========================================================================
 * L1: API - Operand construction
 * ======================================================================== */

hv_assert_operand_t hv_op_signal_eq(hv_signal_t *sig, uint32_t val);
hv_assert_operand_t hv_op_signal_neq(hv_signal_t *sig, uint32_t val);
hv_assert_operand_t hv_op_signal_lt(hv_signal_t *sig, uint32_t val);
hv_assert_operand_t hv_op_signal_gt(hv_signal_t *sig, uint32_t val);
hv_assert_operand_t hv_op_signal_bits_set(hv_signal_t *sig, uint32_t mask);
hv_assert_operand_t hv_op_signal_eq_signal(hv_signal_t *a, hv_signal_t *b);

/* ========================================================================
 * L1: API - Sequence construction
 * ======================================================================== */

hv_sequence_expr_t *hv_seq_atom(hv_assert_operand_t op);
hv_sequence_expr_t *hv_seq_delay(hv_sequence_expr_t *seq, uint32_t n);
hv_sequence_expr_t *hv_seq_range_delay(hv_sequence_expr_t *seq,
                                        uint32_t min, uint32_t max);
hv_sequence_expr_t *hv_seq_consecutive_rep(hv_sequence_expr_t *seq,
                                            uint32_t n);
hv_sequence_expr_t *hv_seq_and(hv_sequence_expr_t *a, hv_sequence_expr_t *b);
hv_sequence_expr_t *hv_seq_or(hv_sequence_expr_t *a, hv_sequence_expr_t *b);
hv_sequence_expr_t *hv_seq_intersect(hv_sequence_expr_t *a,
                                      hv_sequence_expr_t *b);
hv_sequence_expr_t *hv_seq_throughout(hv_assert_operand_t expr,
                                       hv_sequence_expr_t *seq);
void                hv_seq_destroy(hv_sequence_expr_t *s);

/* ========================================================================
 * L1: API - Property construction
 * ======================================================================== */

hv_property_expr_t *hv_prop_sequence(hv_sequence_expr_t *seq);
hv_property_expr_t *hv_prop_implies_overlap(hv_sequence_expr_t *ante,
                                              hv_property_expr_t *cons);
hv_property_expr_t *hv_prop_implies_nonoverlap(hv_sequence_expr_t *ante,
                                                 hv_property_expr_t *cons);
hv_property_expr_t *hv_prop_disable_iff(hv_assert_operand_t cond,
                                         hv_property_expr_t *prop);
hv_property_expr_t *hv_prop_not(hv_property_expr_t *p);
hv_property_expr_t *hv_prop_and(hv_property_expr_t *a, hv_property_expr_t *b);
void                hv_prop_destroy(hv_property_expr_t *p);

/* ========================================================================
 * L2: API - Assertion management
 * ======================================================================== */

hv_assertion_t *hv_assertion_create(const char *name, assertion_type_t type,
                                     hv_property_expr_t *prop,
                                     hv_signal_t *clk);
void            hv_assertion_destroy(hv_assertion_t *a);
void            hv_assertion_set_message(hv_assertion_t *a, const char *msg);
void            hv_assertion_set_severity(hv_assertion_t *a, severity_t sev);

/* ========================================================================
 * L2 & L3: API - Assertion Engine
 * ======================================================================== */

hv_assertion_engine_t *hv_assertion_engine_create(hv_dut_t *dut);
void            hv_assertion_engine_destroy(hv_assertion_engine_t *eng);
void            hv_assertion_engine_add(hv_assertion_engine_t *eng,
                                         hv_assertion_t *a);
/* Evaluate all assertions at current simulation time */
void            hv_assertion_engine_eval(hv_assertion_engine_t *eng,
                                          sim_time_t time, uint32_t cycle);
/* Get pass/fail statistics */
void            hv_assertion_engine_report(const hv_assertion_engine_t *eng,
                                            FILE *fp);
/* Check if all assertions have passed */
bool            hv_assertion_engine_all_pass(const hv_assertion_engine_t *eng);

#endif /* ASSERTION_ENGINE_H */
