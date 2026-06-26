#include "thermal_power.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void tp_init(thermal_power_model_t *tm,
             double die_area_mm2, double ambient_temp_c) {
    if (!tm) return;
    memset(tm, 0, sizeof(*tm));
    tm->die_area_mm2 = die_area_mm2;
    tm->state.ambient_temp_c = ambient_temp_c;
    tm->state.die_temp_c[0] = ambient_temp_c;
    tm->state.junction_temp_c = ambient_temp_c;
    tm->state.case_temp_c = ambient_temp_c;
    tm->state.total_power_w = 0.0;
    tm->state.cooling_capacity_w = 100.0;
    tm->state.thermal_margin_c = 50.0;
    tm->state.throttling = 0;
    tm->theta.theta_ja = 25.0;
    tm->theta.theta_jc = 0.5;
    tm->theta.theta_jb = 2.0;
    tm->theta.theta_ca = 0.5;
    tm->theta.ambient_temp_c = ambient_temp_c;
    tm->die_thickness_um = 750.0;
    tm->power_density_w_per_mm2 = 0.0;
    tm->three_d_stack = 0;
    tm->stack_layers = 1;

    tp_pdn_init(&tm->pdn, 0.85, 50.0);
}

void tp_set_thermal_resistance(thermal_power_model_t *tm,
                                double theta_ja, double theta_jc) {
    if (!tm) return;
    tm->theta.theta_ja = theta_ja;
    tm->theta.theta_jc = theta_jc;
}

void tp_set_tim(thermal_power_model_t *tm, const tim_spec_t *tim) {
    if (!tm || !tim) return;
    memcpy(&tm->tim, tim, sizeof(tim_spec_t));
    tm->tim.thermal_resistance_c_per_w =
        (tim->bondline_thickness_um * 1e-6) /
        (tim->thermal_conductivity * tm->die_area_mm2 * 1e-6);
}

int tp_add_hotspot(thermal_power_model_t *tm, const hotspot_t *hs) {
    if (!tm || !hs) return -1;
    if (tm->num_hotspots >= THERMAL_MAX_HOTSPOTS) return -2;
    memcpy(&tm->hotspots[tm->num_hotspots], hs, sizeof(hotspot_t));
    tm->hotspots[tm->num_hotspots].active = 1;
    tm->num_hotspots++;
    return 0;
}

int tp_remove_hotspot(thermal_power_model_t *tm, uint32_t index) {
    if (!tm || index >= tm->num_hotspots) return -1;
    for (uint32_t i = index; i < tm->num_hotspots - 1; i++)
        memcpy(&tm->hotspots[i], &tm->hotspots[i + 1], sizeof(hotspot_t));
    tm->num_hotspots--;
    return 0;
}

double tp_calc_junction_temp(thermal_power_model_t *tm, double power_w) {
    if (!tm) return tm->state.ambient_temp_c;
    tm->state.total_power_w = power_w;
    tm->state.junction_temp_c = tm->state.ambient_temp_c +
        tm->theta.theta_ja * power_w;
    tm->state.case_temp_c = tm->state.junction_temp_c -
        tm->theta.theta_jc * power_w;

    for (uint32_t i = 0; i < tm->num_hotspots; i++) {
        double local_t = tm->state.junction_temp_c +
            tm->hotspots[i].power_density_w_per_mm2 *
            tm->hotspots[i].width_mm * tm->hotspots[i].height_mm *
            tm->theta.theta_jc * 0.1;
        tm->hotspots[i].peak_temp_c = local_t;
        if (local_t > tm->state.hotspot_peak_c[i])
            tm->state.hotspot_peak_c[i] = local_t;
    }

    tm->state.thermal_margin_c = 105.0 - tm->state.junction_temp_c;
    return tm->state.junction_temp_c;
}

double tp_calc_case_temp(const thermal_power_model_t *tm) {
    if (!tm) return 0.0;
    return tm->state.case_temp_c;
}

double tp_calc_heatspreader_temp(const thermal_power_model_t *tm) {
    if (!tm) return 0.0;
    return tm->state.case_temp_c -
           tm->theta.theta_ca * tm->state.total_power_w * 0.2;
}

int tp_check_thermal_throttle(thermal_power_model_t *tm, double max_temp_c) {
    if (!tm) return 0;
    double t_junction = tp_calc_junction_temp(tm, tm->state.total_power_w);
    if (t_junction > max_temp_c) {
        tm->state.throttling = 1;
        return 1;
    }
    tm->state.throttling = 0;
    return 0;
}

void tp_mitigate_hotspots(thermal_power_model_t *tm) {
    if (!tm) return;

    for (uint32_t i = 0; i < tm->num_hotspots; i++) {
        hotspot_t *hs = &tm->hotspots[i];
        double excess_temp = hs->peak_temp_c - 85.0;
        if (excess_temp > 0.0) {
            double area = hs->width_mm * hs->height_mm;
            double new_area = area * 1.5;
            double new_width = sqrt(new_area);
            double new_height = sqrt(new_area);
            hs->width_mm = new_width;
            hs->height_mm = new_height;
            hs->power_density_w_per_mm2 *= area / new_area;
            hs->peak_temp_c -= excess_temp * 0.5;
        }
    }
}

void tp_pdn_init(power_delivery_t *pdn, double vdd_v, double total_current_a) {
    if (!pdn) return;
    memset(pdn, 0, sizeof(*pdn));
    pdn->vdd_v = vdd_v;
    pdn->total_current_a = total_current_a;
    pdn->ir_drop_target_mv = 25.0;
    pdn->pdn_impedance_mohm = 10.0;
    pdn->resonance_freq_mhz = 100.0;
    pdn->decap_total_nf = 0.0;
    pdn->ripple_mv = 5.0;
    pdn->num_c4_bumps = 5000;
    pdn->c4_resistance_mohm = 5.0;
    pdn->c4_inductance_ph = 10.0;
    pdn->board_resistance_mohm = 20.0;
    pdn->board_inductance_ph = 200.0;
}

double tp_pdn_ir_drop(const power_delivery_t *pdn) {
    if (!pdn) return 0.0;
    double total_r = pdn->c4_resistance_mohm + pdn->board_resistance_mohm +
                     pdn->pdn_impedance_mohm;
    double ir = pdn->total_current_a * total_r;
    return ir;
}

double tp_pdn_impedance_at_freq(const power_delivery_t *pdn, double freq_mhz) {
    if (!pdn) return 0.0;
    double omega = 2.0 * 3.1415926535 * freq_mhz * 1e6;
    double r = pdn->pdn_impedance_mohm * 1e-3;
    double l = (pdn->c4_inductance_ph + pdn->board_inductance_ph) * 1e-12;
    double c = pdn->decap_total_nf * 1e-9;
    double xl = omega * l;
    double xc = (c > 0.0) ? 1.0 / (omega * c) : 1e12;
    double z = sqrt(r * r + (xl - xc) * (xl - xc));
    return z * 1000.0;
}

int tp_pdn_add_decap(thermal_power_model_t *tm, const decap_t *d) {
    if (!tm || !d) return -1;
    if (tm->num_decaps >= THERMAL_MAX_DECAPS) return -2;
    memcpy(&tm->decaps[tm->num_decaps], d, sizeof(decap_t));
    tm->decaps[tm->num_decaps].placed = 1;
    tm->pdn.decap_total_nf += d->capacitance_nf;
    tm->num_decaps++;
    return 0;
}

double tp_pdn_resonance_freq(const power_delivery_t *pdn) {
    if (!pdn || pdn->decap_total_nf <= 0.0) return 0.0;
    double l = (pdn->c4_inductance_ph + pdn->board_inductance_ph) * 1e-12;
    double c = pdn->decap_total_nf * 1e-9;
    return 1.0 / (2.0 * 3.1415926535 * sqrt(l * c)) / 1e6;
}

double tp_pdn_ripple(const power_delivery_t *pdn, double switching_freq_mhz,
                      double di_dt_a_per_ns) {
    if (!pdn) return 0.0;
    double l = (pdn->c4_inductance_ph + pdn->board_inductance_ph) * 1e-12;
    double l_di_dt = l * di_dt_a_per_ns * 1e9;
    double r = pdn->pdn_impedance_mohm * 1e-3;
    double ir_ripple = r * pdn->total_current_a * 0.1;
    double z_at_f = tp_pdn_impedance_at_freq(pdn, switching_freq_mhz) * 1e-3;
    double i_ripple = z_at_f * pdn->total_current_a * 0.05;
    return (l_di_dt + ir_ripple + i_ripple) * 1000.0;
}

double tp_calc_3d_stack_temp(const thermal_power_model_t *tm,
                              const double *layer_powers, uint32_t num_layers) {
    if (!tm || !layer_powers || num_layers == 0) return 0.0;

    double ambient = tm->state.ambient_temp_c;
    double theta_inter = tm->inter_layer_tim_c_per_w > 0.0 ?
                         tm->inter_layer_tim_c_per_w : 5.0;
    double max_temp = ambient;

    for (uint32_t i = 0; i < num_layers && i < THERMAL_MAX_LAYERS; i++) {
        double cumulative_power = layer_powers[i];
        for (uint32_t j = 0; j < i; j++)
            cumulative_power += layer_powers[j] * 0.3;
        double t_layer = ambient +
            tm->theta.theta_jc * cumulative_power +
            theta_inter * (double)i * cumulative_power;
        if (t_layer > max_temp) max_temp = t_layer;
    }
    return max_temp;
}

double tp_calc_cooling_required(const thermal_power_model_t *tm,
                                 double target_temp_c) {
    if (!tm) return 0.0;
    double temp_rise = tm->state.junction_temp_c - tm->state.ambient_temp_c;
    double target_rise = target_temp_c - tm->state.ambient_temp_c;
    if (target_rise <= 0.0) return tm->state.cooling_capacity_w;
    return tm->state.total_power_w * (temp_rise / target_rise);
}

void tp_print_state(const thermal_power_model_t *tm) {
    if (!tm) return;
    printf("Thermal/Power State:\n");
    printf("  Die area: %.1f mm^2\n", tm->die_area_mm2);
    printf("  Power density: %.2f W/mm^2\n", tm->power_density_w_per_mm2);
    printf("  Total power: %.2f W\n", tm->state.total_power_w);
    printf("  T_junction: %.1f C\n", tm->state.junction_temp_c);
    printf("  T_case: %.1f C\n", tm->state.case_temp_c);
    printf("  T_ambient: %.1f C\n", tm->state.ambient_temp_c);
    printf("  Thermal margin: %.1f C\n", tm->state.thermal_margin_c);
    printf("  Theta_JA: %.2f C/W\n", tm->theta.theta_ja);
    printf("  Theta_JC: %.2f C/W\n", tm->theta.theta_jc);
    printf("  Throttling: %s\n", tm->state.throttling ? "YES" : "no");
    printf("  TIM: %s, %.0f um, %.1f W/mK, R_th=%.4f C/W\n",
           tim_material_name(tm->tim.material),
           tm->tim.bondline_thickness_um,
           tm->tim.thermal_conductivity,
           tm->tim.thermal_resistance_c_per_w);
    printf("  3D stack: %s, layers=%u\n",
           tm->three_d_stack ? "yes" : "no", tm->stack_layers);
}

void tp_print_hotspots(const thermal_power_model_t *tm) {
    if (!tm) return;
    printf("Hotspots: %u\n", tm->num_hotspots);
    for (uint32_t i = 0; i < tm->num_hotspots; i++) {
        printf("  [%u] at (%.1f,%.1f) %.1fx%.1f mm, %.2f W/mm^2, peak=%.1f C\n",
               i, tm->hotspots[i].x_mm, tm->hotspots[i].y_mm,
               tm->hotspots[i].width_mm, tm->hotspots[i].height_mm,
               tm->hotspots[i].power_density_w_per_mm2,
               tm->hotspots[i].peak_temp_c);
    }
}

void tp_print_pdn(const thermal_power_model_t *tm) {
    if (!tm) return;
    printf("Power Delivery Network:\n");
    printf("  VDD: %.3f V\n", tm->pdn.vdd_v);
    printf("  Total current: %.2f A\n", tm->pdn.total_current_a);
    printf("  IR drop: %.2f mV (target %.2f mV)\n",
           tp_pdn_ir_drop(&tm->pdn), tm->pdn.ir_drop_target_mv);
    printf("  PDN impedance: %.1f mohm\n", tm->pdn.pdn_impedance_mohm);
    printf("  Resonance: %.1f MHz\n",
           tp_pdn_resonance_freq(&tm->pdn));
    printf("  Decap total: %.1f nF (%u caps)\n",
           tm->pdn.decap_total_nf, tm->num_decaps);
    printf("  Ripple: %.2f mV\n", tm->pdn.ripple_mv);
    printf("  C4 bumps: %u (%.1f mohm, %.1f pH)\n",
           tm->pdn.num_c4_bumps,
           tm->pdn.c4_resistance_mohm,
           tm->pdn.c4_inductance_ph);
}

double tim_select_conductivity(tim_material_t material) {
    switch (material) {
    case TIM_SOLDER:        return 30.0;
    case TIM_GREASE:        return 3.5;
    case TIM_PCM:           return 0.5;
    case TIM_LIQUID_METAL:  return 40.0;
    case TIM_GRAPHITE_PAD:  return 5.0;
    case TIM_SINTERED_SILVER: return 200.0;
    default:                return 3.0;
    }
}

const char *tim_material_name(tim_material_t material) {
    switch (material) {
    case TIM_SOLDER:        return "Solder";
    case TIM_GREASE:        return "Thermal Grease";
    case TIM_PCM:           return "Phase-Change Material";
    case TIM_LIQUID_METAL:  return "Liquid Metal";
    case TIM_GRAPHITE_PAD:  return "Graphite Pad";
    case TIM_SINTERED_SILVER: return "Sintered Silver";
    default:                return "Unknown";
    }
}
