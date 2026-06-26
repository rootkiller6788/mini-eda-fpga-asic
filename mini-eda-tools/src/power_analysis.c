#include "power_analysis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

bool paw_create_analysis(power_analysis_t *analysis, const netlist_t *nl) {
    if (!analysis || !nl) return false;
    memset(analysis, 0, sizeof(*analysis));
    analysis->num_gates = nl->num_gates;
    analysis->gate_powers = (gate_power_t*)calloc(analysis->num_gates,
                                                  sizeof(gate_power_t));
    analysis->params.vdd_v = 1.0;
    analysis->params.frequency_mhz = 100.0;
    analysis->params.temperature_c = 25.0;
    analysis->params.alpha_avg = 0.1;
    analysis->params.leakage_per_gate_pw = 1.0;
    analysis->params.default_vt = VT_STD;
    for (int i = 0; i < analysis->num_gates; i++) {
        analysis->gate_powers[i].gate_id = i;
        analysis->gate_powers[i].alpha = 0.1;
        analysis->gate_powers[i].cap_load = 1.0;
        analysis->gate_powers[i].vt = VT_STD;
        analysis->gate_powers[i].is_clock_gated = false;
        analysis->gate_powers[i].is_power_gated = false;
        analysis->gate_powers[i].power_domain = 0;
    }
    return true;
}

void paw_free_analysis(power_analysis_t *analysis) {
    if (!analysis) return;
    free(analysis->gate_powers);
    memset(analysis, 0, sizeof(*analysis));
}

bool paw_load_activity_file(activity_file_t *act, const char *filename) {
    if (!act) return false;
    memset(act, 0, sizeof(*act));
    act->default_toggle_rate = 0.1;
    act->default_static_prob = 0.5;
    act->clock_prob = 0.5;
    act->num_activities = 10;
    act->activities = (switching_activity_t*)calloc(
        act->num_activities, sizeof(switching_activity_t));
    for (int i = 0; i < act->num_activities; i++) {
        act->activities[i].signal_id = i;
        act->activities[i].static_prob = 0.3 + i * 0.05;
        act->activities[i].toggle_rate = 0.1 + i * 0.02;
        act->activities[i].alpha = act->activities[i].toggle_rate * 0.5;
        act->activities[i].is_clock = (i == 0);
    }
    return true;
}

bool paw_set_activity(activity_file_t *act, int signal_id,
                      double static_prob, double toggle_rate) {
    if (!act) return false;
    for (int i = 0; i < act->num_activities; i++) {
        if (act->activities[i].signal_id == signal_id) {
            act->activities[i].static_prob = static_prob;
            act->activities[i].toggle_rate = toggle_rate;
            act->activities[i].alpha = toggle_rate * 0.5;
            return true;
        }
    }
    act->num_activities++;
    act->activities = (switching_activity_t*)realloc(act->activities,
        act->num_activities * sizeof(switching_activity_t));
    switching_activity_t *sa = &act->activities[act->num_activities - 1];
    sa->signal_id = signal_id;
    sa->static_prob = static_prob;
    sa->toggle_rate = toggle_rate;
    sa->alpha = toggle_rate * 0.5;
    sa->is_clock = false;
    return true;
}

bool paw_estimate_activity(activity_file_t *act, const netlist_t *nl) {
    if (!act || !nl) return false;
    for (int i = 0; i < nl->num_gates; i++) {
        if (i >= act->num_activities) {
            paw_set_activity(act, i, 0.5, 0.1);
        }
    }
    return true;
}

void paw_free_activity(activity_file_t *act) {
    if (!act) return;
    free(act->activities);
    memset(act, 0, sizeof(*act));
}

double paw_switching_power(double cap_load, double vdd, double freq,
                           double alpha) {
    return alpha * cap_load * vdd * vdd * freq * 1e-6;
}

double paw_short_circuit_power(double avg_current, double vdd) {
    return avg_current * vdd * 1e-3;
}

double paw_internal_power(const std_cell_t *cell, double slew_in,
                          double cap_out, edge_type_e edge) {
    if (!cell) return 0.01;
    double e_rise = 0.5 * cell->cap_input_ff * 0.001 * 1.0 * 1.0;
    double e_fall = 0.5 * cell->cap_input_ff * 0.001 * 1.0 * 1.0;
    return (e_rise + e_fall) * 0.5;
}

double paw_gate_leakage(const std_cell_t *cell, double vdd, double temp) {
    if (!cell) return 1.0;
    double t_factor = exp((temp - 25.0) / 50.0);
    return cell->leakage_pw * t_factor;
}

double paw_subthreshold_leakage(double vth, double vdd, double temp) {
    double kT_q = 0.0259;
    return 1.0 * exp(-vth / (kT_q * 1.5)) * (vdd / 1.0);
}

double paw_gate_leakage_current(double tox, double area) {
    return area / (tox * tox + 0.01) * 1e-9;
}

bool paw_calc_dynamic_power(power_analysis_t *analysis,
                            const netlist_t *nl, const tech_lib_t *lib,
                            const activity_file_t *act) {
    if (!analysis || !nl) return false;
    for (int i = 0; i < analysis->num_gates; i++) {
        double alpha = analysis->gate_powers[i].alpha;
        if (act && i < act->num_activities)
            alpha = act->activities[i].alpha;
        double cap = analysis->gate_powers[i].cap_load;
        double vdd = analysis->params.vdd_v;
        double freq = analysis->params.frequency_mhz;
        analysis->gate_powers[i].dynamic_power_uw =
            paw_switching_power(cap, vdd, freq, alpha);
    }
    return true;
}

bool paw_calc_static_power(power_analysis_t *analysis, const netlist_t *nl,
                           const tech_lib_t *lib) {
    if (!analysis || !nl) return false;
    for (int i = 0; i < analysis->num_gates; i++) {
        double leak = analysis->params.leakage_per_gate_pw;
        if (lib && i < lib->num_cells)
            leak = lib->cells[i].leakage_pw;
        double temp_factor = exp((analysis->params.temperature_c - 25.0) / 50.0);
        analysis->gate_powers[i].static_power_uw = leak * temp_factor * 1e-3;
    }
    return true;
}

bool paw_calc_clock_power(power_analysis_t *analysis, const netlist_t *nl,
                          const tech_lib_t *lib, const sta_result_t *sta) {
    if (!analysis || !nl) return false;
    double total_clock_cap = 0;
    for (int i = 0; i < nl->num_gates; i++) {
        if (nl->gates[i].is_clock || nl->gates[i].type == GATE_DFF)
            total_clock_cap += 2.0;
    }
    analysis->report.clock_tree_mw =
        0.5 * total_clock_cap * analysis->params.vdd_v *
        analysis->params.vdd_v * analysis->params.frequency_mhz * 1e-3;
    return true;
}

bool paw_run_analysis(power_analysis_t *analysis, const netlist_t *nl,
                      const tech_lib_t *lib, const activity_file_t *act,
                      const sta_result_t *sta) {
    if (!analysis || !nl) return false;
    paw_calc_dynamic_power(analysis, nl, lib, act);
    paw_calc_static_power(analysis, nl, lib);
    paw_calc_clock_power(analysis, nl, lib, sta);
    paw_generate_report(analysis, &analysis->report);
    return true;
}

bool paw_generate_report(const power_analysis_t *analysis,
                         power_report_t *report) {
    if (!analysis || !report) return false;
    memset(report, 0, sizeof(*report));
    for (int i = 0; i < analysis->num_gates; i++) {
        report->dynamic_switching_mw +=
            analysis->gate_powers[i].dynamic_power_uw * 0.001;
        report->static_leakage_mw +=
            analysis->gate_powers[i].static_power_uw * 0.001;
    }
    report->dynamic_short_circuit_mw =
        report->dynamic_switching_mw * 0.15;
    report->io_mw = report->dynamic_switching_mw * 0.05;
    report->total_mw = report->dynamic_switching_mw +
                       report->dynamic_short_circuit_mw +
                       report->static_leakage_mw +
                       report->clock_tree_mw +
                       report->io_mw;
    report->peak_mw = report->total_mw * 1.5;
    report->avg_mw = report->total_mw * 0.7;
    return true;
}

void paw_print_report(const power_report_t *report) {
    if (!report) return;
    printf("Power Report:\n");
    printf("  Dynamic (switching):    %.4f mW\n", report->dynamic_switching_mw);
    printf("  Dynamic (short-circuit): %.4f mW\n", report->dynamic_short_circuit_mw);
    printf("  Static (leakage):       %.4f mW\n", report->static_leakage_mw);
    printf("  Clock Tree:             %.4f mW\n", report->clock_tree_mw);
    printf("  IO:                     %.4f mW\n", report->io_mw);
    printf("  Total:                  %.4f mW\n", report->total_mw);
    printf("  Peak:                   %.4f mW\n", report->peak_mw);
}

bool paw_apply_clock_gating(power_analysis_t *analysis, netlist_t *nl,
                            clock_gating_e mode) {
    if (!analysis || !nl) return false;
    int gated = 0;
    for (int i = 0; i < analysis->num_gates; i++) {
        if (!analysis->gate_powers[i].is_clock_gated &&
            analysis->gate_powers[i].alpha > 0.05) {
            analysis->gate_powers[i].is_clock_gated = true;
            analysis->gate_powers[i].alpha *= 0.3;
            gated++;
        }
    }
    return true;
}

bool paw_apply_multi_vt(power_analysis_t *analysis, netlist_t *nl,
                        const tech_lib_t *lib, double max_delay_ps) {
    if (!analysis || !nl) return false;
    int swapped = 0;
    for (int i = 0; i < analysis->num_gates; i++) {
        if (analysis->gate_powers[i].alpha < 0.05 &&
            analysis->gate_powers[i].vt > VT_HIGH) {
            analysis->gate_powers[i].vt = VT_HIGH;
            analysis->gate_powers[i].static_power_uw *= 0.3;
            swapped++;
        }
    }
    return true;
}

bool paw_apply_power_gating(power_analysis_t *analysis, netlist_t *nl,
                            power_domain_t *domains, int num_domains) {
    if (!analysis || !nl || !domains) return false;
    for (int d = 0; d < num_domains; d++) {
        domains[d].sleep_power_mw = analysis->report.static_leakage_mw * 0.1;
        int gates_in_domain = 0;
        for (int i = 0; i < analysis->num_gates; i++) {
            if (analysis->gate_powers[i].power_domain == domains[d].id) {
                analysis->gate_powers[i].is_power_gated = !domains[d].is_active;
                gates_in_domain++;
            }
        }
    }
    return true;
}

bool paw_apply_gate_sizing(power_analysis_t *analysis, netlist_t *nl,
                           const tech_lib_t *lib) {
    if (!analysis || !nl || !lib) return false;
    int resized = 0;
    for (int i = 0; i < analysis->num_gates; i++) {
        if (analysis->gate_powers[i].dynamic_power_uw > 10.0) {
            analysis->gate_powers[i].dynamic_power_uw *= 0.8;
            analysis->gate_powers[i].static_power_uw *= 0.9;
            resized++;
        }
    }
    return true;
}

double paw_total_dynamic(const power_report_t *report) {
    return report ? report->dynamic_switching_mw +
                    report->dynamic_short_circuit_mw : 0;
}

double paw_total_static(const power_report_t *report) {
    return report ? report->static_leakage_mw : 0;
}

double paw_energy_per_op(const power_analysis_t *analysis, double freq_mhz) {
    if (!analysis || freq_mhz <= 0) return 0;
    return analysis->report.total_mw / freq_mhz * 1e3;
}
