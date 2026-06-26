#include "rtl_to_gds.h"
#include "std_cells.h"
#include "floorplan_cts.h"
#include "eco_flow.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static double compute_delay(double r, double c, double driver_r)
{
    return (r * c + driver_r * c) * 1e12 * 1.5;
}

static double compute_ir_drop(double current_a, double resistance_mohm)
{
    return current_a * resistance_mohm;
}

static int run_synthesis_check(double freq_mhz, double area_um2, double power_mw,
                                double *out_slack, double *out_area,
                                double *out_power)
{
    double freq_factor = freq_mhz / 2000.0;
    *out_slack  = 0.05 * freq_factor - 0.02;
    *out_area   = area_um2 * (0.85 + freq_factor * 0.15);
    *out_power  = power_mw * (0.7 + freq_factor * 0.3);
    return (*out_slack > 0.0) ? 0 : 1;
}

static int run_sta_analysis(double clk_period_ns, int path_count,
                            double *setup_slack, double *hold_slack,
                            int *setup_viol, int *hold_viol)
{
    double setup_margin = clk_period_ns * 0.1;
    double hold_margin  = clk_period_ns * 0.05;
    *setup_slack = setup_margin - 0.01 * (double)path_count * 0.001;
    *hold_slack  = hold_margin;
    *setup_viol  = (*setup_slack < 0.0) ? (int)(-*setup_slack * 1000.0) : 0;
    *hold_viol   = (*hold_slack < 0.0)  ? (int)(-*hold_slack * 1000.0)  : 0;
    return *setup_viol + *hold_viol;
}

static void generate_signoff_gdsii(const char *design_name, int layers,
                                    int cell_count, double *file_size_mb)
{
    double bytes_per_cell = 200.0 + (double)layers * 50.0;
    *file_size_mb = bytes_per_cell * (double)cell_count / (1024.0 * 1024.0);
}

static void build_library(std_cell_lib_t *lib, int tech_nm)
{
    int i;
    double tracks = tech_nm <= 7 ? 240.0 : 300.0;
    int num_tracks = tech_nm <= 7 ? 9 : 12;
    double vdd = tech_nm <= 7 ? 0.75 : 0.8;

    std_cell_lib_init(lib, "signoff_lib", tech_nm, tracks, num_tracks, vdd);

    for (i = 0; i < 30; i++) {
        std_cell_t cell;
        vt_type_t vt = (i < 10) ? VT_LVT : (i < 20) ? VT_SVT : VT_HVT;
        drive_strength_t drv = (drive_strength_t)(i % 5);
        char name[32];
        snprintf(name, sizeof(name), "%s_BUF_%s",
                 vt_type_name(vt), drive_strength_name(drv));
        std_cell_init(&cell, name, CELL_COMBINATIONAL, vt, drv, (int)num_tracks);
        std_cell_add_timing_arc(&cell, ARC_CELL_RISE,
                                4.0 + drv * 1.5, 5.0 + drv * 1.5,
                                1.0 + drv * 0.3, 2.0 + drv * 0.3);
        std_cell_add_timing_arc(&cell, ARC_CELL_FALL,
                                3.5 + drv * 1.5, 4.5 + drv * 1.5,
                                1.0 + drv * 0.3, 2.0 + drv * 0.3);
        cell.leakage_power_nw = (vt == VT_LVT) ? 40.0 + drv * 15.0 :
                                (vt == VT_SVT) ? 8.0 + drv * 4.0  : 1.5 + drv * 0.8;
        cell.internal_power_uw = 0.4 + drv * 0.25;
        std_cell_lib_add_cell(lib, &cell);
    }

    for (i = 0; i < 6; i++) {
        std_cell_t cell;
        int vt_idx = i % 3;
        vt_type_t vt = (vt_idx == 0) ? VT_LVT : (vt_idx == 1) ? VT_SVT : VT_HVT;
        char name[32];
        snprintf(name, sizeof(name), "%s_DFF_X%d", vt_type_name(vt), 1 << (i/2));
        std_cell_init(&cell, name, CELL_SEQUENTIAL, vt,
                      (drive_strength_t)(i / 2), (int)num_tracks);
        cell.input_count  = 3;
        cell.output_count = 1;
        std_cell_add_timing_arc(&cell, ARC_SETUP_RISE, 8.0, 8.0, 1.0, 1.8);
        std_cell_add_timing_arc(&cell, ARC_SETUP_FALL, 7.0, 7.0, 1.0, 1.8);
        std_cell_add_timing_arc(&cell, ARC_HOLD_RISE, -1.5, -1.5, 1.0, 1.8);
        cell.leakage_power_nw = (vt == VT_LVT) ? 80.0 : (vt == VT_SVT) ? 15.0 : 3.0;
        std_cell_lib_add_cell(lib, &cell);
    }
}

static void build_floorplan_for_signoff(floorplan_t *fp)
{
    floorplan_init(fp, 1000.0, 800.0, 11);
    fp->cell_utilization = 0.75;
    floorplan_create_pg_grid(fp, 3.0, 42.0, 5, 0.75);

    {
        macro_block_t m;
        memset(&m, 0, sizeof(m));
        m.id = 1; m.type = MACRO_MEMORY;
        strncpy(m.name, "L2_CACHE_8MB", sizeof(m.name) - 1);
        m.width_um = 350.0; m.height_um = 250.0;
        m.x_um = 10.0; m.y_um = 540.0;
        m.is_locked = 1;
        floorplan_place_macro(fp, &m);
    }
    {
        macro_block_t m;
        memset(&m, 0, sizeof(m));
        m.id = 2; m.type = MACRO_PLL;
        strncpy(m.name, "PLL_3G", sizeof(m.name) - 1);
        m.width_um = 100.0; m.height_um = 80.0;
        m.x_um = 880.0; m.y_um = 710.0;
        m.is_locked = 1;
        floorplan_place_macro(fp, &m);
    }
    {
        macro_block_t m;
        memset(&m, 0, sizeof(m));
        m.id = 3; m.type = MACRO_IP;
        strncpy(m.name, "USB3_PHY", sizeof(m.name) - 1);
        m.width_um = 200.0; m.height_um = 180.0;
        m.x_um = 380.0; m.y_um = 610.0;
        m.is_locked = 1;
        floorplan_place_macro(fp, &m);
    }
    {
        macro_block_t m;
        memset(&m, 0, sizeof(m));
        m.id = 4; m.type = MACRO_ANALOG;
        strncpy(m.name, "ADC_12B", sizeof(m.name) - 1);
        m.width_um = 120.0; m.height_um = 100.0;
        m.x_um = 870.0; m.y_um = 10.0;
        m.is_locked = 1;
        floorplan_place_macro(fp, &m);
    }
    floorplan_auto_place_io_ring(fp, 55.0);
}

static void build_clock_tree_for_signoff(clock_tree_t *ct)
{
    clock_tree_init(ct, CLK_H_TREE);
    clock_tree_add_domain(ct, "CLK_CORE",    0.333, 8.0);
    clock_tree_add_domain(ct, "CLK_DDR",     0.625, 20.0);
    clock_tree_add_domain(ct, "CLK_PERIPH",  1.5,   40.0);
    clock_tree_add_domain(ct, "CLK_MEM",     0.8,   15.0);

    clock_tree_build_h_tree(ct, 0, 500.0, 400.0, 900.0, 700.0, 6);
    clock_tree_build_h_tree(ct, 1, 200.0, 100.0, 400.0, 200.0, 4);
    clock_tree_build_h_tree(ct, 2, 50.0, 50.0, 300.0, 150.0, 3);
    clock_tree_build_clock_mesh(ct, 3, 10.0, 540.0, 370.0, 790.0, 5, 8);

    clock_tree_insert_buffers(ct, 0, DRIVE_X32, 5);
    clock_tree_insert_buffers(ct, 1, DRIVE_X16, 3);
    clock_tree_insert_buffers(ct, 2, DRIVE_X8,  2);
    clock_tree_insert_buffers(ct, 3, DRIVE_X4,  3);
}

int main(void)
{
    asic_flow_t flow;
    std_cell_lib_t lib;
    floorplan_t fp;
    clock_tree_t ct;
    eco_flow_t eco;
    design_milestone_t ms;
    int i, tech_nm = 5;
    double setup_slack, hold_slack, out_area, out_power, out_slack;
    int setup_viol, hold_viol;
    double gds_size_mb;
    double ir_drop;
    int eco_signoff_pass;

    printf("========================================\n");
    printf("  Full Chip Signoff Demo  (%d nm)\n", tech_nm);
    printf("========================================\n\n");

    printf("--- Phase 1: Library Setup ---\n");
    build_library(&lib, tech_nm);
    std_cell_lib_report(&lib);

    printf("\n--- Phase 2: ASIC Flow Setup ---\n");
    asic_flow_init(&flow, tech_nm, 13);
    asic_flow_set_targets(&flow, 3000.0, 800000.0, 250.0);

    run_synthesis_check(flow.target_freq_mhz, flow.target_area_um2,
                        flow.target_power_mw, &out_slack, &out_area,
                        &out_power);
    printf("  Synthesis: slack=%.3f ns  area=%.1f um2  power=%.1f mW\n",
           out_slack, out_area, out_power);

    printf("\n--- Phase 3: Floorplan ---\n");
    build_floorplan_for_signoff(&fp);
    floorplan_report(&fp);

    printf("\n--- Phase 4: CTS ---\n");
    build_clock_tree_for_signoff(&ct);
    clock_tree_report(&ct);

    printf("\n--- Phase 5: STA Per Domain ---\n");
    for (i = 0; i < ct.domain_count; i++) {
        run_sta_analysis(ct.domains[i].period_ns, ct.domains[i].sink_count * 50,
                         &setup_slack, &hold_slack, &setup_viol, &hold_viol);
        printf("  %s: period=%.3f ns  setup_slack=%.3f ns  hold_slack=%.3f ns  "
               "setup_v=%d  hold_v=%d\n",
               ct.domains[i].name, ct.domains[i].period_ns,
               setup_slack, hold_slack, setup_viol, hold_viol);
    }

    printf("\n--- Phase 6: Stream-Out & GDSII ---\n");
    generate_signoff_gdsii("TOP_CHIP", fp.metal_layers, 1500000, &gds_size_mb);
    printf("  GDSII file size: %.1f MB\n", gds_size_mb);

    printf("\n--- Phase 7: IR Drop Analysis ---\n");
    ir_drop = compute_ir_drop(2.5, 12.0);
    printf("  Worst-case IR drop: %.1f mV  Target: %.1f mV  %s\n",
           ir_drop, flow.stages[ASIC_STAGE_FLOORPLAN].milestone.power_mw * 0.1,
           ir_drop < 25.0 ? "PASS" : "FAIL");

    printf("\n--- Phase 8: ECO Flow ---\n");
    eco_flow_init(&eco, ECO_FUNCTIONAL);
    eco_flow_set_max_iterations(&eco, 3);
    spare_cell_init(&eco.spares, 1.5, 100.0, 200.0);
    spare_cell_place(&eco.spares, &fp, 50);
    spare_cell_report(&eco.spares);

    eco_add_cell(&eco, "SVT_BUF_X4", 100.0, 200.0);
    eco_add_cell(&eco, "LVT_INV_X2", 150.0, 250.0);
    eco_remove_cell(&eco, "OLD_BUF_X1");
    eco_reconnect_net(&eco, "N123", "N456", "AND2_X4", 0);
    eco_insert_buffer(&eco, "CLK_NET_LONG", 1, DRIVE_X8);
    eco_change_drive_strength(&eco, "BUF_X2", DRIVE_X4);
    eco_tie_pin(&eco, "UNUSED_INV", 0, 0);

    eco_apply_operations(&eco);
    eco_verify_operations(&eco);
    eco_signoff_pass = eco_flow_signoff(&eco);

    printf("  ECO sign-off: %s\n", eco_signoff_pass ? "PASS" : "FAIL");

    printf("\n--- Phase 9: Stage Completion ---\n");
    memset(&ms, 0, sizeof(ms));
    ms.total_cells    = 1200000;
    ms.total_nets     = 2500000;
    ms.area_um2       = fp.core_area_um2 * 0.78;
    ms.power_mw       = 220.0;
    ms.clk_period_ns  = 0.333;
    ms.max_slack_ns   = 0.12;
    ms.min_slack_ns   = -0.02;
    ms.drc_errors     = 0;
    ms.lvs_errors     = 0;
    ms.setup_violations = 0;
    ms.hold_violations  = 0;

    for (i = 0; i < ASIC_STAGE_COUNT; i++) {
        asic_flow_advance_stage(&flow, (asic_stage_t)i, &ms);
    }

    asic_flow_report(&flow);

    printf("\n========================================\n");
    printf("  SignOff Summary\n");
    printf("========================================\n");
    printf("  Timing:   %s\n",
           flow.stages[ASIC_STAGE_STA].milestone.setup_violations == 0 &&
           flow.stages[ASIC_STAGE_STA].milestone.hold_violations == 0
           ? "PASS" : "FAIL");
    printf("  DRC/LVS:  %s\n",
           flow.stages[ASIC_STAGE_SIGNOFF].milestone.drc_errors == 0 &&
           flow.stages[ASIC_STAGE_SIGNOFF].milestone.lvs_errors == 0
           ? "PASS" : "FAIL");
    printf("  Power:    %s (%.1f mW / %.1f mW target)\n",
           flow.stages[ASIC_STAGE_SIGNOFF].milestone.power_mw <= flow.target_power_mw
           ? "PASS" : "FAIL",
           flow.stages[ASIC_STAGE_SIGNOFF].milestone.power_mw,
           flow.target_power_mw);
    printf("  Area:     %s (%.1f um2 / %.1f um2 target)\n",
           flow.stages[ASIC_STAGE_SIGNOFF].milestone.area_um2 <= flow.target_area_um2
           ? "PASS" : "FAIL",
           flow.stages[ASIC_STAGE_SIGNOFF].milestone.area_um2,
           flow.target_area_um2);
    printf("  ECO:      %s\n", eco_signoff_pass ? "PASS" : "FAIL");
    printf("  Signed-off: %s\n",
           asic_flow_is_signed_off(&flow) ? "YES -- TAPEOUT READY" : "NO");
    printf("========================================\n");

    return 0;
}
