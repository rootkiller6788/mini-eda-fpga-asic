#ifndef COVERAGE_MDL_H
#define COVERAGE_MDL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ================================================================
   Coverage Model — Code & Functional Coverage (C99)
   coverage_mdl.h
   ================================================================ */

/* ----- Code Coverage Types ----- */
typedef enum {
    COV_CODE_LINE       = 0,
    COV_CODE_BRANCH     = 1,
    COV_CODE_TOGGLE     = 2,
    COV_CODE_FSM_STATE  = 3,
    COV_CODE_FSM_ARC    = 4,   /* FSM transition / edge */
    COV_CODE_EXPRESSION = 5,   /* expression coverage */
    COV_CODE_CONDITION  = 6,   /* condition coverage */
    COV_CODE_COUNT
} cov_code_type_t;

static inline const char* cov_code_type_name(cov_code_type_t t) {
    static const char* names[COV_CODE_COUNT] = {
        "line", "branch", "toggle", "fsm_state",
        "fsm_arc", "expression", "condition"
    };
    return (t < COV_CODE_COUNT) ? names[t] : "unknown";
}

/* ----- Code Coverage Item ----- */
typedef struct cov_code_item cov_code_item_t;

struct cov_code_item {
    char            name[128];
    char            file[256];
    int             line_start;
    int             line_end;
    cov_code_type_t type;
    bool            hit;
    uint64_t        hit_count;
    int             total_bins;       /* for toggle: 0/1/X; for branch: T/F count */
    int             hit_bins;
    bool            waived;           /* excluded from coverage */
    char            waiver_reason[256];
};

cov_code_item_t* cov_code_item_create(const char* name,
    cov_code_type_t type, const char* file, int line);
void cov_code_item_destroy(cov_code_item_t* item);
void cov_code_item_hit(cov_code_item_t* item);

/* ----- Code Coverage Database ----- */
typedef struct cov_code_db cov_code_db_t;

#define COV_CODE_DB_MAX_ITEMS 1024

struct cov_code_db {
    cov_code_item_t* items[COV_CODE_DB_MAX_ITEMS];
    int              count;
    double           overall_coverage;  /* hit / total */
    double           line_coverage;
    double           branch_coverage;
    double           toggle_coverage;
    double           fsm_coverage;
};

cov_code_db_t* cov_code_db_create(void);
void cov_code_db_destroy(cov_code_db_t* db);
void cov_code_db_add(cov_code_db_t* db, cov_code_item_t* item);
cov_code_item_t* cov_code_db_find(cov_code_db_t* db, const char* name);
void cov_code_db_update_metrics(cov_code_db_t* db);
void cov_code_db_report(const cov_code_db_t* db, FILE* fp);
void cov_code_db_merge(cov_code_db_t* dst, const cov_code_db_t* src);

/* ----- Coverpoint ----- */
typedef struct cov_coverpoint cov_coverpoint_t;

typedef int (*cov_cp_bin_fn)(uint64_t value, void* ctx);

struct cov_coverpoint {
    char            name[64];
    int             num_bins;           /* total number of bins */
    uint64_t*       bin_hits;           /* per-bin hit counts */
    uint64_t*       bin_values;         /* auto-bin: representative value */
    double          bin_min;            /* minimum value range */
    double          bin_max;            /* maximum value range */
    int             auto_bin_count;     /* auto-generated bin count */
    bool            auto_bin_mode;
    cov_cp_bin_fn   bin_func;           /* custom binning */
    void*           bin_ctx;
    uint64_t        total_samples;      /* total times sampled */
    int             hit_bins;           /* number of bins hit */
    double          coverage;           /* hit_bins / num_bins * 100% */
    int             at_least;           /* at_least threshold per bin */
    double          goal;               /* coverage goal (e.g. 100.0) */
    bool            covered;
    bool            is_illegal_bin[64]; /* illegal bin flags */
    uint64_t        illegal_hits;
};

cov_coverpoint_t* cov_coverpoint_create(const char* name, int num_bins);
void cov_coverpoint_destroy(cov_coverpoint_t* cp);
void cov_coverpoint_set_range(cov_coverpoint_t* cp,
    double min_val, double max_val);
void cov_coverpoint_set_auto_bins(cov_coverpoint_t* cp, int count);
void cov_coverpoint_set_custom_bin_func(cov_coverpoint_t* cp,
    cov_cp_bin_fn fn, void* ctx);
void cov_coverpoint_set_at_least(cov_coverpoint_t* cp, int threshold);
void cov_coverpoint_set_goal(cov_coverpoint_t* cp, double goal_pct);
void cov_coverpoint_add_illegal_bin(cov_coverpoint_t* cp, int bin_idx);
void cov_coverpoint_sample(cov_coverpoint_t* cp, uint64_t value);
bool cov_coverpoint_is_covered(const cov_coverpoint_t* cp);
void cov_coverpoint_reset(cov_coverpoint_t* cp);

/* ----- Cross Coverage ----- */
typedef struct cov_cross cov_cross_t;

struct cov_cross {
    char              name[64];
    cov_coverpoint_t** coverpoints;     /* participating coverpoints */
    int               num_cps;
    int               total_cross_bins; /* product of all bin counts */
    uint32_t*         cross_hits;       /* flat 2D/3D hit array */
    int               hit_bins;
    double            coverage;
    double            goal;
    bool              covered;
    bool              auto_bin_max;     /* auto bin all cross products */
};

cov_cross_t* cov_cross_create(const char* name);
void cov_cross_destroy(cov_cross_t* cross);
void cov_cross_add_coverpoint(cov_cross_t* cross, cov_coverpoint_t* cp);
void cov_cross_build(cov_cross_t* cross);
void cov_cross_sample(cov_cross_t* cross);
bool cov_cross_is_covered(const cov_cross_t* cross);
void cov_cross_report(const cov_cross_t* cross, FILE* fp);

/* ----- Covergroup ----- */
typedef struct cov_covergroup cov_covergroup_t;

typedef void (*cov_group_sample_fn)(struct cov_covergroup* cg,
    void* ctx);

struct cov_covergroup {
    char              name[64];
    cov_coverpoint_t** coverpoints;
    int               cp_count;
    int               cp_capacity;
    cov_cross_t**     crosses;
    int               cross_count;
    int               cross_capacity;
    cov_group_sample_fn sample_fn;
    void*             sample_ctx;
    double            overall_coverage;
    double            goal;
    uint64_t          sample_count;
    bool              auto_sample;
    uint32_t          sample_event;     /* event ID for triggering */
};

cov_covergroup_t* cov_covergroup_create(const char* name);
void cov_covergroup_destroy(cov_covergroup_t* cg);
void cov_covergroup_add_coverpoint(cov_covergroup_t* cg,
    cov_coverpoint_t* cp);
void cov_covergroup_add_cross(cov_covergroup_t* cg, cov_cross_t* cross);
void cov_covergroup_set_sample_fn(cov_covergroup_t* cg,
    cov_group_sample_fn fn, void* ctx);
void cov_covergroup_sample(cov_covergroup_t* cg);
double cov_covergroup_get_coverage(const cov_covergroup_t* cg);
bool cov_covergroup_is_covered(const cov_covergroup_t* cg);
void cov_covergroup_report(const cov_covergroup_t* cg, FILE* fp);

/* ----- Coverage Closure ----- */
typedef struct cov_closure cov_closure_t;

typedef enum {
    COV_CLOSURE_MERGED     = 0,   /* all covergroups merged */
    COV_CLOSURE_PER_TEST   = 1,   /* per-test coverage */
    COV_CLOSURE_INCREMENTAL = 2   /* incremental additions */
} cov_closure_type_t;

struct cov_closure {
    cov_covergroup_t** groups;
    int               group_count;
    int               group_capacity;
    cov_code_db_t*    code_db;
    double            functional_goal;
    double            code_goal;
    uint64_t          total_samples;
    bool              all_covered;
};

cov_closure_t* cov_closure_create(void);
void cov_closure_destroy(cov_closure_t* closure);
void cov_closure_add_group(cov_closure_t* closure,
    cov_covergroup_t* cg);
void cov_closure_set_code_db(cov_closure_t* closure,
    cov_code_db_t* db);
void cov_closure_set_goals(cov_closure_t* closure,
    double functional_goal, double code_goal);
double cov_closure_get_overall_coverage(const cov_closure_t* closure);
void cov_closure_report(const cov_closure_t* closure, FILE* fp);
void cov_closure_save(const cov_closure_t* closure,
    const char* filename);
void cov_closure_merge(cov_closure_t* dst,
    const cov_closure_t* src);

/* ----- Coverage Waiver / Exclusion ----- */
typedef struct cov_waiver cov_waiver_t;

struct cov_waiver {
    char   target_name[128];
    char   reason[256];
    char   owner[64];
    char   date[32];
    bool   is_active;
    int    waived_bins[64];
    int    waived_count;
};

cov_waiver_t* cov_waiver_create(const char* target,
    const char* reason, const char* owner);
void cov_waiver_destroy(cov_waiver_t* w);
void cov_waiver_apply_to_coverpoint(cov_waiver_t* w,
    cov_coverpoint_t* cp);
void cov_waiver_apply_to_item(cov_waiver_t* w,
    cov_code_item_t* item);

/* ----- Coverage-Driven Verification (CDV) ----- */
typedef struct cov_driven_vrf cov_cdv_t;

struct cov_driven_vrf {
    cov_closure_t*  closure;
    uint32_t        seed;
    int             iteration;
    int             max_iterations;
    double          prev_coverage;
    double          improvement_threshold;
    bool            converged;
    uint32_t*       random_seeds;
    int             seed_count;
};

cov_cdv_t* cov_cdv_create(cov_closure_t* closure, uint32_t seed);
void cov_cdv_destroy(cov_cdv_t* cdv);
bool cov_cdv_has_converged(const cov_cdv_t* cdv);
void cov_cdv_iterate(cov_cdv_t* cdv);
void cov_cdv_report(const cov_cdv_t* cdv, FILE* fp);

/* ----- Coverage Database Merge & Export ----- */
void cov_export_html(const cov_closure_t* closure,
    const char* output_dir);
void cov_export_xml(const cov_closure_t* closure,
    const char* filename);
void cov_export_ucis(const cov_closure_t* closure,
    const char* filename);

/* ----- Utility ----- */
void cov_print_hit_map(const cov_code_db_t* db, const char* filename);
void cov_print_histogram(const cov_covergroup_t* cg, FILE* fp);

#endif /* COVERAGE_MDL_H */
