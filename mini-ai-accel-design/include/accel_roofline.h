#ifndef ACCEL_ROOFLINE_H
#define ACCEL_ROOFLINE_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/* ================================================================
 * Roofline Model for AI Accelerator Performance Analysis
 * Based on: Williams, Waterman, Patterson (2009) "Roofline: An
 * Insightful Visual Performance Model for Multicore Architectures"
 *
 * L1: Core data structures for performance characterization
 * L2: Operational intensity concept, compute vs memory bound
 * L4: Roofline model theorem, Amdahl's Law for accelerators
 * L5: Roofline ceiling analysis algorithm
 * L7: Application — DSE for accelerator configuration
 * ================================================================ */

#define ROOFLINE_MAX_CEILINGS  10
#define ROOFLINE_MAX_POINTS    256

/* --- Performance Ceilings --- */
typedef enum {
    ROOF_PEAK_COMPUTE = 0,      /* raw FLOP/s */
    ROOF_ILP_LIMITED,           /* instruction-level parallelism bound */
    ROOF_SIMD_LIMITED,          /* SIMD width bound */
    ROOF_FMA_LIMITED,           /* fused multiply-add utilization */
    ROOF_BRANCH_LIMITED,        /* branch divergence bound */
    ROOF_SPARSITY_OVERHEAD,     /* sparse computation overhead */
    ROOF_PRECISION_CASTING,     /* mixed-precision conversion cost */
    ROOF_MEM_BW,                /* DRAM bandwidth bound */
    ROOF_L2_BW,                 /* L2/global buffer bandwidth bound */
    ROOF_L1_BW                  /* L1/local buffer bandwidth bound */
} roof_ceiling_type_t;

typedef struct {
    roof_ceiling_type_t type;
    char    name[64];
    double  value_gflops;       /* ceiling in GFLOP/s */
    bool    is_memory_bound;    /* true if this is a bandwidth ceiling */
} roof_ceiling_t;

/* --- Operational Point --- */
typedef struct {
    char    kernel_name[64];
    double  operational_intensity;   /* FLOP/byte */
    double  performance_gflops;      /* achieved GFLOP/s */
    bool    compute_bound;           /* true if above the ridge point */
    double  ridge_distance;          /* log distance from ridge */
    double  utilization;             /* fraction of peak */
    uint64_t total_flops;
    uint64_t total_bytes;
} roof_point_t;

/* --- Accelerator Specification --- */
typedef struct {
    double  peak_compute_gflops;     /* theoretical peak */
    double  peak_sram_bw_gb_s;       /* on-chip SRAM bandwidth */
    double  peak_dram_bw_gb_s;       /* off-chip DRAM bandwidth */
    double  l1_size_kb;              /* local buffer size */
    double  l2_size_kb;              /* global buffer size */
    double  clock_ghz;
    uint32_t num_mac_units;
    uint32_t simd_width;
    uint32_t num_cores;
    double  process_node_nm;         /* technology node */
    double  power_budget_w;
    double  energy_per_mac_pj;
} roof_accel_spec_t;

/* --- Roofline Model --- */
typedef struct {
    roof_accel_spec_t   spec;
    roof_ceiling_t      ceilings[ROOFLINE_MAX_CEILINGS];
    uint32_t            ceiling_count;
    roof_point_t        points[ROOFLINE_MAX_POINTS];
    uint32_t            point_count;
    double              ridge_point;         /* operational intensity at the ridge */
    double              peak_bandwidth_gb_s; /* maximum achievable bandwidth */
} roofline_model_t;

/* --- Design Space Exploration Result --- */
typedef struct {
    uint32_t    array_rows;
    uint32_t    array_cols;
    double      sram_kb;
    double      dram_bw_gb_s;
    double      clock_ghz;
    double      projected_tops;
    double      projected_utilization;
    double      energy_efficiency_tops_per_w;
    double      area_mm2_est;
    double      cost_score;
} roof_dse_point_t;

/* API */
void roofline_init(roofline_model_t *rm, const roof_accel_spec_t *spec);
void roofline_compute_ceilings(roofline_model_t *rm);
double roofline_get_ridge_point(const roofline_model_t *rm);

/* Ceiling management */
void roofline_add_ceiling(roofline_model_t *rm, roof_ceiling_type_t type,
                           const char *name, double gflops, bool is_mem);
double roofline_get_performance_bound(const roofline_model_t *rm,
                                       double operational_intensity);

/* Kernel analysis */
void roofline_add_point(roofline_model_t *rm, const char *name,
                         double ops, double bytes, double time_s);
void roofline_analyze_point(roofline_model_t *rm, roof_point_t *pt);
void roofline_remove_all_points(roofline_model_t *rm);

/* Bottleneck identification */
roof_ceiling_type_t roofline_identify_bottleneck(const roofline_model_t *rm,
                                                  const roof_point_t *pt);
const char *roofline_bottleneck_description(roof_ceiling_type_t bt);

/* Theoretical peak analysis */
double roofline_compute_peak_gflops(const roof_accel_spec_t *spec);
double roofline_compute_ridge_point(const roof_accel_spec_t *spec);

/* Amdahl's Law for accelerator */
double roofline_amdahl_speedup(double fraction_accelerated, double accelerator_speedup);
double roofline_max_speedup_given_time(double fraction_accelerated,
                                        double accelerator_time_ratio);

/* Energy modeling */
double roofline_estimate_energy(const roof_accel_spec_t *spec, uint64_t flops,
                                 uint64_t dram_bytes, uint64_t sram_bytes);

/* Design Space Exploration */
void roofline_dse_init(roofline_model_t *rm, roof_dse_point_t *points, uint32_t count);
void roofline_dse_evaluate(roofline_model_t *rm, roof_dse_point_t *points, uint32_t count,
                            double target_ops, double target_bytes);
void roofline_dse_find_optimal(const roof_dse_point_t *points, uint32_t count,
                                roof_dse_point_t *best, bool minimize_energy);

/* I/O: Print / report */
void roofline_print_model(const roofline_model_t *rm);
void roofline_print_point(const roof_point_t *pt);
void roofline_print_summary(const roofline_model_t *rm);

/* Ridge calculation utilities */
double roofline_bw_bound(double operational_intensity, double bw_gb_s);
double roofline_compute_bound(double operational_intensity, double peak_gflops);

#endif /* ACCEL_ROOFLINE_H */