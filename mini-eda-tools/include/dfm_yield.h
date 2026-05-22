#ifndef DFM_YIELD_H
#define DFM_YIELD_H

#include <stdint.h>
#include <stdbool.h>
#include "synthesis.h"
#include "place_route.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DRC_METAL_WIDTH,
    DRC_METAL_SPACE,
    DRC_VIA_ENCLOSURE,
    DRC_AREA,
    DRC_ANTENNA,
    DRC_WELL,
    DRC_DENSITY,
    DRC_OVERLAP,
    DRC_NOTCH,
    DRC_HOLE,
    DRC_CMP,
    DRC_LITHO,
    DRC_COUNT
} drc_type_e;

typedef enum {
    LITHO_OPTICAL, LITHO_OPC, LITHO_EUV, LITHO_DSA
} litho_type_e;

typedef enum {
    CMP_COPPER, CMP_OXIDE, CMP_TUNGSTEN
} cmp_material_e;

typedef struct {
    drc_type_e type;
    int        x, y;
    int        layer1, layer2;
    double     actual_val;
    double     required_val;
    char       description[256];
} drc_violation_t;

typedef struct {
    drc_violation_t *violations;
    int              num_violations;
    int              num_fatal;
    int              num_warning;
    double           total_score;
} drc_report_t;

typedef struct {
    double na;
    double wavelength_nm;
    double sigma;
    double defocus_nm;
    double dose;
    bool   use_opc;
    bool   use_sraf;
    double resist_thickness;
    double resist_sensitivity;
} litho_params_t;

typedef struct {
    double **aerial_image;
    double **latent_image;
    double **resist_profile;
    int     grid_x, grid_y;
    double  resolution_nm;
    double  contrast;
    double  depth_of_focus;
    double  process_window_area;
} litho_result_t;

typedef struct {
    cmp_material_e material;
    double         removal_rate_nm_s;
    double         pressure_psi;
    double         velocity_rpm;
    double         slurry_flow;
    double         polish_time_s;
    double         planarity_nm;
    double         dishing_nm;
    double         erosion_nm;
} cmp_params_t;

typedef struct {
    double **topography;
    double **pressure_map;
    double **removal_map;
    int      grid_x, grid_y;
    double   resolution_um;
    double   final_planarity;
    double   max_dishing;
    double   max_erosion;
    double   within_die_range;
} cmp_result_t;

typedef enum {
    YIELD_POISSON, YIELD_MURPHY, YIELD_SEEDS,
    YIELD_NEG_BINOMIAL, YIELD_BOSE
} yield_model_e;

typedef struct {
    yield_model_e model;
    double        defect_density;
    double        clustering_factor;
    double        critical_area_cm2;
    double        chip_area_cm2;
    double        systematic_yield;
    double        random_yield;
    double        parametric_yield;
} yield_params_t;

typedef struct {
    double total_yield;
    double random_defect_yield;
    double systematic_yield;
    double parametric_yield;
    double defect_limited_yield;
    double dpw;
    double good_die_per_wafer;
    double wafer_diameter_mm;
    double cost_per_good_die;
    int    num_critical_defects;
} yield_report_t;

typedef struct {
    double   min_width[16];
    double   min_space[16];
    double   min_enclosure[16];
    double   min_area[16];
    double   min_density[16];
    double   max_density[16];
    double   antenna_ratio[16];
    int      num_layers;
    double   via_size;
    double   via_space;
} design_rules_t;

typedef struct {
    double   max_antenna_ratio;
    double   metal_density_target;
    double   metal_density_window_um;
    double   cmp_dishing_target_nm;
    bool     use_redundant_vias;
    bool     use_wire_spreading;
    bool     use_opc;
    bool     use_cmp_fill;
    int      min_redundant_vias;
} dfm_params_t;

typedef struct {
    int    x, y;
    int    width, height;
    int    layer;
    double fill_density;
} metal_fill_t;

typedef struct {
    int    id;
    double ratio;
    bool   is_violation;
    double suggested_ratio;
} antenna_check_t;

bool    dfm_init_design_rules(design_rules_t *rules, int num_layers);
bool    dfm_set_layer_rule(design_rules_t *rules, int layer,
                           double min_w, double min_s,
                           double min_enc, double min_area);
void    dfm_print_rules(const design_rules_t *rules);

bool    dfm_run_drc(const netlist_t *nl, const design_rules_t *rules,
                    drc_report_t *report);
bool    dfm_run_drc_physical(const route_result_t *rr,
                             const design_rules_t *rules,
                             drc_report_t *report);
bool    dfm_check_metal_width(const route_result_t *rr, int net_id, int seg_id,
                              const design_rules_t *rules, drc_report_t *r);
bool    dfm_check_metal_space(const congestion_map_t *cmap, int x, int y,
                              int layer, const design_rules_t *rules,
                              drc_report_t *r);
bool    dfm_check_via_enclosure(const route_result_t *rr, int net_id,
                                int via_idx, const design_rules_t *rules,
                                drc_report_t *r);
bool    dfm_check_antenna(const route_result_t *rr, int net_id,
                          const design_rules_t *rules, drc_report_t *r);
bool    dfm_check_density(const congestion_map_t *cmap, int layer,
                          const design_rules_t *rules, drc_report_t *r);
void    dfm_free_drc_report(drc_report_t *report);

bool    dfm_run_litho_sim(const litho_params_t *params,
                          const double **mask, int mx, int my,
                          litho_result_t *result);
bool    dfm_opc_correction(const litho_params_t *params,
                           double **mask, int mx, int my);
bool    dfm_add_sraf(const litho_params_t *params,
                     double **mask, int mx, int my);
void    dfm_free_litho_result(litho_result_t *result);

bool    dfm_run_cmp_sim(const cmp_params_t *params,
                        const double **layout_density,
                        int gx, int gy, cmp_result_t *result);
bool    dfm_cmp_fill_insertion(const cmp_params_t *params,
                               const design_rules_t *rules,
                               const congestion_map_t *cmap,
                               metal_fill_t *fills, int *num_fills,
                               int max_fills);
bool    dfm_estimate_dishing(const cmp_params_t *params,
                             double density, double line_width,
                             double *dishing);
void    dfm_free_cmp_result(cmp_result_t *result);

bool    dfm_predict_yield(const yield_params_t *params,
                          yield_report_t *report);
double  dfm_poisson_yield(double defect_density, double critical_area);
double  dfm_murphy_yield(double defect_density, double critical_area);
double  dfm_seeds_yield(double defect_density, double critical_area);
double  dfm_neg_binomial_yield(double defect_density, double critical_area,
                               double clustering);
double  dfm_bose_einstein_yield(double defect_density, double critical_area,
                                int n_critical);

double  dfm_calc_critical_area(const congestion_map_t *cmap,
                               double defect_size);
int     dfm_calc_dpw(double wafer_diam, double die_w, double die_h,
                     double edge_exclusion);
double  dfm_cost_per_die(double wafer_cost, double good_die_per_wafer);

bool    dfm_run_redundant_via_insertion(route_result_t *rr,
                                        const design_rules_t *rules,
                                        double max_via_increase);
bool    dfm_run_wire_spreading(const congestion_map_t *cmap,
                               const design_rules_t *rules);
bool    dfm_run_metal_fill(const congestion_map_t *cmap,
                           const design_rules_t *rules,
                           const cmp_params_t *cmp,
                           metal_fill_t *fills, int *num_fills, int max);
bool    dfm_run_antenna_fix(route_result_t *rr,
                            const design_rules_t *rules);

bool    dfm_generate_report(const drc_report_t *drc,
                            const litho_result_t *litho,
                            const cmp_result_t *cmp,
                            const yield_report_t *yield,
                            const char *filename);
void    dfm_print_drc_report(const drc_report_t *report);
void    dfm_print_yield_report(const yield_report_t *report);
void    dfm_print_litho_result(const litho_result_t *result);
void    dfm_print_cmp_result(const cmp_result_t *result);

#ifdef __cplusplus
}
#endif
#endif /* DFM_YIELD_H */
