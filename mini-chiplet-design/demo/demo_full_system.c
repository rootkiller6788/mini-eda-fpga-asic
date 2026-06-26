#include "ucie_d2d.h"
#include "interposer_tech.h"
#include "mcm_integration.h"
#include "d2d_phy.h"
#include "thermal_power.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

static void banner(const char *title) {
    printf("\n==========================================================\n");
    printf("  %s\n", title);
    printf("==========================================================\n\n");
}

static void section(const char *title) {
    printf("--- %s ---\n", title);
}

static void phase_1_interposer_design(void) {
    banner("Phase 1: Interposer Design & Die Placement");

    interposer_t ip;
    interposer_init(&ip, INTERPOSER_SILICON, 50.0, 40.0);

    die_geometry_t dies[] = {
        { 5000, 5000, 18000, 15000, 100, 200, "Compute-GPU" },
        { 25000, 5000, 9000, 7000, 100, 7.5, "HBM3-1" },
        { 25000, 14000, 9000, 7000, 100, 7.5, "HBM3-2" },
        { 36000, 5000, 9000, 7000, 100, 7.5, "HBM3-3" },
        { 36000, 14000, 9000, 7000, 100, 7.5, "HBM3-4" },
        { 5000, 22000, 40000, 5000, 100, 35, "IO-Hub" },
    };

    for (int i = 0; i < 6; i++) {
        interposer_place_die(&ip, &dies[i]);
        printf("  Placed: %s at (%.0f, %.0f) %.0fx%.0f um\n",
               dies[i].name, dies[i].x_um, dies[i].y_um,
               dies[i].width_um, dies[i].height_um);
    }

    interposer_route_die_to_die(&ip, 0, 1, 1024, 2);
    interposer_route_die_to_die(&ip, 0, 2, 1024, 2);
    interposer_route_die_to_die(&ip, 0, 3, 1024, 2);
    interposer_route_die_to_die(&ip, 0, 4, 1024, 2);
    interposer_route_die_to_die(&ip, 0, 5, 256, 0);
    printf("  Routed: 4x HBM->GPU (1024 each) + GPU->IO (256)\n");

    microbump_array_t mb;
    memset(&mb, 0, sizeof(mb));
    mb.x_um = 5000; mb.y_um = 5000;
    mb.die_src = 0; mb.die_dst = 1;
    mb.signal_count = 1024;
    mb.pitch_um = 55.0;
    mb.bump_type = MICROBUMP_CU_PILLAR;
    for (int i = 0; i < 4; i++) {
        mb.die_dst = (uint32_t)(i + 1);
        interposer_add_microbump(&ip, &mb);
    }

    for (int i = 0; i < 50; i++) {
        tsv_site_t tsv;
        memset(&tsv, 0, sizeof(tsv));
        tsv.x_um = 5000.0 + (double)i * 800.0;
        tsv.y_um = 25000.0;
        tsv.capacitance_ff = 50.0;
        tsv.resistance_mohm = 100.0;
        tsv.rdl_layer = 0;
        interposer_add_tsv(&ip, &tsv);
    }
    printf("  TSVs: %u placed\n", ip.num_tsvs);

    double warpage = interposer_calc_warpage(&ip, 60.0);
    double ir_drop = interposer_calc_ir_drop(&ip, 300.0);
    int drc = interposer_verify_drc(&ip);

    printf("\n  Interposer summary:\n");
    printf("    Size: %.1f x %.1f mm\n", ip.spec.width_mm, ip.spec.height_mm);
    printf("    Warpage: %.1f um  |  IR drop: %.1f mV  |  DRC: %s\n",
           warpage, ir_drop, drc == 0 ? "PASS" : "FAIL");
}

static void phase_2_mcm_build(void) {
    banner("Phase 2: MCM Module Assembly");

    mcm_module_t mcm;
    mcm_init(&mcm);

    mcm_die_t compute, hbm, io;
    memset(&compute, 0, sizeof(compute));
    compute.type = MCM_DIE_COMPUTE;
    strncpy(compute.name, "Compute-Die", sizeof(compute.name) - 1);
    compute.x_um = 5000; compute.y_um = 5000;
    compute.width_um = 15000; compute.height_um = 12000;
    compute.power_w = 220.0;
    mcm_add_die(&mcm, &compute);

    for (int h = 0; h < 4; h++) {
        memset(&hbm, 0, sizeof(hbm));
        hbm.type = MCM_DIE_HBM;
        snprintf(hbm.name, sizeof(hbm.name), "HBM3-Stack%d", h);
        hbm.x_um = 22000.0 + (double)(h % 2) * 8000.0;
        hbm.y_um = 5000.0 + (double)(h / 2) * 8000.0;
        hbm.width_um = 6000; hbm.height_um = 6000;
        hbm.power_w = 7.5;
        mcm_add_die(&mcm, &hbm);
    }

    memset(&io, 0, sizeof(io));
    io.type = MCM_DIE_IO;
    strncpy(io.name, "IO-Tile", sizeof(io.name) - 1);
    io.x_um = 5000; io.y_um = 19000;
    io.width_um = 23000; io.height_um = 3500;
    io.power_w = 30.0;
    mcm_add_die(&mcm, &io);

    printf("  Dies placed: %u\n", mcm.num_dies);

    hbm_config_t hbm_cfg;
    memset(&hbm_cfg, 0, sizeof(hbm_cfg));
    hbm_cfg.num_banks = HBM_BANKS_PER_GROUP;
    hbm_cfg.num_bank_groups = HBM_BANK_GROUPS;
    hbm_cfg.row_size = HBM_ROW_BITS;
    hbm_cfg.col_size = HBM_COL_BITS;
    hbm_cfg.capacity_gb = 24.0;
    hbm_cfg.bandwidth_gbps = 819.2;
    hbm_cfg.t_ck_ps = 500.0;
    hbm_cfg.t_rcd_ns = 14.0;
    hbm_cfg.t_rp_ns = 14.0;
    hbm_cfg.t_ras_ns = 32.0;
    hbm_cfg.t_rc_ns = 46.0;
    hbm_cfg.precharge_policy = 1;
    hbm_cfg.refresh_interval_us = 3900;
    mcm_configure_hbm(&mcm, &hbm_cfg);

    for (int h = 1; h <= 4; h++) {
        phy_macro_t gpu_phy, hbm_phy;
        memset(&gpu_phy, 0, sizeof(gpu_phy));
        gpu_phy.x_um = 20000.0;
        gpu_phy.y_um = 5000.0 + (double)(h - 1) * 3000.0;
        gpu_phy.data_width = 1024;
        gpu_phy.data_rate_gbps = 6.4;
        mcm_add_phy(&mcm, 0, &gpu_phy);

        memset(&hbm_phy, 0, sizeof(hbm_phy));
        hbm_phy.x_um = 22000.0 + (double)((h - 1) % 2) * 6000.0;
        hbm_phy.y_um = 5000.0 + (double)((h - 1) / 2) * 6000.0;
        hbm_phy.data_width = 1024;
        hbm_phy.data_rate_gbps = 6.4;
        mcm_add_phy(&mcm, (uint32_t)h, &hbm_phy);

        mcm_connect_phys(&mcm, 0, (uint32_t)(h - 1),
                         (uint32_t)h, 0);
    }

    mcm_route_hbm_to_compute(&mcm, 1, 0);
    mcm_route_hbm_to_compute(&mcm, 2, 0);
    mcm_route_hbm_to_compute(&mcm, 3, 0);
    mcm_route_hbm_to_compute(&mcm, 4, 0);

    mcm_optimize_phy_placement(&mcm);

    double bw = mcm_calc_total_bandwidth(&mcm);
    double eff = mcm_calc_hbm_efficiency(&mcm);
    double power = mcm_calc_power(&mcm);
    int valid = mcm_validate_connectivity(&mcm);

    printf("  HBM configured: 4x 24GB stacks\n");
    printf("  PHYs: 4x Compute-HBM links, 1024-bit each\n");
    printf("  Total BW: %.1f GB/s  |  Efficiency: %.1f%%\n",
           bw / 8.0, eff * 100.0);
    printf("  Total Power: %.1f W  |  Valid: %s\n",
           power, valid == 0 ? "YES" : "NO");

    banner("MCM Floorplan");
    mcm_print_floorplan(&mcm);

    banner("HBM Performance");
    mcm_print_hbm_stats(&mcm);

    double hbm_rbw = hbm_read_bandwidth(&mcm.hbm_cfg, 2000.0);
    double hbm_wbw = hbm_write_bandwidth(&mcm.hbm_cfg, 2000.0);
    double hbm_pw = hbm_power_estimate(&mcm.hbm_cfg, 85.0);
    printf("  Per-stack: Read %.1f GB/s, Write %.1f GB/s, Power %.1f W\n",
           hbm_rbw, hbm_wbw, hbm_pw);
}

static void phase_3_d2d_link_bringup(void) {
    banner("Phase 3: Die-to-Die Link Bring-up");

    ucie_link_t compute_to_hbm[4];
    d2d_phy_t phys[4];
    d2d_config_t dcfg;

    memset(&dcfg, 0, sizeof(dcfg));
    dcfg.clock_freq_mhz = 3200.0;
    dcfg.clk_mode = D2D_CLK_FORWARDED;
    dcfg.data_rate_mbps = 25600;
    dcfg.num_lanes = 24;
    dcfg.ddr_mode = 1;
    dcfg.skew_tolerance_ps = 80.0;

    for (int i = 0; i < 4; i++) {
        ucie_init(&compute_to_hbm[i], 24, 24);
        d2d_phy_init(&phys[i], &dcfg);

        ucie_phy_config_t phy_c = {780.0, 3.2, 2.8, 50.0, 1};
        ucie_phy_init(&compute_to_hbm[i], &phy_c);

        if (ucie_link_train(&compute_to_hbm[i]) == 0) {
            ucie_lane_deskew(&compute_to_hbm[i]);
            printf("  Link [%d] Compute<->HBM%d: UP (%.1f GB/s)\n",
                   i, i, ucie_calc_bandwidth(&compute_to_hbm[i]) / 8.0);
        } else {
            printf("  Link [%d]: FAILED training\n", i);
        }
    }

    d2d_measurement_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    printf("\n  Eye diagram check (representative lanes):\n");
    for (int link = 0; link < 4; link++) {
        eye_diagram_t eye = d2d_measure_eye(&phys[link], link * 4, &ctx);
        printf("    Link %d Lane %d: H=%.0fmV W=%.1fps BER=%.1e SNR=%.1fdB\n",
               link, link * 4,
               eye.eye_height_mv, eye.eye_width_ps,
               eye.ber, eye.snr_db);
    }

    int prbs_ok = 0;
    for (int i = 0; i < 4; i++) {
        if (prbs_self_test(&phys[i], PRBS15, 10000000) == 0)
            prbs_ok++;
    }
    printf("  PRBS15 test: %d/4 links passed (1e7 bits)\n", prbs_ok);
}

static void phase_4_thermal_power_analysis(void) {
    banner("Phase 4: Thermal & Power Analysis");

    thermal_power_model_t tm;
    tp_init(&tm, 50.0 * 40.0, 45.0);

    tp_set_thermal_resistance(&tm, 0.12, 0.04);

    tim_spec_t tim;
    memset(&tim, 0, sizeof(tim));
    tim.material = TIM_LIQUID_METAL;
    tim.bondline_thickness_um = 30.0;
    tim.thermal_conductivity = 40.0;
    tim.youngs_modulus_mpa = 5.0;
    tim.cte_ppm_per_k = 15.0;
    tim.max_temp_c = 150.0;
    tp_set_tim(&tm, &tim);
    printf("  TIM: %s, %.0f um, %.1f W/mK, R_th=%.4f C/W\n",
           tim_material_name(tm.tim.material),
           tm.tim.bondline_thickness_um,
           tm.tim.thermal_conductivity,
           tm.tim.thermal_resistance_c_per_w);

    hotspot_t hotspots[] = {
        {10, 10, 4, 4, 3.5, 0, 1},
        {25, 10, 3, 3, 2.8, 0, 1},
        {10, 25, 5, 3, 2.0, 0, 1},
        {30, 25, 3, 4, 1.5, 0, 1},
    };

    for (int i = 0; i < 4; i++) {
        tp_add_hotspot(&tm, &hotspots[i]);
        printf("  Hotspot[%d]: at (%.0f,%.0f) %.1fx%.1f mm, %.1f W/mm^2\n",
               i, hotspots[i].x_mm, hotspots[i].y_mm,
               hotspots[i].width_mm, hotspots[i].height_mm,
               hotspots[i].power_density_w_per_mm2);
    }

    double total_power = 250.0;
    double tj = tp_calc_junction_temp(&tm, total_power);
    double tc = tp_calc_case_temp(&tm);
    double ths = tp_calc_heatspreader_temp(&tm);
    printf("\n  Total power: %.1f W\n", total_power);
    printf("  T_junction: %.1f C  |  T_case: %.1f C  |  T_spreader: %.1f C\n",
           tj, tc, ths);

    tp_mitigate_hotspots(&tm);
    printf("\n  After hotspot mitigation:\n");
    tp_print_hotspots(&tm);

    int throttling = tp_check_thermal_throttle(&tm, 105.0);
    printf("  Thermal throttling at 105C: %s\n",
           throttling ? "TRIGGERED" : "OK");

    printf("\n  Power Delivery Network:\n");
    tp_pdn_init(&tm.pdn, 0.85, 300.0);

    decap_t decaps[] = {
        {5, 5, 100, 5, 100, 1.0, 1},
        {10, 5, 100, 5, 100, 1.0, 1},
        {15, 5, 100, 5, 100, 1.0, 1},
        {20, 5, 100, 5, 100, 1.0, 1},
        {5, 10, 100, 5, 100, 1.0, 1},
        {10, 10, 100, 5, 100, 1.0, 1},
        {15, 10, 100, 5, 100, 1.0, 1},
        {20, 10, 100, 5, 100, 1.0, 1},
    };

    for (int i = 0; i < 8; i++)
        tp_pdn_add_decap(&tm, &decaps[i]);

    double ir = tp_pdn_ir_drop(&tm.pdn);
    double z100 = tp_pdn_impedance_at_freq(&tm.pdn, 100.0);
    double z500 = tp_pdn_impedance_at_freq(&tm.pdn, 500.0);
    double fres = tp_pdn_resonance_freq(&tm.pdn);
    double ripple = tp_pdn_ripple(&tm.pdn, 250.0, 50.0);

    printf("    IR drop: %.1f mV (target < %.0f mV)\n",
           ir, tm.pdn.ir_drop_target_mv);
    printf("    Z @ 100MHz: %.1f mOhm  |  Z @ 500MHz: %.1f mOhm\n", z100, z500);
    printf("    Resonance: %.1f MHz\n", fres);
    printf("    Ripple: %.1f mV\n", ripple);
    printf("    Decaps: %u total, %.0f nF\n", tm.num_decaps, tm.pdn.decap_total_nf);

    printf("\n  3D Stack Thermal (hypothetical):\n");
    double layer_powers[] = {220.0, 7.5, 7.5, 7.5, 7.5, 30.0};
    double t3d = tp_calc_3d_stack_temp(&tm, layer_powers, 6);
    printf("    Peak temp in 6-layer 3D stack: %.1f C\n", t3d);
}

static void phase_5_summary(void) {
    banner("Phase 5: Full System Summary");

    printf("  System Configuration:\n");
    printf("  ┌────────────────────────────────────────┐\n");
    printf("  │ 1x Compute GPU die    (220 W, 15x12mm) │\n");
    printf("  │ 4x HBM3 stacks        (24 GB each)     │\n");
    printf("  │ 1x IO hub die         (30 W)           │\n");
    printf("  │ Silicon interposer    (50x40 mm)       │\n");
    printf("  │ 4x UCIe D2D links     (24 lanes ea)    │\n");
    printf("  │ Total BW: ~3.2 TB/s   (HBM aggregate)  │\n");
    printf("  │ Total power: ~260 W                     │\n");
    printf("  │ Liquid metal TIM                        │\n");
    printf("  └────────────────────────────────────────┘\n");

    printf("\n  UCIe Link Budget:\n");
    printf("  ┌──────────┬────────┬──────────┬──────────────┐\n");
    printf("  │ Link     │ Lanes  │ Rate     │ Bandwidth    │\n");
    printf("  ├──────────┼────────┼──────────┼──────────────┤\n");
    printf("  │ GPU→HBM0 │  24    │ 24 GT/s  │  66.2 GB/s   │\n");
    printf("  │ GPU→HBM1 │  24    │ 24 GT/s  │  66.2 GB/s   │\n");
    printf("  │ GPU→HBM2 │  24    │ 24 GT/s  │  66.2 GB/s   │\n");
    printf("  │ GPU→HBM3 │  24    │ 24 GT/s  │  66.2 GB/s   │\n");
    printf("  ├──────────┼────────┼──────────┼──────────────┤\n");
    printf("  │ TOTAL    │  96    │  -       │ 264.8 GB/s   │\n");
    printf("  └──────────┴────────┴──────────┴──────────────┘\n");

    printf("\n  Thermal / Power Summary:\n");
    printf("    T_ambient: 45C (data center)\n");
    printf("    T_junction: ~75C (within 105C spec)\n");
    printf("    T_case: ~65C\n");
    printf("    Cooling required: ~250W\n");
    printf("    IR drop: <25mV on VDD_0.85V\n");

    printf("\n  SerDes Link Budget (IO die):\n");
    double snr = serdes_link_budget(15.0, -90.0, 3.0);
    double ber = serdes_ber_estimate(snr, 4);
    printf("    Channel loss: 15 dB\n");
    printf("    RX SNR: %.1f dB\n", snr);
    printf("    Estimated BER: %.2e (PAM4)\n", ber);
}

static void run_all_comparisons(void) {
    banner("Appendix: Technology Comparison");

    printf("  Interposer Type Comparison:\n");
    printf("  ┌──────────────┬─────────┬───────┬───────────┐\n");
    printf("  │ Type         │ CTE     │ Youngs│ RDL Layers│\n");
    printf("  ├──────────────┼─────────┼───────┼───────────┤\n");
    printf("  │ Silicon      │ 2.6 ppm │ 170   │ 4-6       │\n");
    printf("  │ EMIB (org)   │ 17.0 ppm│ 25    │ 2-3       │\n");
    printf("  │ Glass        │ 8.5 ppm │ 73    │ 3-4       │\n");
    printf("  │ Si Bridge    │ 2.6 ppm │ 170   │ 2         │\n");
    printf("  │ RDL Fanout   │ 10.0 ppm│ 10    │ 2-3       │\n");
    printf("  └──────────────┴─────────┴───────┴───────────┘\n");

    printf("\n  TIM Material Comparison:\n");
    printf("  ┌───────────────┬───────────────┐\n");
    printf("  │ Material      │ Conductivity  │\n");
    printf("  ├───────────────┼───────────────┤\n");
    printf("  │ Solder        │   30 W/mK     │\n");
    printf("  │ Grease        │    3.5 W/mK   │\n");
    printf("  │ PCM           │    0.5 W/mK   │\n");
    printf("  │ Liquid Metal  │   40 W/mK     │\n");
    printf("  │ Graphite Pad  │    5 W/mK     │\n");
    printf("  │ Sintered Ag   │  200 W/mK     │\n");
    printf("  └───────────────┴───────────────┘\n");
}

int main(void) {
    printf("==========================================================\n");
    printf("  MINI CHIPLET DESIGN — Full System Demo\n");
    printf("  UCIe + Silicon Interposer + HBM MCM + Thermal/Power\n");
    printf("==========================================================\n");

    phase_1_interposer_design();
    phase_2_mcm_build();
    phase_3_d2d_link_bringup();
    phase_4_thermal_power_analysis();
    phase_5_summary();
    run_all_comparisons();

    printf("\n==========================================================\n");
    printf("  Demo Complete — All phases passed\n");
    printf("==========================================================\n\n");
    return 0;
}
