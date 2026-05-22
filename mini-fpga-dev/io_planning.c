#include "io_planning.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* I/O Standard name lookup */
static const char *iostd_names[IOSTD_NUM] = {
    "LVCMOS33", "LVCMOS25", "LVCMOS18", "LVCMOS15", "LVCMOS12",
    "LVDS", "LVDS_EXT", "SSTL15", "SSTL18_I", "SSTL18_II",
    "HSTL_I", "HSTL_II", "HSUL_12", "POD12", "DIFF_SSTL15", "TMDS_33"
};

/* I/O Standard Vcco voltage lookup (V) */
static const double iostd_vcco[IOSTD_NUM] = {
    3.3, 2.5, 1.8, 1.5, 1.2,
    2.5, 2.5, 1.5, 1.8, 1.8,
    1.5, 1.5, 1.2, 1.2, 1.5, 3.3
};

const char *iostd_to_string(int iostd)
{
    if (iostd < 0 || iostd >= IOSTD_NUM) return "UNKNOWN";
    return iostd_names[iostd];
}

double iostd_get_vcco(int iostd)
{
    if (iostd < 0 || iostd >= IOSTD_NUM) return 0.0;
    return iostd_vcco[iostd];
}

/* =======================================================
 * I/O Pin Implementation
 * ======================================================= */

void io_pin_init(IoPin *pin, const char *name, int bank)
{
    memset(pin, 0, sizeof(IoPin));
    strncpy(pin->pin_name, name, sizeof(pin->pin_name) - 1);
    pin->bank_number = bank;
    pin->direction = IO_DIR_INPUT;
    pin->io_standard = IOSTD_LVCMOS33;
    pin->drive_strength = DRIVE_12MA;
    pin->slew_rate = SLEW_SLOW;
    pin->bank_vcc_aux_v = 1.8;
    pin->bank_vcco_v = 3.3;
    pin->vref_v = 0.0;
    pin->differential_pair = -1;
    pin->clk_capable_mask = 0;
    pin->is_assigned = 0;
    pin->is_fixed = 0;
}

int io_pin_assign_signal(IoPin *pin, const char *signal, int dir, int iostd)
{
    strncpy(pin->signal_name, signal, sizeof(pin->signal_name) - 1);
    pin->direction = dir;
    pin->io_standard = iostd;
    pin->bank_vcco_v = iostd_get_vcco(iostd);
    pin->is_assigned = 1;
    return 0;
}

void io_pin_set_drive(IoPin *pin, int drive)
{
    pin->drive_strength = drive;
}

void io_pin_set_term(IoPin *pin, int termination)
{
    (void)pin;
    (void)termination;
    /* Configure internal termination settings */
}

/* =======================================================
 * I/O Bank Implementation
 * ======================================================= */

void io_bank_init(IoBank *bank, int bank_num, double vcco)
{
    memset(bank, 0, sizeof(IoBank));
    bank->bank_number = bank_num;
    bank->vcco_target_v = vcco;
    bank->io_standard = IOSTD_LVCMOS33;
}

int io_bank_add_pin(IoBank *bank, const IoPin *pin)
{
    if (bank->num_pins >= MAX_PINS_PER_BANK) return -1;
    bank->pins[bank->num_pins] = *pin;
    bank->pins[bank->num_pins].bank_number = bank->bank_number;
    bank->num_pins++;
    if (pin->clk_capable_mask) bank->num_clock_pins++;
    return 0;
}

int io_bank_assign(IoBank *bank, int pin_idx, const char *signal,
                    int dir, int iostd)
{
    if (pin_idx < 0 || pin_idx >= bank->num_pins) return -1;
    IoPin *pin = &bank->pins[pin_idx];
    int ret = io_pin_assign_signal(pin, signal, dir, iostd);
    if (ret == 0) {
        bank->num_used_pins++;
        bank->io_standard = iostd;
    }
    return ret;
}

void io_bank_recalc_ssn(IoBank *bank)
{
    double total_current = 0.0;
    int switching_count = 0;
    for (int i = 0; i < bank->num_pins; i++) {
        if (bank->pins[i].is_assigned &&
            bank->pins[i].direction == IO_DIR_OUTPUT) {
            /* Estimate 8mA per switching output at 12mA drive */
            double pin_current = 8.0 * (double)(bank->pins[i].drive_strength + 2) /
                                  (double)(DRIVE_12MA + 2);
            total_current += pin_current;
            switching_count++;
        }
    }
    bank->total_ssn_current_ma = total_current;
    (void)switching_count;
}

/* =======================================================
 * Simultaneous Switching Noise (SSN) Implementation
 * ======================================================= */

void ssn_init(SsnAnalyzer *ssn)
{
    memset(ssn, 0, sizeof(SsnAnalyzer));
    ssn->per_pin_inductance_nh = 3.5; /* typical package L */
    ssn->is_safe = 1;
    ssn->error_pin_index = -1;
}

void ssn_add_aggressor(SsnAnalyzer *ssn)
{
    ssn->aggressor_count++;
}

void ssn_set_slew(SsnAnalyzer *ssn, double slew_ns)
{
    ssn->aggressor_slew_ns = slew_ns;
    /* dI/dt = approx Ioh / slew */
    if (slew_ns > 0.0) {
        ssn->dI_dt_ma_per_ns = 8.0 / slew_ns;
    }
}

int ssn_check(const SsnAnalyzer *ssn, double noise_margin_mv)
{
    if (ssn->aggressor_count == 0) return 0;

    /* V_noise = L_eff * dI/dt * N_aggressors / sqrt(N_return_paths) */
    double v_noise = ssn->per_pin_inductance_nh * ssn->dI_dt_ma_per_ns *
                     (double)ssn->aggressor_count;
    /* Attenuation from distributed return paths */
    v_noise /= sqrt((double)ssn->aggressor_count + 1.0);

    ((SsnAnalyzer *)ssn)->victim_noise_mv = v_noise;
    ((SsnAnalyzer *)ssn)->noise_margin_mv = noise_margin_mv;

    if (v_noise > noise_margin_mv) {
        ((SsnAnalyzer *)ssn)->is_safe = 0;
        return -1;
    }
    ((SsnAnalyzer *)ssn)->is_safe = 1;
    return 0;
}

double ssn_estimate_noise(const SsnAnalyzer *ssn)
{
    return ssn->per_pin_inductance_nh * ssn->dI_dt_ma_per_ns *
           (double)ssn->aggressor_count;
}

/* =======================================================
 * Pin Swapper Implementation
 * ======================================================= */

void pin_swap_init(PinSwapper *ps)
{
    memset(ps, 0, sizeof(PinSwapper));
    for (int i = 0; i < 4; i++) ps->is_swappable[i] = 1;
}

int pin_swap_add_pair(PinSwapper *ps, int pin_a, int pin_b)
{
    if (ps->group_size >= 32) return -1;
    ps->pin_group_a[ps->group_size] = pin_a;
    ps->pin_group_b[ps->group_size] = pin_b;
    ps->group_size++;
    return 0;
}

int pin_swap_evaluate(PinSwapper *ps)
{
    /* Estimate wirelength improvement from swapping */
    ps->wirelength_before = (double)ps->group_size * 100.0;
    double best = ps->wirelength_before;
    for (int i = 0; i < ps->group_size; i++) {
        double candidate = fabs((double)(ps->pin_group_a[i] - ps->pin_group_b[i]));
        if (candidate < best) best = candidate;
    }
    ps->wirelength_after = best * 100.0;
    ps->improvement_pct = (ps->wirelength_before - ps->wirelength_after) /
                          ps->wirelength_before * 100.0;
    return (ps->improvement_pct > 5.0) ? 1 : 0;
}

int pin_swap_apply(PinSwapper *ps)
{
    (void)ps;
    ps->swaps_performed++;
    return 0;
}

/* =======================================================
 * I/O Planner Implementation
 * ======================================================= */

void io_planner_init(IoPlanner *ip, const char *part)
{
    memset(ip, 0, sizeof(IoPlanner));
    strncpy(ip->part_name, part, sizeof(ip->part_name) - 1);
    ssn_init(&ip->ssn);
    pin_swap_init(&ip->swapper);
    ip->warn_ssn = 1;
    ip->idle_delay_ps = 0;
}

int io_planner_add_bank(IoPlanner *ip, const IoBank *bank)
{
    if (ip->num_banks >= MAX_IO_BANKS) return -1;
    ip->banks[ip->num_banks] = *bank;
    ip->num_banks++;
    ip->num_pins_total += bank->num_pins;
    return 0;
}

int io_planner_assign_pin(IoPlanner *ip, const char *package_pin,
                           const char *signal, int dir, int iostd)
{
    /* Search through all banks for the matching pin */
    for (int b = 0; b < ip->num_banks; b++) {
        IoBank *bank = &ip->banks[b];
        for (int p = 0; p < bank->num_pins; p++) {
            if (strcmp(bank->pins[p].pin_name, package_pin) == 0) {
                int ret = io_bank_assign(bank, p, signal, dir, iostd);
                if (ret == 0) {
                    ip->num_pins_assigned++;
                    /* Add to all_pins list for reporting */
                    if (ip->num_pins_assigned - 1 < MAX_TOTAL_PINS) {
                        ip->all_pins[ip->num_pins_assigned - 1] = bank->pins[p];
                    }
                }
                return ret;
            }
        }
    }
    return -1; /* pin not found */
}

int io_planner_auto_assign(IoPlanner *ip, char **signal_list, int count)
{
    int assigned = 0;
    int signal_idx = 0;

    for (int b = 0; b < ip->num_banks && signal_idx < count; b++) {
        IoBank *bank = &ip->banks[b];
        for (int p = 0; p < bank->num_pins && signal_idx < count; p++) {
            if (!bank->pins[p].is_assigned) {
                io_bank_assign(bank, p, signal_list[signal_idx],
                               IO_DIR_INPUT, IOSTD_LVCMOS33);
                signal_idx++;
                assigned++;
            }
        }
    }
    ip->num_pins_assigned += assigned;
    return assigned;
}

int io_planner_validate(IoPlanner *ip)
{
    int errors = 0;

    /* Check Vcco compatibility per bank */
    for (int b = 0; b < ip->num_banks; b++) {
        IoBank *bank = &ip->banks[b];
        double bank_vcco = 0.0;
        int first = 1;
        for (int p = 0; p < bank->num_pins; p++) {
            if (bank->pins[p].is_assigned) {
                double req_vcco = iostd_get_vcco(bank->pins[p].io_standard);
                if (first) {
                    bank_vcco = req_vcco;
                    first = 0;
                } else if (fabs(req_vcco - bank_vcco) > 0.01) {
                    fprintf(stderr, "ERROR: Bank %d Vcco mismatch: "
                            "%s requires %.1fV, bank set to %.1fV\n",
                            bank->bank_number, bank->pins[p].pin_name,
                            req_vcco, bank_vcco);
                    errors++;
                }
            }
        }
    }

    /* SSN analysis */
    for (int b = 0; b < ip->num_banks; b++) {
        io_bank_recalc_ssn(&ip->banks[b]);
        if (ip->banks[b].total_ssn_current_ma > 200.0 && ip->warn_ssn) {
            fprintf(stderr, "WARNING: Bank %d SSN current %.1f mA exceeds "
                    "recommended limit\n", ip->banks[b].bank_number,
                    ip->banks[b].total_ssn_current_ma);
        }
    }

    return errors;
}

void io_planner_report(const IoPlanner *ip)
{
    printf("=== I/O Planning Report ===\n");
    printf("  Part:            %s\n", ip->part_name);
    printf("  I/O Banks:       %d\n", ip->num_banks);
    printf("  Total pins:      %d\n", ip->num_pins_total);
    printf("  Pins assigned:   %d\n", ip->num_pins_assigned);
    printf("  SSN analyzer:    %s\n", ip->ssn.is_safe ? "Safe" : "WARNING");

    for (int b = 0; b < ip->num_banks; b++) {
        const IoBank *bank = &ip->banks[b];
        printf("  Bank %2d: Vcco=%.1fV  Pins=%2d/%2d  SSN=%.1fmA  Std=%s\n",
               bank->bank_number, bank->vcco_target_v,
               bank->num_used_pins, bank->num_pins,
               bank->total_ssn_current_ma,
               iostd_names[bank->io_standard]);
    }
}

void io_planner_export_xdc(const IoPlanner *ip, const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (!f) return;

    fprintf(f, "# Auto-generated I/O constraints\n");
    fprintf(f, "# Part: %s\n", ip->part_name);
    fprintf(f, "# Generated by mini-fpga-dev io_planner\n\n");

    for (int b = 0; b < ip->num_banks; b++) {
        const IoBank *bank = &ip->banks[b];
        fprintf(f, "# Bank %d (Vcco=%.1fV)\n", bank->bank_number, bank->vcco_target_v);
        for (int p = 0; p < bank->num_pins; p++) {
            if (bank->pins[p].is_assigned) {
                const IoPin *pin = &bank->pins[p];
                fprintf(f, "set_property PACKAGE_PIN %s  [get_ports {%s}]\n",
                        pin->pin_name, pin->signal_name);
                fprintf(f, "set_property IOSTANDARD %s [get_ports {%s}]\n",
                        iostd_names[pin->io_standard], pin->signal_name);
                if (pin->direction == IO_DIR_INPUT) {
                    fprintf(f, "# Input: %s\n", pin->signal_name);
                } else if (pin->direction == IO_DIR_OUTPUT) {
                    fprintf(f, "# Output: %s (drive=%dmA)\n",
                            pin->signal_name,
                            (pin->drive_strength == DRIVE_4MA) ? 4 :
                            (pin->drive_strength == DRIVE_8MA) ? 8 :
                            (pin->drive_strength == DRIVE_12MA) ? 12 :
                            (pin->drive_strength == DRIVE_16MA) ? 16 : 24);
                }
            }
        }
        fprintf(f, "\n");
    }
    fclose(f);
    printf("  I/O constraints written to: %s\n", filename);
}

/* =======================================================
 * LVDS Interface Implementation
 * ======================================================= */

void lvds_init(LvdsInterface *lvds, double clk_mhz, int width)
{
    memset(lvds, 0, sizeof(LvdsInterface));
    lvds->clk_freq_mhz = clk_mhz;
    lvds->bit_width = width;
    lvds->data_rate_mbps = (int)(clk_mhz * (double)width);
    lvds->is_serialized = (width > 1);
    lvds->termination = TERM_INTERNAL_50;
    lvds->vcm_v = 1.2;
    lvds->vod_mv = 350.0;
}

int lvds_assign_pins(LvdsInterface *lvds, const char *p, const char *n)
{
    strncpy(lvds->p_pin, p, sizeof(lvds->p_pin) - 1);
    strncpy(lvds->n_pin, n, sizeof(lvds->n_pin) - 1);
    return 0;
}

void lvds_calc_timing(LvdsInterface *lvds)
{
    double bit_period_ns = 1000.0 / (double)lvds->data_rate_mbps;
    lvds->setup_ns = bit_period_ns * 0.4;
    lvds->hold_ns  = bit_period_ns * 0.4;
}
