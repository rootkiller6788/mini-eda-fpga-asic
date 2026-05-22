#ifndef POWER_ANALYSIS_H
#define POWER_ANALYSIS_H

#include <stdint.h>
#include <stdbool.h>
#include "synthesis.h"
#include "static_timing.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    POW_DYNAMIC_SWITCH,
    POW_DYNAMIC_SC,
    POW_STATIC_LEAK,
    POW_CLOCK_TREE,
    POW_IO,
    POW_TOTAL
} power_component_e;

typedef enum {
    VT_LOW, VT_STD, VT_HIGH, VT_ULTRA
} vt_level_e;

typedef enum {
    CG_MANUAL, CG_AUTO_XOR, CG_AUTO_AND,
    CG_AUTO_ICG, CG_NONE
} clock_gating_e;

typedef struct {
    double  static_prob;
    double  toggle_rate;
    double  alpha;
    int     signal_id;
    bool    is_clock;
} switching_activity_t;

typedef struct {
    switching_activity_t *activities;
    int                   num_activities;
    double                default_toggle_rate;
    double                default_static_prob;
    double                clock_prob;
} activity_file_t;

typedef struct {
    double dynamic_switching_mw;
    double dynamic_short_circuit_mw;
    double static_leakage_mw;
    double clock_tree_mw;
    double io_mw;
    double total_mw;
    double peak_mw;
    double avg_mw;
} power_report_t;

typedef struct {
    double          vdd_v;
    double          frequency_mhz;
    double          temperature_c;
    double          c_load_total_ff;
    double          alpha_avg;
    double          leakage_per_gate_pw;
    vt_level_e      default_vt;
    bool            enable_clock_gating;
    bool            enable_power_gating;
    bool            enable_multi_vt;
    bool            enable_dvfs;
    double          vdd_standby_v;
} power_params_t;

typedef struct {
    int        gate_id;
    double     dynamic_power_uw;
    double     static_power_uw;
    double     alpha;
    double     cap_load;
    vt_level_e vt;
    bool       is_clock_gated;
    bool       is_power_gated;
    int        power_domain;
} gate_power_t;

typedef struct {
    gate_power_t   *gate_powers;
    int             num_gates;
    power_report_t  report;
    power_params_t  params;
    double          total_energy_pj;
    double          energy_per_cycle_pj;
} power_analysis_t;

typedef struct {
    int    id;
    char   name[64];
    bool   is_active;
    double sleep_power_mw;
} power_domain_t;

bool    paw_create_analysis(power_analysis_t *analysis, const netlist_t *nl);
void    paw_free_analysis(power_analysis_t *analysis);

bool    paw_load_activity_file(activity_file_t *act, const char *filename);
bool    paw_set_activity(activity_file_t *act, int signal_id,
                         double static_prob, double toggle_rate);
bool    paw_estimate_activity(activity_file_t *act, const netlist_t *nl);
void    paw_free_activity(activity_file_t *act);

bool    paw_calc_dynamic_power(power_analysis_t *analysis,
                               const netlist_t *nl,
                               const tech_lib_t *lib,
                               const activity_file_t *act);
double  paw_switching_power(double cap_load, double vdd, double freq,
                            double alpha);
double  paw_short_circuit_power(double avg_current, double vdd);
double  paw_internal_power(const std_cell_t *cell, double slew_in,
                           double cap_out, edge_type_e edge);

bool    paw_calc_static_power(power_analysis_t *analysis,
                              const netlist_t *nl,
                              const tech_lib_t *lib);
double  paw_gate_leakage(const std_cell_t *cell, double vdd, double temp);
double  paw_subthreshold_leakage(double vth, double vdd, double temp);
double  paw_gate_leakage_current(double tox, double area);

bool    paw_calc_clock_power(power_analysis_t *analysis,
                             const netlist_t *nl,
                             const tech_lib_t *lib,
                             const sta_result_t *sta);

bool    paw_run_analysis(power_analysis_t *analysis,
                         const netlist_t *nl,
                         const tech_lib_t *lib,
                         const activity_file_t *act,
                         const sta_result_t *sta);
bool    paw_generate_report(const power_analysis_t *analysis,
                            power_report_t *report);
void    paw_print_report(const power_report_t *report);

bool    paw_apply_clock_gating(power_analysis_t *analysis,
                               netlist_t *nl,
                               clock_gating_e mode);
bool    paw_apply_multi_vt(power_analysis_t *analysis,
                           netlist_t *nl,
                           const tech_lib_t *lib,
                           double max_delay_ps);
bool    paw_apply_power_gating(power_analysis_t *analysis,
                               netlist_t *nl,
                               power_domain_t *domains,
                               int num_domains);
bool    paw_apply_gate_sizing(power_analysis_t *analysis,
                              netlist_t *nl,
                              const tech_lib_t *lib);

double  paw_total_dynamic(const power_report_t *report);
double  paw_total_static(const power_report_t *report);
double  paw_energy_per_op(const power_analysis_t *analysis,
                          double frequency_mhz);

#ifdef __cplusplus
}
#endif
#endif /* POWER_ANALYSIS_H */
