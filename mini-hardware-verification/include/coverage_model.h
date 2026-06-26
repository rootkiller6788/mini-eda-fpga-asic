/**
 * coverage_model.h - Coverage-Driven Verification (CDV)
 *
 * Implements IEEE 1800-2017 functional coverage model in C:
 *   Covergroups, coverpoints, cross-coverage, bins (auto/manual)
 *   Coverage database with merge/query/report
 *   Coverage-driven stimulus feedback loop
 *
 * L1: struct definitions for covergroup, coverpoint, bin
 * L2: CDV methodology (coverage-driven test generation)
 * L5: coverage gap analysis algorithm
 * L7: application to RISC-V ISA coverage
 *
 * Course: UT ECE 382V (VLSI Verification), CMU 18-613 (Testing)
 * Reference: Piziali, "Functional Verification Coverage Measurement" (2004)
 */

#ifndef COVERAGE_MODEL_H
#define COVERAGE_MODEL_H

#include "hw_verify.h"
#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 * L1: Bin Types
 * ======================================================================== */

typedef enum {
    BIN_AUTO,        /* automatic equal-width bins */
    BIN_MANUAL,      /* user-defined ranges */
    BIN_TRANSITION,  /* transition coverage (value A -> value B) */
    BIN_CROSS,       /* cross-product bin */
    BIN_ILLEGAL,     /* illegal value (should never occur) */
    BIN_IGNORE,      /* excluded from coverage */
} bin_type_t;

/* A single value range bin [low, high] */
typedef struct {
    uint32_t    low;
    uint32_t    high;
} hv_range_t;

/* Coverage bin */
typedef struct hv_coverage_bin {
    char        name[128];
    bin_type_t  type;
    hv_range_t  range;
    uint64_t    hit_count;          /* number of times this bin was sampled */
    uint64_t    goal_count;         /* at-least threshold (default 1) */
    bool        is_covered;
    bool        is_illegal;
    /* for transition bins: from_value -> to_value */
    uint32_t    from_value;
    uint32_t    to_value;
    /* for cross bins */
    uint32_t    cross_idx_a;
    uint32_t    cross_idx_b;
} hv_coverage_bin_t;

/* ========================================================================
 * L1: Coverpoint (a single variable being covered)
 * ======================================================================== */

typedef enum {
    CP_SCALAR,       /* single-bit or small enum */
    CP_RANGE,        /* integer range */
    CP_ENUM,         /* enumerated values */
    CP_TRANSITION,   /* transition coverpoint */
} coverpoint_type_t;

typedef struct hv_coverpoint {
    char              name[128];
    coverpoint_type_t type;
    uint32_t          width;           /* bit width of the covered variable */
    hv_coverage_bin_t *bins;
    size_t            num_bins;
    size_t            capacity_bins;
    uint64_t          total_hits;
    float             coverage_pct;    /* (covered bins / total legal bins) * 100 */
    /* callback to sample current value */
    uint32_t        (*sample_cb)(void *ctx);
    void             *sample_ctx;
} hv_coverpoint_t;

/* ========================================================================
 * L1: Cross Coverage (between two coverpoints)
 * ======================================================================== */

typedef struct hv_cross {
    char              name[128];
    hv_coverpoint_t  *cp_a;
    hv_coverpoint_t  *cp_b;
    hv_coverage_bin_t *bins;         /* cp_a->num_bins * cp_b->num_bins */
    size_t            num_bins;
    uint64_t          total_hits;
    float             coverage_pct;
} hv_cross_t;

/* ========================================================================
 * L1 & L2: Covergroup (collection of coverpoints + crosses)
 * ======================================================================== */

typedef struct hv_covergroup {
    char              name[128];
    hv_coverpoint_t **coverpoints;
    size_t            num_coverpoints;
    size_t            capacity_coverpoints;
    hv_cross_t      **crosses;
    size_t            num_crosses;
    size_t            capacity_crosses;
    /* overall metrics */
    float             aggregate_coverage;
    uint64_t          total_samples;
    char              comment[256];
} hv_covergroup_t;

/* ========================================================================
 * L2: Coverage Database (merge multiple covergroups across test runs)
 * ======================================================================== */

typedef struct hv_coverage_db {
    hv_covergroup_t **groups;
    size_t            num_groups;
    size_t            capacity_groups;
    char              name[128];
} hv_coverage_db_t;

/* ========================================================================
 * L1: Coverage API - Bin operations
 * ======================================================================== */

hv_coverage_bin_t  hv_bin_auto(const char *name, uint32_t low, uint32_t high);
hv_coverage_bin_t  hv_bin_manual(const char *name, uint32_t value);
hv_coverage_bin_t  hv_bin_transition(const char *name,
                                      uint32_t from, uint32_t to);
hv_coverage_bin_t  hv_bin_illegal(const char *name, uint32_t value);
void               hv_bin_hit(hv_coverage_bin_t *bin);

/* ========================================================================
 * L1: Coverage API - Coverpoint
 * ======================================================================== */

hv_coverpoint_t   *hv_coverpoint_create(const char *name, coverpoint_type_t type,
                                         uint32_t width);
void               hv_coverpoint_destroy(hv_coverpoint_t *cp);
void               hv_coverpoint_add_bin(hv_coverpoint_t *cp,
                                          hv_coverage_bin_t bin);
void               hv_coverpoint_sample(hv_coverpoint_t *cp, uint32_t value);
float              hv_coverpoint_get_coverage(const hv_coverpoint_t *cp);
void               hv_coverpoint_set_sample_cb(hv_coverpoint_t *cp,
                         uint32_t (*cb)(void*), void *ctx);
void               hv_coverpoint_report(const hv_coverpoint_t *cp, FILE *fp);

/* ========================================================================
 * L1 & L2: Coverage API - Covergroup
 * ======================================================================== */

hv_covergroup_t   *hv_covergroup_create(const char *name);
void               hv_covergroup_destroy(hv_covergroup_t *cg);
void               hv_covergroup_add_coverpoint(hv_covergroup_t *cg,
                                                 hv_coverpoint_t *cp);
hv_cross_t        *hv_covergroup_add_cross(hv_covergroup_t *cg,
                         const char *name, hv_coverpoint_t *a, hv_coverpoint_t *b);
float              hv_covergroup_get_coverage(const hv_covergroup_t *cg);
void               hv_covergroup_report(const hv_covergroup_t *cg, FILE *fp);

/* ========================================================================
 * L2 & L5: Coverage Database API
 * ======================================================================== */

hv_coverage_db_t  *hv_coverage_db_create(const char *name);
void               hv_coverage_db_destroy(hv_coverage_db_t *db);
void               hv_coverage_db_add_group(hv_coverage_db_t *db,
                                             hv_covergroup_t *cg);
float              hv_coverage_db_get_total(const hv_coverage_db_t *db);
void               hv_coverage_db_report(const hv_coverage_db_t *db, FILE *fp);
/* Merge another database into this one (union of hit counts) */
void               hv_coverage_db_merge(hv_coverage_db_t *dst,
                                         const hv_coverage_db_t *src);

/* ========================================================================
 * L5: Coverage Gap Analysis - find uncovered bins
 * ======================================================================== */

typedef struct hv_coverage_gap {
    char        group_name[128];
    char        coverpoint_name[128];
    char        bin_name[128];
    uint32_t    expected_min;
    uint32_t    expected_max;
    uint32_t    priority;       /* 0=critical gap */
} hv_coverage_gap_t;

typedef struct hv_coverage_gap_list {
    hv_coverage_gap_t *gaps;
    size_t              num_gaps;
} hv_coverage_gap_list_t;

hv_coverage_gap_list_t *hv_coverage_find_gaps(const hv_coverage_db_t *db);
void                    hv_coverage_gap_list_destroy(hv_coverage_gap_list_t *list);
void                    hv_coverage_gap_list_report(const hv_coverage_gap_list_t *list,
                                                     FILE *fp);

#endif /* COVERAGE_MODEL_H */
