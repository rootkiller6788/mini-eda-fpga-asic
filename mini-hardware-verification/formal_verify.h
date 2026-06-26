#ifndef FORMAL_VERIFY_H
#define FORMAL_VERIFY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ================================================================
   Formal Verification — Model Checking & Property Proving (C99)
   formal_verify.h
   ================================================================ */

/* ----- Property Types ----- */
typedef enum {
    FORMAL_PROP_SAFETY    = 0,   /* "always" — something bad never happens */
    FORMAL_PROP_LIVENESS  = 1,   /* "eventually" — something good eventually happens */
    FORMAL_PROP_FAIRNESS  = 2,   /* fairness constraint */
    FORMAL_PROP_REACHABILITY = 3, /* reachable state check */
    FORMAL_PROP_INVARIANT = 4    /* invariant holds in all reachable states */
} formal_property_type_t;

static inline const char* formal_property_type_name(formal_property_type_t t) {
    static const char* names[] = {
        "safety", "liveness", "fairness", "reachability", "invariant"
    };
    return (t <= FORMAL_PROP_INVARIANT) ? names[t] : "unknown";
}

/* ----- Verification Engines ----- */
typedef enum {
    FORMAL_ENGINE_BMC       = 0,  /* Bounded Model Checking */
    FORMAL_ENGINE_INDUCTION = 1,  /* k-induction */
    FORMAL_ENGINE_COVER     = 2,  /* cover mode */
    FORMAL_ENGINE_SMT       = 3,  /* SMT-based (SymbiYosys) */
    FORMAL_ENGINE_ABC       = 4   /* ABC engine */
} formal_engine_t;

/* ----- Solver Backend ----- */
typedef enum {
    SOLVER_YICES    = 0,
    SOLVER_Z3       = 1,
    SOLVER_BOOLECTOR = 2,
    SOLVER_CVC4     = 3,
    SOLVER_MATHSAT  = 4,
    SOLVER_BITWUZLA = 5
} formal_solver_t;

static inline const char* formal_solver_name(formal_solver_t s) {
    static const char* names[] = {
        "yices", "z3", "boolector", "cvc4", "mathsat", "bitwuzla"
    };
    return (s <= SOLVER_BITWUZLA) ? names[s] : "unknown";
}

/* ----- Result Codes ----- */
typedef enum {
    FORMAL_RESULT_UNKNOWN   = 0,
    FORMAL_RESULT_PASS      = 1,  /* property holds */
    FORMAL_RESULT_FAIL      = 2,  /* counter-example found */
    FORMAL_RESULT_TIMEOUT   = 3,
    FORMAL_RESULT_INCONCLUSIVE = 4,
    FORMAL_RESULT_ERROR     = 5
} formal_result_t;

/* ----- State Representation ----- */
typedef struct formal_state formal_state_t;

typedef bool (*formal_state_equal_fn)(const formal_state_t* a,
    const formal_state_t* b);
typedef uint64_t (*formal_state_hash_fn)(const formal_state_t* s);
typedef void (*formal_state_print_fn)(const formal_state_t* s, FILE* fp);

struct formal_state {
    uint32_t              id;
    uint8_t*              data;      /* bit-vector of state variables */
    size_t                data_bits; /* total bits in state */
    formal_state_print_fn print;
    formal_state_equal_fn equal;
    formal_state_hash_fn  hash;
};

formal_state_t* formal_state_create(size_t num_bits);
void formal_state_destroy(formal_state_t* state);
bool formal_state_equal(const formal_state_t* a, const formal_state_t* b);
uint64_t formal_state_hash(const formal_state_t* s);
void formal_state_set_bit(formal_state_t* s, size_t bit_idx, bool val);
bool formal_state_get_bit(const formal_state_t* s, size_t bit_idx);

/* ----- Transition Relation ----- */
typedef struct formal_transition formal_transition_t;

typedef void (*formal_transition_fn)(const formal_state_t* current,
    formal_state_t* next, void* ctx);

struct formal_transition {
    char                name[64];
    formal_transition_fn step;
    void*               ctx;
};

formal_transition_t* formal_trans_create(const char* name,
    formal_transition_fn step, void* ctx);
void formal_trans_destroy(formal_transition_t* trans);
void formal_trans_apply(const formal_transition_t* trans,
    const formal_state_t* curr, formal_state_t* next);

/* ----- Initial State Constraint ----- */
typedef bool (*formal_init_fn)(formal_state_t* state, void* ctx);

typedef struct {
    char          name[64];
    formal_init_fn is_init;
    void*         ctx;
} formal_init_t;

/* ----- Property Definition ----- */
typedef struct formal_property formal_property_t;

typedef bool (*formal_prop_eval_fn)(const formal_state_t* state, void* ctx);

struct formal_property {
    char                  name[64];
    formal_property_type_t type;
    formal_prop_eval_fn   check;       /* safety: returns true if property holds */
    char*                 smt_expr;    /* SMT-LIB2 expression string */
    bool                  is_proven;
    formal_result_t       result;
    uint64_t              bound;       /* BMC bound / induction depth */
    void*                 ctx;
};

formal_property_t* formal_property_create(const char* name,
    formal_property_type_t type, formal_prop_eval_fn check, void* ctx);
void formal_property_destroy(formal_property_t* prop);
bool formal_property_check(const formal_property_t* prop,
    const formal_state_t* state);
void formal_property_set_smt(formal_property_t* prop, const char* smt);

/* ----- Counterexample Trace ----- */
typedef struct formal_counterexample formal_cex_t;

struct formal_counterexample {
    formal_state_t*   states;         /* array of states in the trace */
    int               length;         /* number of states */
    char              description[512];
    int               failure_step;   /* step at which property fails */
};

formal_cex_t* formal_cex_create(int max_length);
void formal_cex_destroy(formal_cex_t* cex);
void formal_cex_add_state(formal_cex_t* cex, const formal_state_t* state);
void formal_cex_print(const formal_cex_t* cex, FILE* fp);
void formal_cex_export_vcd(const formal_cex_t* cex,
    const char* filename);

/* ----- Bounded Model Checking (BMC) ----- */
typedef struct formal_bmc formal_bmc_t;

struct formal_bmc {
    formal_engine_t     engine;
    formal_solver_t     solver;
    uint64_t            max_bound;       /* maximum unrolling depth */
    uint64_t            current_bound;
    uint64_t            step_size;
    double              timeout_sec;
    formal_transition_t* transition;
    formal_init_t*      initial;
    formal_property_t** properties;
    int                 prop_count;
    formal_cex_t*       counterexample;
    bool                use_incremental; /* incremental SAT */
    char                solver_path[256];
    bool                verbose;
};

formal_bmc_t* formal_bmc_create(void);
void formal_bmc_destroy(formal_bmc_t* bmc);
void formal_bmc_set_solver(formal_bmc_t* bmc, formal_solver_t solver,
    const char* path);
void formal_bmc_set_bound(formal_bmc_t* bmc, uint64_t max_bound);
void formal_bmc_set_transition(formal_bmc_t* bmc,
    formal_transition_t* trans);
void formal_bmc_set_initial(formal_bmc_t* bmc, formal_init_t* init);
void formal_bmc_add_property(formal_bmc_t* bmc,
    formal_property_t* prop);
formal_result_t formal_bmc_run(formal_bmc_t* bmc);
const formal_cex_t* formal_bmc_get_cex(const formal_bmc_t* bmc);

/* ----- Induction Proof (k-induction) ----- */
typedef struct formal_induction formal_ind_t;

struct formal_induction {
    formal_bmc_t*       base_bmc;        /* base case BMC */
    uint64_t            depth;           /* induction depth k */
    uint64_t            max_depth;
    formal_transition_t* transition;
    formal_init_t*      initial;
    formal_property_t** properties;
    int                 prop_count;
    formal_result_t     result;
    bool                use_induction_strengthening;
};

formal_ind_t* formal_induction_create(void);
void formal_induction_destroy(formal_ind_t* ind);
void formal_induction_set_depth(formal_ind_t* ind, uint64_t depth);
void formal_induction_set_transition(formal_ind_t* ind,
    formal_transition_t* trans);
void formal_induction_set_initial(formal_ind_t* ind, formal_init_t* init);
void formal_induction_add_property(formal_ind_t* ind,
    formal_property_t* prop);
formal_result_t formal_induction_prove(formal_ind_t* ind);

/* ----- SymbiYosys Flow ----- */
typedef struct symbiyosys_flow symbiyosys_flow_t;

typedef enum {
    SYMBI_MODE_BMC     = 0,
    SYMBI_MODE_PROVE   = 1,
    SYMBI_MODE_COVER   = 2,
    SYMBI_MODE_LIVE    = 3
} symbiyosys_mode_t;

struct symbiyosys_flow {
    symbiyosys_mode_t  mode;
    formal_engine_t    engine;
    formal_solver_t    solver;
    char*              design_files[16];
    int                design_count;
    char               top_module[64];
    formal_property_t** properties;
    int                prop_count;
    char               config_file[256];
    char               output_dir[256];
    uint64_t           depth;
    double             timeout_sec;
    bool               append_mode;
};

symbiyosys_flow_t* symbiyosys_create(void);
void symbiyosys_destroy(symbiyosys_flow_t* flow);
void symbiyosys_add_design(symbiyosys_flow_t* flow, const char* file);
void symbiyosys_set_top(symbiyosys_flow_t* flow, const char* top);
void symbiyosys_set_mode(symbiyosys_flow_t* flow, symbiyosys_mode_t mode);
void symbiyosys_set_engine(symbiyosys_flow_t* flow, formal_engine_t eng);
void symbiyosys_set_solver(symbiyosys_flow_t* flow, formal_solver_t solver);
void symbiyosys_add_property(symbiyosys_flow_t* flow,
    formal_property_t* prop);
bool symbiyosys_generate_config(symbiyosys_flow_t* flow);
bool symbiyosys_run(symbiyosys_flow_t* flow);

/* ----- Equivalent Check (Combinational) ----- */
typedef struct formal_equiv_check formal_equiv_t;

typedef bool (*formal_equiv_fn)(void* golden, void* revised,
    formal_state_t* input_state, void* ctx);

struct formal_equiv_check {
    char            name[64];
    formal_equiv_fn checker;
    void*           golden_model;
    void*           revised_model;
    formal_solver_t solver;
    uint64_t        num_inputs;
    uint64_t        checks_passed;
    uint64_t        checks_failed;
    void*           ctx;
};

formal_equiv_t* formal_equiv_create(const char* name,
    formal_equiv_fn checker, void* golden, void* revised);
void formal_equiv_destroy(formal_equiv_t* eq);
formal_result_t formal_equiv_verify(formal_equiv_t* eq);

/* ----- BMC Trace Minimization ----- */
void formal_bmc_minimize_trace(formal_bmc_t* bmc);
int  formal_bmc_trace_quality(const formal_cex_t* cex);

/* ----- Coverage in Formal ----- */
typedef struct formal_coverage formal_cov_t;

struct formal_coverage {
    char        property_name[64];
    bool        covered;        /* reached during prove/cover */
    bool        unreachable;    /* proven unreachable */
    uint64_t    reach_depth;
    bool        is_cover_point;
};

formal_cov_t* formal_cov_create(const char* name);
void formal_cov_destroy(formal_cov_t* cov);

/* ----- Utility ----- */
void formal_print_result(formal_result_t result, FILE* fp);
void formal_dump_state(const formal_state_t* state, FILE* fp);
void formal_export_smtlib2(const char* filename,
    formal_transition_t* trans, formal_property_t** props,
    int prop_count, uint64_t bound);

#endif /* FORMAL_VERIFY_H */
