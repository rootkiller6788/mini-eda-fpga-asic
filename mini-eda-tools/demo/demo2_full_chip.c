#include "synthesis.h"
#include "place_route.h"
#include "static_timing.h"
#include "power_analysis.h"
#include "dfm_yield.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    int num_pi, num_po, num_ff, num_comb;
    int num_macros;
} design_stats_t;

static design_stats_t ds;

static void build_moderate_netlist(netlist_t *nl) {
    syn_create_netlist("top_chip", "nano_core_v2", nl);

    int pi = 0;
    for (int i = 0; i < 32; i++) {
        char name[16]; snprintf(name, sizeof(name), "PI_%02d", i);
        syn_add_gate(nl, GATE_INV, name);
        syn_set_pi(nl, nl->num_gates - 1); pi++;
    }
    int po_start = nl->num_gates;
    for (int i = 0; i < 16; i++) {
        char name[16]; snprintf(name, sizeof(name), "PO_%02d", i);
        syn_add_gate(nl, GATE_BUF, name);
        syn_set_po(nl, nl->num_gates - 1);
    }

    int seq_start = nl->num_gates;
    for (int i = 0; i < 64; i++) {
        char name[16]; snprintf(name, sizeof(name), "FF_%03d", i);
        syn_add_gate(nl, GATE_DFF, name);
    }

    int comb_start = nl->num_gates;
    gate_type_e combo[] = {GATE_NAND, GATE_NOR, GATE_AND, GATE_OR,
                           GATE_XOR, GATE_INV, GATE_MUX, GATE_AOI21,
                           GATE_OAI21, GATE_NAND, GATE_NOR, GATE_AND};
    int num_combo = 96;
    for (int i = 0; i < num_combo; i++) {
        char name[16]; snprintf(name, sizeof(name), "U_%03d", i);
        syn_add_gate(nl, combo[i % 12], name);
    }

    for (int i = 0; i < 32; i++) {
        syn_connect(nl, i, 0, comb_start + i % num_combo, i % 2);
        if (i < 16)
            syn_connect(nl, i, 0, comb_start + (i + 4) % num_combo, 0);
    }

    for (int i = 0; i < num_combo - 4; i++) {
        syn_connect(nl, comb_start + i, 0, comb_start + i + 4, i % 2);
    }

    for (int i = 0; i < 32; i++) {
        syn_connect(nl, comb_start + i * 2, 0,
                    seq_start + (i % 64), 0);
    }

    for (int i = 0; i < 16; i++) {
        int src = seq_start + (i * 3) % 64;
        syn_connect(nl, src, 0, po_start + i, 0);
    }

    int clk_id = nl->num_gates;
    syn_add_gate(nl, GATE_BUF, "CLK");
    syn_set_clock(nl, clk_id);

    ds.num_pi = 32; ds.num_po = 16; ds.num_ff = 64;
    ds.num_comb = num_combo; ds.num_macros = 2;

    printf("Netlist: %s (%d gates)\n", nl->name, nl->num_gates);
    printf("  PI=%d PO=%d FF=%d comb=%d\n",
           ds.num_pi, ds.num_po, ds.num_ff, ds.num_comb);
}

static void run_full_synthesis(netlist_t *nl, tech_lib_t *lib) {
    printf("\n--- Full Synthesis ---\n");
    syn_load_tech_lib("gf_22nm_fdx.lib", lib);
    lib->vendor = TECH_GF;
    lib->process_nm = 22.0;

    synth_params_t sp = {
        .target_freq_mhz   = 1200.0,
        .max_area_um2      = 10000.0,
        .max_leakage_uw    = 500.0,
        .allow_multi_vt    = true,
        .allow_clock_gating = true,
        .preserve_hierarchy = false,
        .optimize_timing   = true,
        .optimize_area     = true,
        .optimize_power    = true,
        .max_fanout        = 24,
        .effort_level      = 5,
    };

    synth_result_t sr;
    syn_synthesize(nl, lib, &sp, &sr);
    printf("Synthesized: area=%.2f um2 cells=%d leakage=%.2f uW\n",
           sr.area_um2, sr.total_cells, sr.leakage_uw);

    double crit = syn_estimate_delay(nl, lib);
    printf("Critical path: %.2f ps (%.0f MHz max)\n",
           crit, 1e6 / (crit + 0.001));
}

static void run_full_place_route(netlist_t *nl, tech_lib_t *lib,
                                 place_instance_t *inst,
                                 congestion_map_t *cmap,
                                 route_result_t *rr) {
    printf("\n--- Full Place & Route ---\n");

    int placeable = 0;
    for (int i = 0; i < nl->num_gates; i++)
        if (!nl->gates[i].is_pi && !nl->gates[i].is_po) placeable++;

    pr_create_place(inst, placeable + ds.num_macros);
    pr_set_die(inst, 500.0, 500.0, 0.28, 1.4, 13);

    double total_area = syn_estimate_area(nl, lib);
    printf("Die: 500x500 um  Utilization: %.1f%%\n",
           total_area / (500.0 * 500.0) * 100);

    int cidx = 0;
    for (int i = 0; i < nl->num_gates; i++) {
        if (nl->gates[i].is_pi || nl->gates[i].is_po) continue;
        int mc = nl->gates[i].mapped_cell;
        double w = 1.5, h = 2.5;
        if (mc >= 0 && mc < lib->num_cells) {
            w = sqrt(lib->cells[mc].area_um2) * 1.8;
            h = w * 0.55;
        }
        pr_add_place_cell(inst, cidx++, w, h,
                          nl->gates[i].type == GATE_DFF);
    }

    pr_add_place_cell(inst, cidx++, 50.0, 40.0, true);
    pr_add_place_cell(inst, cidx++, 40.0, 30.0, true);
    pr_set_fixed_cell(inst, cidx - 2, 50.0, 250.0);
    pr_set_fixed_cell(inst, cidx - 1, 400.0, 250.0);

    for (int i = 0; i < nl->num_gates; i++) {
        for (int j = 0; j < nl->gates[i].num_inputs; j++) {
            int src = nl->gates[i].inputs[j];
            if (src >= 0 && src < nl->num_gates &&
                !nl->gates[src].is_pi && !nl->gates[src].is_po) {
                pr_add_place_net(inst, src % placeable, i % placeable,
                                 nl->gates[src].is_clock ? 10.0 : 1.0);
            }
        }
    }

    inst->params.algo = PLACE_QUADRATIC;
    inst->params.max_iter = 150;

    place_result_t pr;
    pr_run_global_place(inst, &pr);
    printf("Global placement HPWL: %.2f um\n", pr.total_hpwl);

    inst->params.algo = PLACE_FORCE_DIRECTED;
    pr_global_place_force(inst, &pr);
    printf("Force-directed HPWL:   %.2f um\n", pr.total_hpwl);

    pr_legalize(inst);
    pr_detailed_place(inst, &pr);
    printf("Detailed HPWL:         %.2f um\n", pr.total_hpwl);

    pr_create_congestion_map(inst, cmap);
    printf("Congestion map: %dx%dx%d\n", cmap->grid_x, cmap->grid_y, cmap->num_layers);

    pr_run_global_route(inst, rr);
    printf("Global route: WL=%.2f um vias=%d\n",
           pr_total_wirelength(rr), pr_total_vias(rr));

    pr_update_congestion_map(cmap, rr);

    pr_ripup_reroute(inst, rr, 5);
    printf("After rip-up (5x): WL=%.2f um vias=%d\n",
           pr_total_wirelength(rr), pr_total_vias(rr));

    pr_run_detail_route(inst, rr);
    printf("Detail route: WL=%.2f um vias=%d DRC=%d\n",
           pr_total_wirelength(rr), pr_total_vias(rr), pr_count_drc(rr));
}

static void run_full_sta(netlist_t *nl, timing_graph_t *tg, constraints_t *con) {
    printf("\n--- Full STA ---\n");

    sta_init_constraints(con);
    sta_add_clock(con, "clk_fast", 833.0, 20.0, 8.0);
    sta_add_clock(con, "clk_slow", 5000.0, 30.0, 15.0);
    con->clocks[1].is_generated = true;
    strcpy(con->clocks[1].source_clock, "clk_fast");
    con->clocks[1].divide_factor = 6.0;

    for (int i = 0; i < 8; i++) {
        char name[16]; snprintf(name, sizeof(name), "PI_%02d", i);
        sta_set_io_delay(con, name, true, 100.0, 5.0);
    }
    for (int i = 0; i < 8; i++) {
        char name[16]; snprintf(name, sizeof(name), "PO_%02d", i);
        sta_set_io_delay(con, name, false, 150.0, 8.0);
    }

    sta_build_timing_graph(tg, nl, con);

    sta_result_t sr;
    sta_run_analysis(tg, con, &sr);

    printf("Clocks: %d  Constraints: %d IO\n",
           con->num_clocks, con->num_io_constraints);
    printf("Timing graph: %d nodes, %d arcs\n", tg->num_nodes, tg->num_arcs);

    printf("\nSetup timing:\n");
    printf("  WNS: %.2f ps\n", sr.wns_setup);
    printf("  TNS: %.2f ps\n", sr.tns_setup);
    printf("  Violated: %d/%d endpoints\n",
           sr.num_violated_setup, sr.num_endpoints);

    if (sr.num_violated_setup == 0) {
        printf("  Status: MET\n");
    } else {
        printf("  Status: VIOLATED (%.2f ps)\n", sr.wns_setup);
    }
}

static void run_full_power(netlist_t *nl, tech_lib_t *lib,
                           power_analysis_t *pa, activity_file_t *act) {
    printf("\n--- Full Power Analysis ---\n");

    paw_load_activity_file(act, "sim_vcd.saif");
    paw_estimate_activity(act, nl);

    paw_create_analysis(pa, nl);
    pa->params.vdd_v = 0.8;
    pa->params.frequency_mhz = 1200.0;
    pa->params.temperature_c = 105.0;
    pa->params.enable_clock_gating = true;
    pa->params.enable_multi_vt = true;
    pa->params.enable_power_gating = true;
    pa->params.enable_dvfs = true;
    pa->params.vdd_standby_v = 0.5;

    paw_run_analysis(pa, nl, lib, act, NULL);

    printf("Operating point: %.1fV @ %.0f MHz @ %.0f C\n",
           pa->params.vdd_v, pa->params.frequency_mhz,
           pa->params.temperature_c);
    printf("\n");
    paw_print_report(&pa->report);

    printf("\nPower domains:\n");
    power_domain_t domains[] = {
        {.id = 0, .name = "CPU",    .is_active = true},
        {.id = 1, .name = "DSP",    .is_active = true},
        {.id = 2, .name = "IO",     .is_active = true},
        {.id = 3, .name = "PHY",    .is_active = false},
    };
    paw_apply_power_gating(pa, nl, domains, 4);

    for (int d = 0; d < 4; d++) {
        printf("  Domain '%s': %s  sleep=%.4f mW\n",
               domains[d].name,
               domains[d].is_active ? "ACTIVE" : "SLEEP",
               domains[d].sleep_power_mw);
    }

    printf("\nDVFS analysis:\n");
    double freqs[] = {300.0, 600.0, 900.0, 1200.0};
    double vdds[]  = {0.5,  0.65,  0.75,  0.8};
    for (int i = 0; i < 4; i++) {
        double alpha_scale = freqs[i] / 1200.0;
        double vdd_scale = vdds[i] / 0.8;
        double dyn_est = pa->report.dynamic_switching_mw *
                         alpha_scale * vdd_scale * vdd_scale;
        double leak = pa->report.static_leakage_mw * vdd_scale;
        printf("  %.0fMHz @ %.2fV:  dyn=%.3f mW  leak=%.4f mW  total=%.3f mW\n",
               freqs[i], vdds[i], dyn_est, leak, dyn_est + leak);
    }
}

static void run_full_dfm(netlist_t *nl, congestion_map_t *cmap,
                         route_result_t *rr) {
    printf("\n--- Full DFM & Yield ---\n");

    design_rules_t rules;
    dfm_init_design_rules(&rules, 13);

    for (int l = 0; l < 13; l++) {
        double scale = (l == 0)  ? 3.0 :
                        (l < 3)  ? 1.5 :
                        (l < 8)  ? 1.0 : 2.0;
        dfm_set_layer_rule(&rules, l,
                           0.045 * scale, 0.045 * scale,
                           0.025 * scale, 0.005 * scale);
        rules.antenna_ratio[l] = 250.0 + l * 25.0;
        rules.min_density[l] = 0.15;
        rules.max_density[l] = 0.75;
    }

    drc_report_t drc;
    dfm_run_drc_physical(rr, &rules, &drc);
    printf("DRC: %d violations (%d warnings)\n",
           drc.num_violations, drc.num_warning);

    dfm_check_density(cmap, 0, &rules, &drc);
    printf("Metal density check: %d violations\n", drc.num_violations);

    printf("\nAntenna check:\n");
    int antenna_viol = 0;
    for (int i = 0; i < rr->num_nets; i++) {
        if (!pr_check_antenna(rr, i)) {
            antenna_viol++;
            if (antenna_viol <= 3)
                printf("  Net %d: antenna violation\n", i);
        }
    }
    printf("  Total antenna violations: %d\n", antenna_viol);

    printf("\nLithography simulation:\n");
    litho_params_t litho = {
        .na = 1.35, .wavelength_nm = 193.0, .sigma = 0.85,
        .defocus_nm = 50.0, .dose = 30.0,
        .use_opc = true, .use_sraf = true,
        .resist_thickness = 80.0, .resist_sensitivity = 20.0,
    };

    int mx = 32, my = 32;
    double **mask = (double**)calloc(my, sizeof(double*));
    double *mask_buf = (double*)malloc(mx * my * sizeof(double));
    for (int y = 0; y < my; y++) {
        mask[y] = &mask_buf[y * mx];
        for (int x = 0; x < mx; x++) {
            mask[y][x] = ((x / 4 + y / 4) % 2 == 0) ? 1.0 : 0.0;
        }
    }

    litho_result_t litho_r;
    dfm_run_litho_sim(&litho, (const double**)mask, mx, my, &litho_r);
    printf("  Resolution: %.2f nm  Contrast: %.2f\n",
           litho_r.resolution_nm, litho_r.contrast);
    printf("  Process window: %.2f nm^2\n", litho_r.process_window_area);

    dfm_opc_correction(&litho, mask, mx, my);
    dfm_add_sraf(&litho, mask, mx, my);
    printf("  OPC + SRAF applied\n");

    free(mask_buf); free(mask);
    dfm_free_litho_result(&litho_r);

    printf("\nCMP simulation:\n");
    cmp_params_t cmp = {
        .material = CMP_COPPER,
        .removal_rate_nm_s = 3.0, .pressure_psi = 2.5,
        .velocity_rpm = 100.0, .slurry_flow = 200.0,
        .polish_time_s = 90.0,
    };

    double **density = (double**)calloc(cmap->grid_y, sizeof(double*));
    for (int y = 0; y < cmap->grid_y && y < 20; y++) {
        density[y] = (double*)calloc(cmap->grid_x, sizeof(double));
        for (int x = 0; x < cmap->grid_x && x < 20; x++) {
            density[y][x] = pr_query_congestion(cmap, x, y, 0) * 0.05;
        }
    }

    cmp_result_t cmp_r;
    dfm_run_cmp_sim(&cmp, (const double**)density,
                    cmap->grid_x, cmap->grid_y, &cmp_r);
    printf("  Planarity: %.2f nm  Dishing: %.2f nm  Erosion: %.2f nm\n",
           cmp_r.final_planarity, cmp_r.max_dishing, cmp_r.max_erosion);

    metal_fill_t fills[128];
    int num_fills = 0;
    dfm_run_metal_fill(cmap, &rules, &cmp, fills, &num_fills, 128);
    printf("  Metal fill cells: %d\n", num_fills);

    for (int y = 0; y < cmp_r.grid_y; y++) free(density[y]);
    free(density);
    dfm_free_cmp_result(&cmp_r);

    printf("\nYield analysis:\n");
    yield_params_t yp = {
        .model = YIELD_NEG_BINOMIAL,
        .defect_density = 0.10,
        .clustering_factor = 2.0,
        .critical_area_cm2 = dfm_calc_critical_area(cmap, 0.1),
        .chip_area_cm2 = 0.25,
        .systematic_yield = 0.97,
        .parametric_yield = 0.99,
    };

    yield_report_t yr;
    dfm_predict_yield(&yp, &yr);

    printf("\nYield comparison (models):\n");
    yield_model_e models[] = {
        YIELD_POISSON, YIELD_MURPHY, YIELD_SEEDS,
        YIELD_NEG_BINOMIAL, YIELD_BOSE
    };
    const char *mnames[] = {
        "Poisson", "Murphy", "Seeds", "NegBinomial", "Bose-Einstein"
    };
    for (int m = 0; m < 5; m++) {
        yp.model = models[m];
        dfm_predict_yield(&yp, &yr);
        printf("  %-15s: %.2f%%  (good dies: %.0f)\n",
               mnames[m], yr.total_yield * 100.0,
               yr.good_die_per_wafer);
    }

    yp.model = YIELD_NEG_BINOMIAL;
    dfm_predict_yield(&yp, &yr);
    printf("\nFinal yield estimate: %.2f%%\n", yr.total_yield * 100.0);

    printf("\nRedundant via insertion...\n");
    dfm_run_redundant_via_insertion(rr, &rules, 0.3);
    printf("  Vias after insertion: %d\n", rr->num_vias);

    printf("Antenna fix...\n");
    dfm_run_antenna_fix(rr, &rules);
    printf("  Antenna fixes applied\n");
}

int main(void) {
    printf("============================================\n");
    printf("  MINI-EDA-TOOLS: Full Chip Demo\n");
    printf("  Complete ASIC Implementation Flow\n");
    printf("============================================\n\n");

    netlist_t nl;
    tech_lib_t lib;
    place_instance_t inst;
    timing_graph_t tg;
    constraints_t con;
    activity_file_t act;
    power_analysis_t pa;
    congestion_map_t cmap;
    route_result_t rr;

    memset(&nl, 0, sizeof(nl));
    memset(&lib, 0, sizeof(lib));
    memset(&inst, 0, sizeof(inst));
    memset(&tg, 0, sizeof(tg));
    memset(&con, 0, sizeof(con));
    memset(&act, 0, sizeof(act));
    memset(&pa, 0, sizeof(pa));
    memset(&cmap, 0, sizeof(cmap));
    memset(&rr, 0, sizeof(rr));

    build_moderate_netlist(&nl);
    run_full_synthesis(&nl, &lib);
    run_full_place_route(&nl, &lib, &inst, &cmap, &rr);
    run_full_sta(&nl, &tg, &con);
    run_full_power(&nl, &lib, &pa, &act);
    run_full_dfm(&nl, &cmap, &rr);

    printf("\n============================================\n");
    printf("  Full Chip Demo Complete!\n");
    printf("============================================\n");

    paw_free_analysis(&pa);
    paw_free_activity(&act);
    sta_free_graph(&tg);
    sta_free_constraints(&con);
    pr_free_congestion_map(&cmap);
    for (int i = 0; i < rr.num_nets; i++) {
        free(rr.nets[i].seg_x);
        free(rr.nets[i].seg_y);
        free(rr.nets[i].seg_layer);
    }
    free(rr.nets);
    pr_free_place(&inst);
    syn_free_netlist(&nl);
    free(lib.cells);

    return 0;
}
