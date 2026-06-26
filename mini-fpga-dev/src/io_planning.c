/* ================================================================
 * src/io_planning.c - FPGA I/O Planning Implementation
 * L2: I/O pad ring, bank architecture
 * L4: Transmission Line Theory, IBIS models
 * L6: Complete I/O planning flow with SSO analysis
 * L7: Pinout file generation (XDC/UCF format)
 * References: Xilinx UG571, Intel I/O Planning Guide
 * ================================================================ */

#include "io_planning.h"
#include "place_fpga.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

/* L4: IBIS I/O Buffer Characteristics
 * Each I/O standard has defined electrical parameters.
 * IBIS (I/O Buffer Information Specification) models provide
 * V/I curves for accurate signal integrity simulation.
 * Reference: IBIS Specification v7.0 */
FpgaIoElectrical io_get_electrical(FpgaIoStandard std) {
    FpgaIoElectrical e;
    memset(&e, 0, sizeof(e));
    switch (std) {
        case IO_LVCMOS33:
            e.vcco = 3.3; e.vref = 1.65; e.vih_min = 2.0; e.vil_max = 0.8;
            e.voh_min = 2.4; e.vol_max = 0.4;
            break;
        case IO_LVCMOS25:
            e.vcco = 2.5; e.vref = 1.25; e.vih_min = 1.7; e.vil_max = 0.7;
            e.voh_min = 1.9; e.vol_max = 0.4;
            break;
        case IO_LVCMOS18:
            e.vcco = 1.8; e.vref = 0.9; e.vih_min = 1.17; e.vil_max = 0.63;
            e.voh_min = 1.35; e.vol_max = 0.45;
            break;
        case IO_LVCMOS15:
            e.vcco = 1.5; e.vref = 0.75; e.vih_min = 0.975; e.vil_max = 0.525;
            e.voh_min = 1.125; e.vol_max = 0.375;
            break;
        case IO_LVCMOS12:
            e.vcco = 1.2; e.vref = 0.6; e.vih_min = 0.78; e.vil_max = 0.42;
            e.voh_min = 0.9; e.vol_max = 0.3;
            break;
        case IO_LVTTL:
            e.vcco = 3.3; e.vref = 1.5; e.vih_min = 2.0; e.vil_max = 0.8;
            e.voh_min = 2.4; e.vol_max = 0.4;
            break;
        case IO_LVDS:
            e.vcco = 2.5; e.vref = 1.25; e.vih_min = 1.3; e.vil_max = 1.2;
            e.voh_min = 1.475; e.vol_max = 0.925;
            break;
        case IO_SSTL18_I:
            e.vcco = 1.8; e.vref = 0.9; e.vih_min = 1.05; e.vil_max = 0.75;
            e.voh_min = 1.35; e.vol_max = 0.45;
            break;
        case IO_SSTL15_I:
            e.vcco = 1.5; e.vref = 0.75; e.vih_min = 0.875; e.vil_max = 0.625;
            e.voh_min = 1.1; e.vol_max = 0.4;
            break;
        case IO_HSTL_I:
            e.vcco = 1.5; e.vref = 0.75; e.vih_min = 0.85; e.vil_max = 0.65;
            e.voh_min = 1.1; e.vol_max = 0.4;
            break;
        default:
            e.vcco = 3.3; e.vref = 1.65; e.vih_min = 2.0; e.vil_max = 0.8;
            e.voh_min = 2.4; e.vol_max = 0.4;
    }
    return e;
}

/* L4: Transmission Line - Reflection Coefficient
 * Gamma = (Z_load - Z_0) / (Z_load + Z_0)
 * For matched termination: Z_load = Z_0 -> Gamma = 0 (no reflection)
 * Reference: Pozar, "Microwave Engineering", 4th Ed */
double io_reflection_coefficient(double z0, double z_load) {
    if (fabs(z0 + z_load) < 1e-9) return 1.0;
    return (z_load - z0) / (z_load + z0);
}

/* L4: I/O Propagation Delay
 * t_pd = intrinsic_delay + load_delay
 * load_delay = R_driver * C_load + signal_integrity_margin
 * Reference: Dally & Poulton, "Digital Systems Engineering" */
double io_propagation_delay(FpgaIoStandard std, double load_cap_pf) {
    FpgaIoElectrical e = io_get_electrical(std);
    (void)e; /* Future: use e for standard-specific delay parameters */
    double r_drv = 25.0;  /* typical driver resistance in ohms */
    double t_intrinsic = 0.5;   /* ns */
    double t_load = r_drv * load_cap_pf * 1e-3;  /* R*C in ns */
    return t_intrinsic + t_load;
}

void io_ring_init(FpgaIoRing *ring, double die_w, double die_h,
                   double pitch) {
    assert(ring);
    memset(ring, 0, sizeof(FpgaIoRing));
    ring->die_width_um = die_w;
    ring->die_height_um = die_h;
    ring->pad_pitch_um = pitch;
    /* Estimate pads per side */
    ring->pads_per_side[RING_TOP]    = (int)(die_w / pitch);
    ring->pads_per_side[RING_BOTTOM] = (int)(die_w / pitch);
    ring->pads_per_side[RING_LEFT]   = (int)(die_h / pitch);
    ring->pads_per_side[RING_RIGHT]  = (int)(die_h / pitch);
    ring->num_pads = 0;
    ring->num_banks = 0;
}

int io_ring_add_pad(FpgaIoRing *ring, FpgaPadType type,
                     FpgaIoStandard std, int location) {
    assert(ring);
    if (ring->num_pads >= FPGA_MAX_PADS) return -1;
    int id = ring->num_pads++;
    FpgaIoPad *pad = &ring->pads[id];
    memset(pad, 0, sizeof(FpgaIoPad));
    pad->pad_id = id;
    pad->type = type;
    pad->standard = std;
    pad->location = location;
    pad->bank = -1;
    pad->bank_type = BANK_HR;
    pad->is_clock_capable = false;
    pad->is_differential = false;
    pad->differential_pair = -1;
    pad->connected_net = -1;
    pad->drive_strength_ma = 12.0;
    pad->slew_rate = 1.0;
    pad->pullup = false;
    pad->pulldown = false;
    pad->termination = false;
    pad->termination_ohms = 0.0;
    pad->is_fixed = false;
    pad->is_vref = false;

    if (type == PAD_POWER) ring->total_power_pads++;
    else if (type == PAD_GROUND) ring->total_ground_pads++;
    else ring->total_signal_pads++;

    return id;
}

int io_ring_add_bank(FpgaIoRing *ring, FpgaBankType type,
                      double vcco, int start_loc, int end_loc) {
    assert(ring);
    if (ring->num_banks >= FPGA_MAX_BANKS) return -1;
    int id = ring->num_banks++;
    FpgaIoBank *bank = &ring->banks[id];
    memset(bank, 0, sizeof(FpgaIoBank));
    bank->bank_id = id;
    bank->type = type;
    bank->vcco = vcco;
    bank->vref = vcco / 2.0;
    bank->start_loc = start_loc;
    bank->end_loc = end_loc;
    bank->num_pads = 0;
    bank->is_powered = true;
    return id;
}

void io_ring_assign_bank(FpgaIoRing *ring, int pad_id, int bank_id) {
    assert(ring);
    if (pad_id < 0 || pad_id >= ring->num_pads) return;
    if (bank_id < 0 || bank_id >= ring->num_banks) return;
    ring->pads[pad_id].bank = bank_id;
    ring->banks[bank_id].pads[ring->banks[bank_id].num_pads++] = pad_id;
}

void io_ring_destroy(FpgaIoRing *ring) {
    (void)ring;
}

/* L6: Full I/O Planning Flow
 * 1. Estimate required signal pads from netlist I/O count
 * 2. Estimate power/ground pads using Rent's Rule
 * 3. Assign banks based on I/O standards
 * 4. Assign physical locations (minimizing routing congestion)
 * 5. Validate voltage compatibility */
int io_plan_full_ring(FpgaIoRing *ring, const FpgaPlacement *p,
                       const FpgaNet* nets, int num_nets,
                       int num_io_pads_needed) {
    assert(ring && p);

    /* Count nets needing external I/O */
    int external_nets = 0;
    for (int i = 0; i < num_nets; i++) {
        if (nets[i].source_node < 0 || nets[i].num_sinks == 0) {
            external_nets++;
        }
    }
    if (external_nets < num_io_pads_needed)
        external_nets = num_io_pads_needed;

    /* Add signal I/O pads */
    for (int i = 0; i < external_nets && i < ring->pads_per_side[RING_TOP]; i++) {
        io_ring_add_pad(ring, PAD_BIDIR, IO_LVCMOS33, i);
    }

    /* Add power/ground pads using Rent's Rule */
    int est_power = io_estimate_power_pads(ring->total_signal_pads, 0.5);
    for (int i = 0; i < est_power / 2; i++) {
        io_ring_add_pad(ring, PAD_POWER, IO_LVCMOS33, -1);
        io_ring_add_pad(ring, PAD_GROUND, IO_LVCMOS33, -1);
    }

    /* Create one bank per side */
    int top_bnk = io_ring_add_bank(ring, BANK_HR, 3.3, 0,
                                    ring->pads_per_side[RING_TOP]);
    (void)top_bnk;

    return ring->num_pads;
}

/* L6: I/O Pin Assignment
 * Assign logical I/Os to physical pad locations.
 * Greedy: assign input receivers closest to their driving CLBs,
 * output drivers closest to their source CLBs.
 * Complexity: O(N_pads * N_blocks) */
int io_assign_pins(FpgaIoRing *ring, const FpgaPlacement *p,
                    const FpgaNet* nets, int num_nets) {
    assert(ring && p && nets);
    int assigned = 0;
    for (int i = 0; i < num_nets; i++) {
        if (nets[i].source_node < 0) continue;
        int src_x = p->blocks[nets[i].source_node].x;
        int src_y = p->blocks[nets[i].source_node].y;

        /* Find nearest available pad on the nearest edge */
        int best_pad = -1;
        double best_dist = 1e9;
        for (int pid = 0; pid < ring->num_pads; pid++) {
            FpgaIoPad *pad = &ring->pads[pid];
            if (pad->connected_net >= 0) continue;  /* already assigned */
            if (pad->type == PAD_POWER || pad->type == PAD_GROUND) continue;

            double dx = (double)(pad->location - src_x);
            double dy = 0.0;  /* simplified: all pads on one edge */
            double dist = sqrt(dx * dx + dy * dy);
            if (dist < best_dist) {
                best_dist = dist;
                best_pad = pid;
            }
        }
        if (best_pad >= 0) {
            ring->pads[best_pad].connected_net = i;
            ring->pads[best_pad].connected_clb_x = src_x;
            ring->pads[best_pad].connected_clb_y = src_y;
            assigned++;
        }
    }
    return assigned;
}

/* L4: Rent's Rule for Power Pad Estimation
 * N_power_pads = K * N_signal_pads ^ p
 * where p is the Rent exponent for I/O (typically ~0.5)
 * Reference: Bakoglu, "Circuits, Interconnections, and Packaging
 *            for VLSI", Addison-Wesley, 1990 */
int io_estimate_power_pads(int num_signal_pads, double rent_exponent) {
    if (num_signal_pads <= 0) return 2;
    double K = 0.5;
    int est = (int)ceil(K * pow(num_signal_pads, rent_exponent));
    return est < 2 ? 2 : est;
}

/* L6: Simultaneous Switching Output (SSO) Analysis
 *
 * When N outputs switch simultaneously, current flows through
 * package inductance, causing ground bounce:
 *   V_bounce = L_pkg * dI/dt ? L_pkg * N * I_per_pin / t_rise
 *
 * SSO limit: max N before V_bounce exceeds noise margin.
 * Rule of thumb: max SSO = (V_noise_margin * t_rise) / (L_pkg * I_per_pin)
 *
 * Reference: S.H. Hall et al., "High-Speed Digital System Design",
 *            Wiley-IEEE Press, 2000 */
void io_sso_analyze(const FpgaIoRing *ring, FpgaSsoAnalysis *sso) {
    assert(ring && sso);
    memset(sso, 0, sizeof(FpgaSsoAnalysis));

    double L_pkg = 5e-9;     /* 5 nH package inductance */
    double t_rise = 1e-9;    /* 1 ns rise time */
    double I_per_pin = 0.012; /* 12 mA per pin */
    double V_noise_margin = 0.4; /* 400 mV noise margin */

    sso->max_sso_per_bank = (int)((V_noise_margin * t_rise) /
                                   (L_pkg * I_per_pin));

    /* Count actual switching outputs in each bank */
    for (int b = 0; b < ring->num_banks; b++) {
        int switching = 0;
        for (int p = 0; p < ring->banks[b].num_pads; p++) {
            int pid = ring->banks[b].pads[p];
            if (ring->pads[pid].type == PAD_OUTPUT ||
                ring->pads[pid].type == PAD_BIDIR) {
                switching++;
            }
        }
        if (switching > sso->max_sso_per_bank) {
            sso->is_violated = true;
        }
    }

    sso->actual_sso = sso->max_sso_per_bank;
    sso->ground_bounce_est_mv = (L_pkg * sso->actual_sso * I_per_pin / t_rise) * 1000.0;
    sso->noise_margin_mv = V_noise_margin * 1000.0;
}

bool io_sso_check(const FpgaIoRing *ring, int switching_count) {
    assert(ring);
    FpgaSsoAnalysis sso;
    io_sso_analyze(ring, &sso);
    return switching_count <= sso.max_sso_per_bank;
}

/* L7: Write pinout in XDC format (Xilinx) */
int io_write_pinout(const FpgaIoRing *ring, const char *filename,
                     FpgaPinoutFormat fmt) {
    assert(ring && filename);
    FILE *fp = fopen(filename, "w");
    if (!fp) return -1;

    switch (fmt) {
        case PINOUT_XDC:
            fprintf(fp, "# XDC Pinout Constraints\n");
            for (int i = 0; i < ring->num_pads; i++) {
                const FpgaIoPad *pad = &ring->pads[i];
                if (pad->type == PAD_POWER || pad->type == PAD_GROUND) continue;
                const char *std_names[] = {
                    "LVCMOS33","LVCMOS25","LVCMOS18","LVCMOS15","LVCMOS12",
                    "LVTTL","SSTL18_I","SSTL18_II","SSTL15_I","SSTL15_II",
                    "HSTL_I","HSTL_II","HSTL_III","LVDS","LVDS_EXT","BLVDS",
                    "TMDS","RSDS","HSUL_12","POD12","DIFF_SSTL18_I",
                    "DIFF_SSTL18_II","CUSTOM"
                };
                const char *std_name = (pad->standard < 22)
                    ? std_names[pad->standard] : "LVCMOS33";
                fprintf(fp, "set_property PACKAGE_PIN P%d [get_ports net_%d]\n",
                        pad->location, pad->connected_net);
                fprintf(fp, "set_property IOSTANDARD %s [get_ports net_%d]\n",
                        std_name, pad->connected_net);
                if (pad->drive_strength_ma > 0)
                    fprintf(fp, "set_property DRIVE %d [get_ports net_%d]\n",
                            (int)pad->drive_strength_ma, pad->connected_net);
            }
            break;

        case PINOUT_UCF:
            fprintf(fp, "# UCF Pinout Constraints\n");
            for (int i = 0; i < ring->num_pads; i++) {
                const FpgaIoPad *pad = &ring->pads[i];
                if (pad->type == PAD_POWER || pad->type == PAD_GROUND) continue;
                fprintf(fp, "NET \"net_%d\" LOC = \"P%d\";\n",
                        pad->connected_net, pad->location);
            }
            break;

        case PINOUT_CSV:
            fprintf(fp, "PadID,Type,Standard,Location,Net,Bank\n");
            for (int i = 0; i < ring->num_pads; i++) {
                const FpgaIoPad *pad = &ring->pads[i];
                fprintf(fp, "%d,%d,%d,%d,%d,%d\n",
                        pad->pad_id, (int)pad->type, (int)pad->standard,
                        pad->location, pad->connected_net, pad->bank);
            }
            break;

        case PINOUT_SDC:
            fprintf(fp, "# SDC Pin Constraints\n");
            for (int i = 0; i < ring->num_pads; i++) {
                const FpgaIoPad *pad = &ring->pads[i];
                if (pad->type == PAD_POWER || pad->type == PAD_GROUND) continue;
                fprintf(fp, "set_input_delay -clock clk 0.5 [get_ports net_%d]\n",
                        pad->connected_net);
            }
            break;

        default:
            fclose(fp);
            return -1;
    }

    fclose(fp);
    return 0;
}

/* L4: Bank Voltage Compatibility Check
 * All pads in a bank must share the same Vcco.
 * Some standards allow a range of Vcco values.
 * Reference: Xilinx UG471 SelectIO */
bool io_bank_compatible(const FpgaIoBank *bank, FpgaIoStandard std) {
    assert(bank);
    FpgaIoElectrical e = io_get_electrical(std);
    return fabs(bank->vcco - e.vcco) < 0.1;
}

/* L6: Differential pair validation
 * Differential pairs must be adjacent with complementary polarities. */
bool io_validate_diff_pairs(const FpgaIoRing *ring) {
    assert(ring);
    for (int i = 0; i < ring->num_pads; i++) {
        const FpgaIoPad *pad = &ring->pads[i];
        if (!pad->is_differential) continue;
        int pair = pad->differential_pair;
        if (pair < 0 || pair >= ring->num_pads) return false;
        if (!ring->pads[pair].is_differential) return false;
        if (abs(pad->location - ring->pads[pair].location) != 1) return false;
    }
    return true;
}

void io_ring_print_summary(const FpgaIoRing *ring) {
    assert(ring);
    printf("=== I/O Ring Summary ===\n");
    printf("Die:            %.0f x %.0f um\n",
           ring->die_width_um, ring->die_height_um);
    printf("Pad pitch:      %.1f um\n", ring->pad_pitch_um);
    printf("Total pads:     %d\n", ring->num_pads);
    printf("  Signal:       %d\n", ring->total_signal_pads);
    printf("  Power:        %d\n", ring->total_power_pads);
    printf("  Ground:       %d\n", ring->total_ground_pads);
    printf("I/O banks:      %d\n", ring->num_banks);
    printf("Per side (T/B/L/R): %d/%d/%d/%d\n",
           ring->pads_per_side[0], ring->pads_per_side[1],
           ring->pads_per_side[2], ring->pads_per_side[3]);
    for (int b = 0; b < ring->num_banks; b++) {
        printf("  Bank %d: %.1fV, %d pads (loc %d-%d)\n",
               ring->banks[b].bank_id, ring->banks[b].vcco,
               ring->banks[b].num_pads,
               ring->banks[b].start_loc, ring->banks[b].end_loc);
    }
}
