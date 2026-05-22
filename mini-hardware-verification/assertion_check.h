#ifndef ASSERTION_CHECK_H
#define ASSERTION_CHECK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

/* ================================================================
   Assertion Check — SVA & PSL-style Assertions (C99)
   assertion_check.h
   ================================================================ */

/* ----- Assertion Types ----- */
typedef enum {
    ASSERT_IMMEDIATE = 0,   /* evaluated immediately in procedural code */
    ASSERT_CONCURRENT = 1,  /* evaluated on clock edge, temporal behavior */
    ASSERT_DEFERRED   = 2   /* reported at end of time step */
} assertion_type_t;

typedef enum {
    ASSERT_STATE_ACTIVE   = 0,
    ASSERT_STATE_PASS     = 1,
    ASSERT_STATE_FAIL     = 2,
    ASSERT_STATE_DISABLED = 3,
    ASSERT_STATE_VACUOUS  = 4   /* antecedent never satisfied */
} assertion_state_t;

typedef enum {
    ASSERT_SEVERITY_INFO    = 0,
    ASSERT_SEVERITY_WARNING = 1,
    ASSERT_SEVERITY_ERROR   = 2,
    ASSERT_SEVERITY_FATAL   = 3
} assertion_severity_t;

/* ----- SVA-style Sequence Operators ----- */
typedef enum {
    SEQ_CYCLE_DELAY      = 0,  /* ##N */
    SEQ_CYCLE_RANGE      = 1,  /* ##[min:max] */
    SEQ_FIRST_MATCH      = 2,  /* first_match */
    SEQ_THROUGHOUT       = 3,  /* throughout */
    SEQ_WITHIN           = 4,  /* within */
    SEQ_INTERSECT        = 5,  /* intersect */
    SEQ_AND              = 6,  /* and */
    SEQ_OR               = 7,  /* or */
    SEQ_OVERLAP_IMPL     = 8,  /* |-> overlapping implication */
    SEQ_NONOVERLAP_IMPL  = 9,  /* |=> non-overlapping implication */
    SEQ_REPETITION       = 10, /* [*N] consecutive repetition */
    SEQ_GOTO_REPETITION  = 11, /* [->N] goto repetition */
    SEQ_NONCONSEC_REP    = 12  /* [=N] non-consecutive repetition */
} sequence_op_t;

/* ----- SVA Property Operators ----- */
typedef enum {
    PROP_S_ALWAYS     = 0,  /* always */
    PROP_S_EVENTUALLY = 1,  /* s_eventually */
    PROP_S_NEXT       = 2,  /* s_next */
    PROP_UNTIL        = 3,  /* until */
    PROP_S_UNTIL      = 4,  /* s_until */
    PROP_ACCEPT_ON    = 5,  /* accept_on */
    PROP_REJECT_ON    = 6,  /* reject_on */
    PROP_SYNC_ACCEPT  = 7,  /* sync_accept_on */
    PROP_SYNC_REJECT  = 8,  /* sync_reject_on */
    PROP_NOT          = 9,  /* not */
    PROP_DISABLE_IFF  = 10  /* disable iff */
} property_op_t;

/* ----- SVA-style Temporal Expression ----- */
typedef struct sva_expr sva_expr_t;

typedef bool (*sva_eval_fn)(sva_expr_t* expr, void* ctx);

typedef struct sva_sequence_step sva_sequence_step_t;

struct sva_sequence_step {
    sva_expr_t*    match_expr;   /* expression that must match at this step */
    int            cycle_delay;  /* delay before this step (##N) */
    int            cycle_min;    /* range min */
    int            cycle_max;    /* range max */
    sequence_op_t  op;           /* sequence operator */
    void*          next;         /* next step in chain */
    bool           is_repetition;
    int            rep_min;
    int            rep_max;
};

/* SVA expression */
struct sva_expr {
    char        name[64];
    sva_eval_fn eval;
    void*       ctx;
    bool        last_value;
    int         evaluation_count;
};

/* ----- Assertion Clock Specification ----- */
typedef struct assertion_clock assertion_clock_t;

typedef bool (*assert_clock_edge_fn)(assertion_clock_t* clk);

struct assertion_clock {
    char               name[32];
    assert_clock_edge_fn posedge_detect;
    assert_clock_edge_fn negedge_detect;
    uint64_t           tick_count;
    bool               is_posedge;
};

/* ----- Assertion Definition ----- */
typedef struct assertion_def assertion_def_t;

typedef bool (*assert_check_fn)(assertion_def_t* assert, void* ctx);

struct assertion_def {
    char                name[64];
    assertion_type_t    type;
    assertion_severity_t severity;
    assertion_state_t   state;
    assertion_clock_t*  clock;          /* clock for concurrent assertions */
    sva_sequence_step_t* antecedent;     /* left-hand side of implication */
    sva_sequence_step_t* consequent;     /* right-hand side of implication */
    assert_check_fn     check;          /* immediate check function */
    char                message[256];   /* failure message */
    uint64_t            pass_count;
    uint64_t            fail_count;
    uint64_t            vacuous_pass_count;
    uint64_t            attempt_count;
    bool                is_formal;      /* used for formal proof target */
    bool                is_disabled;
    char                action_block[256]; /* action on failure */
    void*               ctx;
};

assertion_def_t* assertion_create(const char* name, assertion_type_t type);
void assertion_destroy(assertion_def_t* a);
void assertion_set_clock(assertion_def_t* a, assertion_clock_t* clk);
void assertion_set_severity(assertion_def_t* a, assertion_severity_t sev);
void assertion_set_message(assertion_def_t* a, const char* msg);
void assertion_set_action(assertion_def_t* a, const char* action);

/* ----- Immediate Assertion ----- */
bool assert_immediate(assertion_def_t* a, bool condition);
#define ASSERT_IMMEDIATE_COND(a, cond, sev, msg) \
    do { \
        (a)->attempt_count++; \
        if (!(cond)) { \
            (a)->fail_count++; \
            (a)->state = ASSERT_STATE_FAIL; \
            assertion_report_failure(a, __FILE__, __LINE__, msg); \
            if ((sev) == ASSERT_SEVERITY_FATAL) exit(1); \
        } else { \
            (a)->pass_count++; \
            (a)->state = ASSERT_STATE_PASS; \
        } \
    } while(0)

/* ----- Concurrent Assertion ----- */
bool assert_concurrent_eval(assertion_def_t* a, void* ctx);
bool assert_concurrent_tick(assertion_def_t* a, void* ctx);
bool assert_sequence_step_eval(sva_sequence_step_t* step, void* ctx, int* cycle);

/* ----- PSL (Property Specification Language) ----- */
typedef struct psl_property psl_property_t;

typedef enum {
    PSL_ALWAYS   = 0,
    PSL_NEVER    = 1,
    PSL_NEXT     = 2,
    PSL_EVENTUALLY = 3,
    PSL_BEFORE   = 4,
    PSL_UNTIL    = 5,
    PSL_UNTIL2   = 6,    /* strong until */
    PSL_BEFORE2  = 7     /* strong before */
} psl_op_t;

struct psl_property {
    char      name[64];
    psl_op_t  op;
    sva_expr_t* left;
    sva_expr_t* right;
    assertion_clock_t* clock;
    uint64_t  check_count;
    uint64_t  hold_count;
};

psl_property_t* psl_property_create(const char* name, psl_op_t op);
void psl_property_destroy(psl_property_t* prop);
bool psl_property_check(psl_property_t* prop, void* ctx);

/* ----- Assertion Coverage ----- */
typedef struct assertion_coverage assertion_coverage_t;

struct assertion_coverage {
    char        name[64];
    uint64_t    attempt_count;       /* total attempts */
    uint64_t    success_count;       /* assertion passed */
    uint64_t    failure_count;       /* assertion failed */
    uint64_t    vacuous_count;       /* vacuous pass */
    uint64_t    active_count;        /* non-vacuous */
    double      pass_rate;           /* success / attempt */
    double      activation_rate;     /* active / attempt */
    bool        covered;             /* at least one success */
    int         min_cycle_depth;
    int         max_cycle_depth;
};

assertion_coverage_t* assertion_coverage_create(const char* name);
void assertion_coverage_destroy(assertion_coverage_t* cov);
void assertion_coverage_record(assertion_coverage_t* cov,
    assertion_def_t* a);
void assertion_coverage_report(const assertion_coverage_t* cov);

/* ----- Assertion Bank (Manager) ----- */
typedef struct assertion_bank assertion_bank_t;

#define ASSERTION_BANK_MAX 128

struct assertion_bank {
    assertion_def_t*     assertions[ASSERTION_BANK_MAX];
    int                  count;
    assertion_clock_t*   default_clock;
    assertion_coverage_t* coverages[ASSERTION_BANK_MAX];
    int                  cov_count;
    uint64_t             total_pass;
    uint64_t             total_fail;
    uint64_t             total_attempt;
};

assertion_bank_t* assertion_bank_create(void);
void assertion_bank_destroy(assertion_bank_t* bank);
void assertion_bank_add(assertion_bank_t* bank, assertion_def_t* a);
void assertion_bank_add_coverage(assertion_bank_t* bank,
    assertion_coverage_t* cov);
void assertion_bank_set_default_clock(assertion_bank_t* bank,
    assertion_clock_t* clk);
bool assertion_bank_check_all(assertion_bank_t* bank, void* ctx);
void assertion_bank_report(const assertion_bank_t* bank);

/* ----- Formal Verification with Assertions ----- */
bool assertion_to_formal(assertion_def_t* a,
    char* output_file, const char* lang);
bool assertion_prove(assertion_def_t* a, const char* solver_path,
    int max_cycles);
bool assertion_get_counterexample(assertion_def_t* a,
    void** trace, int* trace_len);

/* ----- Utility Functions ----- */
void assertion_report_failure(const assertion_def_t* a,
    const char* file, int line, const char* msg);
void assertion_print_state(const assertion_def_t* a, FILE* fp);
void assertion_log_to_file(const assertion_def_t* a,
    const char* filename);

sva_expr_t* sva_expr_create(const char* name, sva_eval_fn eval, void* ctx);
void sva_expr_destroy(sva_expr_t* expr);
bool sva_expr_eval(sva_expr_t* expr, void* ctx);

sva_sequence_step_t* sva_seq_step_create(sva_expr_t* expr,
    sequence_op_t op, int delay);
void sva_seq_step_destroy(sva_sequence_step_t* step);
sva_sequence_step_t* sva_seq_chain(sva_sequence_step_t** steps, int count);

assertion_clock_t* assertion_clock_create(const char* name,
    assert_clock_edge_fn posedge_fn);
void assertion_clock_destroy(assertion_clock_t* clk);
bool assertion_clock_tick(assertion_clock_t* clk);

#endif /* ASSERTION_CHECK_H */
