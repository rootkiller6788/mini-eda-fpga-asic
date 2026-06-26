/* accel_roofline.c - Roofline model for AI accelerator */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "accel_roofline.h"

void roofline_init(roofline_model_t *rm, const roof_accel_spec_t *spec) {
    if (!rm) return;
    memset(rm, 0, sizeof(*rm));
    if (spec) rm->spec = *spec;
}

double roofline_compute_peak_gflops(const roof_accel_spec_t *spec) {
    if (!spec) return 0.0;
    return (double)spec->num_mac_units * 2.0 * spec->clock_ghz * (double)spec->num_cores;
}

double roofline_compute_ridge_point(const roof_accel_spec_t *spec) {
    if (!spec) return 0.0;
    double peak = roofline_compute_peak_gflops(spec);
    double bw = spec->peak_dram_bw_gb_s;
    if (bw <= 0) return 1e308;
    return peak / bw;
}

double roofline_get_ridge_point(const roofline_model_t *rm) {
    if (!rm) return 0.0;
    return roofline_compute_ridge_point(&rm->spec);
}

void roofline_add_ceiling(roofline_model_t *rm, roof_ceiling_type_t type,
                           const char *name, double gflops, bool is_mem) {
    if (!rm || rm->ceiling_count >= ROOFLINE_MAX_CEILINGS) return;
    roof_ceiling_t *c = &rm->ceilings[rm->ceiling_count++];
    c->type = type;
    if (name) strncpy(c->name, name, sizeof(c->name) - 1);
    c->value_gflops = gflops;
    c->is_memory_bound = is_mem;
}

void roofline_compute_ceilings(roofline_model_t *rm) {
    if (!rm) return;
    rm->ceiling_count = 0;
    double peak = roofline_compute_peak_gflops(&rm->spec);
    roofline_add_ceiling(rm, ROOF_PEAK_COMPUTE, "Peak Compute", peak, false);
    roofline_add_ceiling(rm, ROOF_SIMD_LIMITED, "SIMD Limited", peak * 0.75, false);
    roofline_add_ceiling(rm, ROOF_FMA_LIMITED, "FMA Utilization", peak * 0.80, false);
    roofline_add_ceiling(rm, ROOF_ILP_LIMITED, "ILP Limited", peak * 0.60, false);
    roofline_add_ceiling(rm, ROOF_BRANCH_LIMITED, "Branch Limited", peak * 0.85, false);
    roofline_add_ceiling(rm, ROOF_SPARSITY_OVERHEAD, "Sparsity Overhead", peak * 0.70, false);
    roofline_add_ceiling(rm, ROOF_PRECISION_CASTING, "Precision Casting", peak * 0.90, false);
    roofline_add_ceiling(rm, ROOF_MEM_BW, "DRAM Bandwidth", 0.0, true);
    roofline_add_ceiling(rm, ROOF_L2_BW, "L2 Buffer BW", 0.0, true);
    roofline_add_ceiling(rm, ROOF_L1_BW, "L1 Buffer BW", 0.0, true);
}

double roofline_get_performance_bound(const roofline_model_t *rm, double ob) {
    if (!rm) return 1e308;
    double minb = 1e308;
    uint32_t i;
    for (i = 0; i < rm->ceiling_count; i++) {
        const roof_ceiling_t *c = &rm->ceilings[i];
        double bound;
        if (c->is_memory_bound) {
            double bw;
            if (c->type == ROOF_MEM_BW) bw = rm->spec.peak_dram_bw_gb_s;
            else if (c->type == ROOF_L2_BW) bw = rm->spec.peak_sram_bw_gb_s;
            else bw = rm->spec.peak_sram_bw_gb_s * 1.5;
            bound = ob * bw;
        } else {
            bound = c->value_gflops;
        }
        if (bound < minb) minb = bound;
    }
    return minb;
}

void roofline_add_point(roofline_model_t *rm, const char *name,
                         double ops, double bytes, double time_s) {
    if (!rm || rm->point_count >= ROOFLINE_MAX_POINTS) return;
    roof_point_t *pt = &rm->points[rm->point_count++];
    memset(pt, 0, sizeof(*pt));
    if (name) strncpy(pt->kernel_name, name, sizeof(pt->kernel_name) - 1);
    pt->total_flops = (uint64_t)ops;
    pt->total_bytes = (uint64_t)bytes;
    if (bytes > 0 && time_s > 0) {
        pt->operational_intensity = ops / bytes;
        pt->performance_gflops = (ops / time_s) / 1e9;
    }
    roofline_analyze_point(rm, pt);
}

void roofline_analyze_point(roofline_model_t *rm, roof_point_t *pt) {
    if (!rm || !pt) return;
    double ridge = roofline_get_ridge_point(rm);
    pt->compute_bound = (pt->operational_intensity >= ridge);
    if (pt->operational_intensity > 1e-10 && ridge > 1e-10)
        pt->ridge_distance = log10(pt->operational_intensity) - log10(ridge);
    double peak = roofline_compute_peak_gflops(&rm->spec);
    if (peak > 0) pt->utilization = pt->performance_gflops / peak;
}

void roofline_remove_all_points(roofline_model_t *rm) {
    if (rm) rm->point_count = 0;
}

roof_ceiling_type_t roofline_identify_bottleneck(const roofline_model_t *rm,
                                                  const roof_point_t *pt) {
    if (!rm || !pt) return ROOF_PEAK_COMPUTE;
    double minb = 1e308;
    roof_ceiling_type_t bottleneck = ROOF_PEAK_COMPUTE;
    uint32_t i;
    for (i = 0; i < rm->ceiling_count; i++) {
        const roof_ceiling_t *c = &rm->ceilings[i];
        double bound = c->is_memory_bound
            ? pt->operational_intensity * (
                (c->type == ROOF_MEM_BW) ? rm->spec.peak_dram_bw_gb_s :
                (c->type == ROOF_L2_BW) ? rm->spec.peak_sram_bw_gb_s :
                rm->spec.peak_sram_bw_gb_s * 1.5)
            : c->value_gflops;
        if (bound < minb) { minb = bound; bottleneck = c->type; }
    }
    return bottleneck;
}

const char *roofline_bottleneck_description(roof_ceiling_type_t bt) {
    switch (bt) {
    case ROOF_PEAK_COMPUTE:    return "Peak compute limited";
    case ROOF_ILP_LIMITED:     return "ILP limited";
    case ROOF_SIMD_LIMITED:    return "SIMD width limited";
    case ROOF_FMA_LIMITED:     return "FMA utilization limited";
    case ROOF_BRANCH_LIMITED:  return "Branch divergence limited";
    case ROOF_SPARSITY_OVERHEAD: return "Sparsity overhead";
    case ROOF_PRECISION_CASTING: return "Precision casting overhead";
    case ROOF_MEM_BW:          return "DRAM bandwidth limited";
    case ROOF_L2_BW:           return "L2 buffer BW limited";
    case ROOF_L1_BW:           return "L1 buffer BW limited";
    default:                   return "Unknown bottleneck";
    }
}

double roofline_amdahl_speedup(double f, double s) {
    if (f <= 0) return 1.0;
    if (f >= 1.0) return s;
    return 1.0 / ((1.0 - f) + f / s);
}

double roofline_max_speedup_given_time(double f, double r) {
    (void)r;
    if (f <= 0) return 1.0;
    return 1.0 / (1.0 - f);
}

double roofline_estimate_energy(const roof_accel_spec_t *spec, uint64_t flops,
                                 uint64_t dram_bytes, uint64_t sram_bytes) {
    if (!spec) return 0.0;
    return (double)flops * 0.9 + (double)dram_bytes * 200.0 + (double)sram_bytes * 5.0;
}

void roofline_dse_init(roofline_model_t *rm, roof_dse_point_t *points, uint32_t count) {
    if (!rm || !points) return;
    rm->point_count = 0;
    uint32_t i;
    for (i = 0; i < count && i < ROOFLINE_MAX_POINTS; i++)
        memset(&points[i], 0, sizeof(roof_dse_point_t));
}

void roofline_dse_evaluate(roofline_model_t *rm, roof_dse_point_t *points, uint32_t count,
                            double target_ops, double target_bytes) {
    if (!rm || !points) return;
    uint32_t i;
    for (i = 0; i < count; i++) {
        roof_dse_point_t *dp = &points[i];
        if (dp->array_rows > 0 && dp->array_cols > 0 && dp->clock_ghz > 0) {
            double pk = (double)dp->array_rows * (double)dp->array_cols * 2.0 * dp->clock_ghz / 1000.0;
            double sf = (dp->sram_kb > 0) ? fmin(1.0, dp->sram_kb / 512.0) : 0.5;
            double ob = (target_bytes > 0) ? target_ops / target_bytes : 100.0;
            dp->projected_tops = fmin(pk, dp->dram_bw_gb_s * ob) * sf;
            dp->projected_utilization = (pk > 0) ? dp->projected_tops / pk : 0.0;
            double sp = dp->sram_kb * 0.001;
            double dpw = (double)dp->array_rows * (double)dp->array_cols * dp->clock_ghz * 1e-9 * 0.8;
            double tp = sp + dpw;
            if (tp > 0) dp->energy_efficiency_tops_per_w = dp->projected_tops / tp;
            dp->area_mm2_est = (double)dp->array_rows * (double)dp->array_cols * 0.001 + dp->sram_kb * 0.002;
        }
        if (dp->energy_efficiency_tops_per_w > 0 && dp->area_mm2_est > 0) {
            double tpm2 = dp->projected_tops / dp->area_mm2_est;
            dp->cost_score = 1.0 / (dp->energy_efficiency_tops_per_w * tpm2);
        }
    }
}

void roofline_dse_find_optimal(const roof_dse_point_t *points, uint32_t count,
                                roof_dse_point_t *best, bool minimize_energy) {
    if (!points || !best || count == 0) return;
    *best = points[0];
    uint32_t i;
    for (i = 1; i < count; i++) {
        bool better = minimize_energy
            ? (points[i].energy_efficiency_tops_per_w > best->energy_efficiency_tops_per_w)
            : (points[i].projected_tops > best->projected_tops);
        if (better) *best = points[i];
    }
}

double roofline_bw_bound(double ob, double bw) { return ob * bw; }
double roofline_compute_bound(double ob, double pk) { (void)ob; return pk; }

void roofline_print_model(const roofline_model_t *rm) {
    if (!rm) return;
    printf("=== Roofline Model ===\n");
    printf("  Peak compute: %.2f GFLOP/s\n", roofline_compute_peak_gflops(&rm->spec));
    printf("  Ridge point:  %.2f FLOP/byte\n", roofline_get_ridge_point(rm));
    printf("  MAC: %u  Clock: %.2f GHz  Cores: %u\n", rm->spec.num_mac_units, rm->spec.clock_ghz, rm->spec.num_cores);
    printf("  SRAM BW: %.1f GB/s  DRAM BW: %.1f GB/s\n", rm->spec.peak_sram_bw_gb_s, rm->spec.peak_dram_bw_gb_s);
    printf("  Ceilings (%u):\n", rm->ceiling_count);
    uint32_t i;
    for (i = 0; i < rm->ceiling_count; i++) {
        printf("    [%c] %-25s %8.2f GFLOP/s\n", rm->ceilings[i].is_memory_bound ? 'M' : 'C', rm->ceilings[i].name, rm->ceilings[i].value_gflops);
    }
    printf("  Kernel points (%u):\n", rm->point_count);
    for (i = 0; i < rm->point_count; i++) {
        printf("    %-25s I=%.2f P=%.2f GFLOP/s %s\n", rm->points[i].kernel_name, rm->points[i].operational_intensity, rm->points[i].performance_gflops, rm->points[i].compute_bound ? "[compute-bound]" : "[memory-bound]");
    }
}
