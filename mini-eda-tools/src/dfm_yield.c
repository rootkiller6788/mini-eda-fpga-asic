#include "dfm_yield.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *drc_names[] = {
    "METAL_WIDTH", "METAL_SPACE", "VIA_ENCLOSURE", "AREA",
    "ANTENNA", "WELL", "DENSITY", "OVERLAP", "NOTCH",
    "HOLE", "CMP", "LITHO"
};

bool dfm_init_design_rules(design_rules_t *rules, int num_layers) {
    if (!rules) return false;
    memset(rules, 0, sizeof(*rules));
    rules->num_layers = num_layers;
    for (int i = 0; i < num_layers && i < 16; i++) {
        rules->min_width[i] = 0.1;
        rules->min_space[i] = 0.1;
        rules->min_enclosure[i] = 0.05;
        rules->min_area[i] = 0.01;
        rules->min_density[i] = 0.1;
        rules->max_density[i] = 0.8;
        rules->antenna_ratio[i] = 400.0;
    }
    rules->via_size = 0.05;
    rules->via_space = 0.1;
    return true;
}

bool dfm_set_layer_rule(design_rules_t *rules, int layer,
                        double min_w, double min_s,
                        double min_enc, double min_area) {
    if (!rules || layer < 0 || layer >= rules->num_layers) return false;
    rules->min_width[layer] = min_w;
    rules->min_space[layer] = min_s;
    rules->min_enclosure[layer] = min_enc;
    rules->min_area[layer] = min_area;
    return true;
}

void dfm_print_rules(const design_rules_t *rules) {
    if (!rules) return;
    printf("Design Rules (%d layers):\n", rules->num_layers);
    for (int i = 0; i < rules->num_layers; i++) {
        printf("  Layer %d: w=%.2f s=%.2f enc=%.2f area=%.2f ant=%.0f\n",
               i, rules->min_width[i], rules->min_space[i],
               rules->min_enclosure[i], rules->min_area[i],
               rules->antenna_ratio[i]);
    }
}

bool dfm_run_drc(const netlist_t *nl, const design_rules_t *rules,
                 drc_report_t *report) {
    if (!nl || !rules || !report) return false;
    memset(report, 0, sizeof(*report));
    report->violations = NULL;
    report->num_violations = 0;
    report->num_warning = 0;
    report->num_fatal = 0;
    report->total_score = 0;
    return true;
}

bool dfm_run_drc_physical(const route_result_t *rr,
                          const design_rules_t *rules,
                          drc_report_t *report) {
    if (!rr || !rules || !report) return false;
    memset(report, 0, sizeof(*report));
    report->violations = NULL;
    report->num_violations = 0;
    for (int i = 0; i < rr->num_nets; i++) {
        for (int s = 0; s < rr->nets[i].num_segments; s++) {
            int ly = rr->nets[i].seg_layer[s];
            if (ly >= 0 && ly < rules->num_layers) {
                double w = 0.05;
                if (w < rules->min_width[ly]) {
                    report->num_violations++;
                    report->num_warning++;
                }
            }
        }
    }
    return true;
}

bool dfm_check_metal_width(const route_result_t *rr, int net_id, int seg_id,
                           const design_rules_t *rules, drc_report_t *r) {
    if (!rr || !rules || !r) return false;
    if (net_id < 0 || net_id >= rr->num_nets) return false;
    route_net_t *rn = &rr->nets[net_id];
    if (seg_id < 0 || seg_id >= rn->num_segments) return false;
    int ly = rn->seg_layer[seg_id];
    if (ly >= 0 && ly < rules->num_layers) {
        double w = 0.05;
        if (w < rules->min_width[ly]) {
            r->num_violations++;
        }
    }
    return true;
}

bool dfm_check_metal_space(const congestion_map_t *cmap, int x, int y,
                           int layer, const design_rules_t *rules,
                           drc_report_t *r) {
    if (!cmap || !rules || !r) return false;
    double density = pr_query_congestion(cmap, x, y, layer);
    if (density > 0.8 && density <= rules->max_density[layer]) {
        r->num_violations++;
        r->num_warning++;
    }
    return true;
}

bool dfm_check_via_enclosure(const route_result_t *rr, int net_id,
                             int via_idx, const design_rules_t *rules,
                             drc_report_t *r) {
    if (!rr || !rules || !r) return false;
    if (net_id < 0 || net_id >= rr->num_nets) return false;
    if (via_idx < 0 || via_idx >= rr->nets[net_id].num_vias) return false;
    return true;
}

bool dfm_check_antenna(const route_result_t *rr, int net_id,
                       const design_rules_t *rules, drc_report_t *r) {
    if (!rr || !rules || !r) return false;
    if (net_id < 0 || net_id >= rr->num_nets) return false;
    double ratio = rr->nets[net_id].wirelength_um / 0.05;
    int ly = 0;
    if (rr->nets[net_id].num_segments > 0)
        ly = rr->nets[net_id].seg_layer[0];
    if (ly >= 0 && ly < rules->num_layers &&
        ratio > rules->antenna_ratio[ly]) {
        r->num_violations++;
    }
    return true;
}

bool dfm_check_density(const congestion_map_t *cmap, int layer,
                       const design_rules_t *rules, drc_report_t *r) {
    if (!cmap || !rules || !r) return false;
    double avg_density = 0;
    int count = 0;
    for (int y = 0; y < cmap->grid_y; y++) {
        for (int x = 0; x < cmap->grid_x; x++) {
            avg_density += pr_query_congestion(cmap, x, y, layer);
            count++;
        }
    }
    if (count > 0) avg_density /= count;
    if (layer >= 0 && layer < rules->num_layers &&
        (avg_density < rules->min_density[layer] ||
         avg_density > rules->max_density[layer])) {
        r->num_violations++;
        r->num_fatal++;
    }
    return true;
}

void dfm_free_drc_report(drc_report_t *report) {
    if (!report) return;
    free(report->violations);
    memset(report, 0, sizeof(*report));
}

bool dfm_run_litho_sim(const litho_params_t *params,
                       const double **mask, int mx, int my,
                       litho_result_t *result) {
    if (!params || !mask || !result || mx <= 0 || my <= 0) return false;
    memset(result, 0, sizeof(*result));
    result->grid_x = mx;
    result->grid_y = my;
    result->resolution_nm = params->wavelength_nm * 0.5;
    result->contrast = 0.8;
    result->depth_of_focus = 100.0;
    result->process_window_area = 200.0;
    result->aerial_image = (double**)calloc(my, sizeof(double*));
    result->latent_image = (double**)calloc(my, sizeof(double*));
    result->resist_profile = (double**)calloc(my, sizeof(double*));
    for (int y = 0; y < my; y++) {
        result->aerial_image[y] = (double*)calloc(mx, sizeof(double));
        result->latent_image[y] = (double*)calloc(mx, sizeof(double));
        result->resist_profile[y] = (double*)calloc(mx, sizeof(double));
        for (int x = 0; x < mx; x++) {
            result->aerial_image[y][x] = mask[y][x];
            result->resist_profile[y][x] =
                mask[y][x] > 0.5 ? 1.0 : 0.0;
        }
    }
    return true;
}

bool dfm_opc_correction(const litho_params_t *params,
                        double **mask, int mx, int my) {
    if (!params || !mask) return false;
    for (int y = 0; y < my; y++) {
        for (int x = 0; x < mx; x++) {
            if (mask[y][x] > 0.5) {
                mask[y][x] *= 1.05;
                if (x > 0) mask[y][x-1] *= 0.95;
                if (x < mx-1) mask[y][x+1] *= 0.95;
                if (y > 0) mask[y-1][x] *= 0.95;
                if (y < my-1) mask[y+1][x] *= 0.95;
            }
        }
    }
    return true;
}

bool dfm_add_sraf(const litho_params_t *params,
                  double **mask, int mx, int my) {
    if (!params || !mask) return false;
    for (int y = 1; y < my - 1; y++) {
        for (int x = 1; x < mx - 1; x++) {
            if (mask[y][x] < 0.2) {
                int neighbors = 0;
                if (mask[y-1][x] > 0.5) neighbors++;
                if (mask[y+1][x] > 0.5) neighbors++;
                if (mask[y][x-1] > 0.5) neighbors++;
                if (mask[y][x+1] > 0.5) neighbors++;
                if (neighbors >= 2) mask[y][x] = 0.15;
            }
        }
    }
    return true;
}

void dfm_free_litho_result(litho_result_t *result) {
    if (!result) return;
    for (int i = 0; i < result->grid_y; i++) {
        free(result->aerial_image[i]);
        free(result->latent_image[i]);
        free(result->resist_profile[i]);
    }
    free(result->aerial_image);
    free(result->latent_image);
    free(result->resist_profile);
    memset(result, 0, sizeof(*result));
}

bool dfm_run_cmp_sim(const cmp_params_t *params,
                     const double **layout_density,
                     int gx, int gy, cmp_result_t *result) {
    if (!params || !layout_density || !result || gx <= 0 || gy <= 0)
        return false;
    memset(result, 0, sizeof(*result));
    result->grid_x = gx; result->grid_y = gy;
    result->resolution_um = 1.0;
    result->topography = (double**)calloc(gy, sizeof(double*));
    result->pressure_map = (double**)calloc(gy, sizeof(double*));
    result->removal_map = (double**)calloc(gy, sizeof(double*));
    for (int y = 0; y < gy; y++) {
        result->topography[y] = (double*)calloc(gx, sizeof(double));
        result->pressure_map[y] = (double*)calloc(gx, sizeof(double));
        result->removal_map[y] = (double*)calloc(gx, sizeof(double));
        for (int x = 0; x < gx; x++) {
            result->removal_map[y][x] =
                params->removal_rate_nm_s * (1.0 - layout_density[y][x] * 0.5);
            result->topography[y][x] = layout_density[y][x] * 10.0;
        }
    }
    result->final_planarity = 5.0;
    result->max_dishing = 15.0;
    result->max_erosion = 8.0;
    result->within_die_range = 20.0;
    return true;
}

bool dfm_cmp_fill_insertion(const cmp_params_t *params,
                            const design_rules_t *rules,
                            const congestion_map_t *cmap,
                            metal_fill_t *fills, int *num_fills,
                            int max_fills) {
    if (!params || !rules || !cmap || !fills || !num_fills) return false;
    int added = 0;
    for (int l = 0; l < cmap->num_layers && added < max_fills; l++) {
        for (int y = 0; y < cmap->grid_y && added < max_fills; y += 10) {
            for (int x = 0; x < cmap->grid_x && added < max_fills; x += 10) {
                double d = pr_query_congestion(cmap, x, y, l);
                if (d < rules->min_density[l]) {
                    fills[added].x = x; fills[added].y = y;
                    fills[added].width = 2; fills[added].height = 2;
                    fills[added].layer = l;
                    fills[added].fill_density = rules->min_density[l] - d;
                    added++;
                }
            }
        }
    }
    *num_fills = added;
    return true;
}

bool dfm_estimate_dishing(const cmp_params_t *params,
                          double density, double line_width,
                          double *dishing) {
    if (!params || !dishing) return false;
    *dishing = params->removal_rate_nm_s * density * line_width *
               params->polish_time_s * 0.01;
    return true;
}

void dfm_free_cmp_result(cmp_result_t *result) {
    if (!result) return;
    for (int i = 0; i < result->grid_y; i++) {
        free(result->topography[i]);
        free(result->pressure_map[i]);
        free(result->removal_map[i]);
    }
    free(result->topography);
    free(result->pressure_map);
    free(result->removal_map);
    memset(result, 0, sizeof(*result));
}

bool dfm_predict_yield(const yield_params_t *params, yield_report_t *report) {
    if (!params || !report) return false;
    memset(report, 0, sizeof(*report));
    double ca = params->critical_area_cm2;
    double dd = params->defect_density;
    switch (params->model) {
        case YIELD_POISSON:
            report->random_defect_yield = dfm_poisson_yield(dd, ca);
            break;
        case YIELD_MURPHY:
            report->random_defect_yield = dfm_murphy_yield(dd, ca);
            break;
        case YIELD_SEEDS:
            report->random_defect_yield = dfm_seeds_yield(dd, ca);
            break;
        case YIELD_NEG_BINOMIAL:
            report->random_defect_yield = dfm_neg_binomial_yield(dd, ca,
                params->clustering_factor);
            break;
        case YIELD_BOSE:
            report->random_defect_yield = dfm_bose_einstein_yield(dd, ca, 4);
            break;
    }
    report->systematic_yield = params->systematic_yield;
    report->parametric_yield = params->parametric_yield;
    report->defect_limited_yield = report->random_defect_yield;
    report->total_yield = report->random_defect_yield *
                          report->systematic_yield *
                          report->parametric_yield;
    report->wafer_diameter_mm = 300.0;
    report->dpw = dfm_calc_dpw(report->wafer_diameter_mm,
                                sqrt(params->chip_area_cm2) * 10.0,
                                sqrt(params->chip_area_cm2) * 10.0, 3.0);
    report->good_die_per_wafer = report->dpw * report->total_yield;
    report->cost_per_good_die = 5000.0 / (report->good_die_per_wafer + 0.001);
    return true;
}

double dfm_poisson_yield(double defect_density, double critical_area) {
    return exp(-defect_density * critical_area);
}

double dfm_murphy_yield(double defect_density, double critical_area) {
    double ad = defect_density * critical_area;
    if (ad < 0.001) return 1.0 - ad / 2.0;
    return (1.0 - exp(-ad)) / (ad);
}

double dfm_seeds_yield(double defect_density, double critical_area) {
    double ad = defect_density * critical_area;
    double result = 0;
    for (int i = 1; i <= 3; i++) {
        result += exp(-i * ad);
    }
    return result / 3.0;
}

double dfm_neg_binomial_yield(double defect_density, double critical_area,
                              double clustering) {
    double ad = defect_density * critical_area;
    if (clustering < 0.01) clustering = 0.01;
    return pow(1.0 + ad / clustering, -clustering);
}

double dfm_bose_einstein_yield(double defect_density, double critical_area,
                               int n_critical) {
    double ad = defect_density * critical_area;
    double yield = 1.0;
    for (int k = 0; k < n_critical; k++) {
        yield /= (1.0 + ad);
    }
    return yield;
}

double dfm_calc_critical_area(const congestion_map_t *cmap,
                              double defect_size) {
    if (!cmap) return 0;
    return cmap->grid_x * cmap->grid_y * defect_size * defect_size * 1e-8;
}

int dfm_calc_dpw(double wafer_diam, double die_w, double die_h,
                 double edge_exclusion) {
    double r = (wafer_diam - 2.0 * edge_exclusion) / 2.0;
    if (r <= 0 || die_w <= 0 || die_h <= 0) return 0;
    int nx = (int)(2.0 * r / die_w);
    int total = 0;
    for (int ix = 0; ix < nx; ix++) {
        double x = -r + ix * die_w + die_w / 2.0;
        double chord_y = sqrt(r * r - x * x) * 2.0;
        if (chord_y > 0) {
            int ny = (int)(chord_y / die_h);
            total += ny;
        }
    }
    return total;
}

double dfm_cost_per_die(double wafer_cost, double good_die_per_wafer) {
    if (good_die_per_wafer <= 0) return 1e12;
    return wafer_cost / good_die_per_wafer;
}

bool dfm_run_redundant_via_insertion(route_result_t *rr,
                                     const design_rules_t *rules,
                                     double max_via_increase) {
    if (!rr || !rules) return false;
    int added = 0;
    for (int i = 0; i < rr->num_nets; i++) {
        if (added < (int)(rr->num_nets * max_via_increase)) {
            pr_add_redundant_via(rr, i);
            added++;
        }
    }
    return true;
}

bool dfm_run_wire_spreading(const congestion_map_t *cmap,
                            const design_rules_t *rules) {
    (void)cmap; (void)rules;
    return true;
}

bool dfm_run_metal_fill(const congestion_map_t *cmap,
                        const design_rules_t *rules,
                        const cmp_params_t *cmp,
                        metal_fill_t *fills, int *num_fills, int max) {
    return dfm_cmp_fill_insertion(cmp, rules, cmap, fills, num_fills, max);
}

bool dfm_run_antenna_fix(route_result_t *rr, const design_rules_t *rules) {
    if (!rr || !rules) return false;
    int fixed = 0;
    for (int i = 0; i < rr->num_nets; i++) {
        if (!pr_check_antenna(rr, i)) {
            fixed++;
        }
    }
    return true;
}

bool dfm_generate_report(const drc_report_t *drc,
                         const litho_result_t *litho,
                         const cmp_result_t *cmp,
                         const yield_report_t *yield,
                         const char *filename) {
    return true;
}

void dfm_print_drc_report(const drc_report_t *report) {
    if (!report) return;
    printf("DRC Report:\n");
    printf("  Violations: %d (fatal=%d, warning=%d)\n",
           report->num_violations, report->num_fatal, report->num_warning);
    printf("  Score: %.2f\n", report->total_score);
}

void dfm_print_yield_report(const yield_report_t *report) {
    if (!report) return;
    printf("Yield Report:\n");
    printf("  Total:       %.4f%%\n", report->total_yield * 100.0);
    printf("  Random:      %.4f%%\n", report->random_defect_yield * 100.0);
    printf("  Systematic:  %.4f%%\n", report->systematic_yield * 100.0);
    printf("  DPW:         %.0f\n", report->dpw);
    printf("  Good dies:   %.1f\n", report->good_die_per_wafer);
    printf("  Cost/die:    $%.2f\n", report->cost_per_good_die);
}

void dfm_print_litho_result(const litho_result_t *result) {
    if (!result) return;
    printf("Litho Simulation:\n");
    printf("  Grid: %dx%d  Resolution: %.2f nm\n",
           result->grid_x, result->grid_y, result->resolution_nm);
    printf("  Contrast: %.2f  DOF: %.2f nm\n",
           result->contrast, result->depth_of_focus);
}

void dfm_print_cmp_result(const cmp_result_t *result) {
    if (!result) return;
    printf("CMP Simulation:\n");
    printf("  Grid: %dx%d  Planarity: %.2f nm\n",
           result->grid_x, result->grid_y, result->final_planarity);
    printf("  Dishing: %.2f nm  Erosion: %.2f nm\n",
           result->max_dishing, result->max_erosion);
}
