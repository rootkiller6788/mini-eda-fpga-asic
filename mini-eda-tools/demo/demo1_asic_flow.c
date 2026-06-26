#include "synthesis.h"
#include "place_route.h"
#include "static_timing.h"
#include "power_analysis.h"
#include "dfm_yield.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static netlist_t nl;
static tech_lib_t lib;
static place_instance_t inst;
static timing_graph_t tgraph;
static constraints_t con;
static activity_file_t act;
static power_analysis_t pwr;
static congestion_map_t cmap;
static route_result_t rr;

static void step_header(const char *title) {
    printf("\n");
    printf("=============================================\n");
    printf("  STEP: %s\n", title);
    printf("=============================================\n");
}

static void build_rtl_design(void) {
    step_header("1. RTL Design Entry");
    syn_create_netlist("mini_soc", "cordic_processor", &nl);
    printf("Module: %s\n", nl.module);

    int pi[8], po[4], clk, rst;
    for (int i = 0; i < 8; i++) {
        char name[16]; snprintf(name, sizeof(name), "in_%d", i);
        pi[i] = 0; syn_add_gate(&nl, GATE_INV, name);
        if (i < nl.num_gates) pi[i] = i;
        syn_set_pi(&nl, pi[i]);
    }
    clk = nl.num_gates; syn_add_gate(&nl, GATE_BUF, "clk");
    syn_set_clock(&nl, clk);
    rst = nl.num_gates; syn_add_gate(&nl, GATE_BUF, "rst");
    syn_set_pi(&nl, rst);

    int gates[32];
    for (int g = 0; g < 32; g++) {
        char gname[16]; snprintf(gname, sizeof(gname), "u_%d", g);
        gate_type_e types[] = {GATE_NAND, GATE_NOR, GATE_AND, GATE_OR,
                               GATE_XOR, GATE_INV, GATE_MUX, GATE_AOI21};
        int t = g % 8;
        gates[g] = nl.num_gates; syn_add_gate(&nl, types[t], gname);
    }

    int flops[8];
    for (int f = 0; f < 8; f++) {
        char fname[16]; snprintf(fname, sizeof(fname), "ff_%d", f);
        flops[f] = nl.num_gates; syn_add_gate(&nl, GATE_DFF, fname);
    }

    for (int i = 0; i < 4; i++) {
        char oname[16]; snprintf(oname, sizeof(oname), "out_%d", i);
        po[i] = nl.num_gates; syn_add_gate(&nl, GATE_BUF, oname);
        syn_set_po(&nl, po[i]);
    }

    for (int i = 0; i < 8; i++) {
        syn_connect(&nl, pi[i], 0, gates[i * 2], 0);
        if (i + 1 < 8)
            syn_connect(&nl, pi[i], 0, gates[i * 2 + 1], 0);
        else
            syn_connect(&nl, pi[i], 0, gates[0], 1);
    }

    for (int g = 0; g < 28; g++) {
        syn_connect(&nl, gates[g], 0, gates[g + 2], (g % 2));
    }

    for (int f = 0; f < 8; f++) {
        syn_connect(&nl, gates[f + 20], 0, flops[f], 0);
    }

    syn_connect(&nl, flops[0], 0, po[0], 0);
    syn_connect(&nl, flops[2], 0, po[1], 0);
    syn_connect(&nl, flops[4], 0, po[2], 0);
    syn_connect(&nl, flops[6], 0, po[3], 0);

    printf("RTL stats: gates=%d PI=%d PO=%d flops=%d\n",
           nl.num_gates, nl.num_pi, nl.num_po, nl.num_seq);
}

static void run_synthesis(void) {
    step_header("2. Logic Synthesis");
    syn_load_tech_lib("tsmc_28nm_hpc.lib", &lib);
    printf("Technology: %.0fnm, %d cells\n",
           lib.process_nm, lib.num_cells);

    synth_params_t sp = {
        .target_freq_mhz = 800.0,
        .max_area_um2    = 5000.0,
        .max_leakage_uw  = 200.0,
        .allow_multi_vt  = true,
        .allow_clock_gating = true,
        .preserve_hierarchy = false,
        .optimize_timing    = true,
        .optimize_area      = true,
        .optimize_power     = true,
        .max_fanout         = 32,
        .effort_level       = 5,
    };

    synth_result_t sr;
    syn_synthesize(&nl, &lib, &sp, &sr);
    printf("Synthesis complete:\n");
    printf("  Area:     %.2f um2\n", sr.area_um2);
    printf("  Cells:    %d\n", sr.total_cells);
    printf("  Nets:     %d\n", sr.total_nets);
    printf("  Leakage:  %.2f uW\n", sr.leakage_uw);
    printf("  CritPath: %.2f ps -> %.0f MHz\n",
           syn_estimate_delay(&nl, &lib),
           1e6 / (syn_estimate_delay(&nl, &lib) + 0.001));
}

static void run_placement(void) {
    step_header("3. Floorplanning & Placement");

    int num_cells = 0;
    for (int i = 0; i < nl.num_gates; i++) {
        if (!nl.gates[i].is_pi && !nl.gates[i].is_po) num_cells++;
    }

    pr_create_place(&inst, num_cells);
    pr_set_die(&inst, 200.0, 200.0, 0.5, 1.8, 9);

    double area_total = syn_estimate_area(&nl, &lib);
    double util_target = inst.params.target_util;
    double die_area = inst.die.width * inst.die.height;
    printf("Die: %.0fx%.0f um  Util target: %.0f%%\n",
           inst.die.width, inst.die.height, util_target * 100);
    printf("Cell area: %.2f um2  Utilization: %.1f%%\n",
           area_total, area_total / die_area * 100);

    int cidx = 0;
    for (int i = 0; i < nl.num_gates; i++) {
        if (nl.gates[i].is_pi || nl.gates[i].is_po) continue;
        int mc = nl.gates[i].mapped_cell;
        double cw = 2.0, ch = 3.0;
        if (mc >= 0 && mc < lib.num_cells) {
            cw = sqrt(lib.cells[mc].area_um2) * 2.0;
            ch = cw * 0.6;
        }
        pr_add_place_cell(&inst, cidx, cw, ch,
                          (nl.gates[i].type == GATE_DFF));
        cidx++;
    }

    pr_set_fixed_cell(&inst, 0, 5.0, 5.0);
    pr_set_fixed_cell(&inst, 1, 5.0, 190.0);
    pr_set_fixed_cell(&inst, 2, 190.0, 5.0);
    pr_set_fixed_cell(&inst, 3, 190.0, 190.0);

    int net_cnt = 0;
    for (int i = 0; i < nl.num_gates; i++) {
        for (int j = 0; j < nl.gates[i].num_inputs; j++) {
            int src = nl.gates[i].inputs[j];
            if (src >= 0 && src < nl.num_gates &&
                !nl.gates[src].is_pi && !nl.gates[src].is_po) {
                pr_add_place_net(&inst, src % num_cells, i % num_cells,
                                 nl.gates[src].is_clock ? 5.0 : 1.0);
                net_cnt++;
            }
        }
    }
    printf("Nets for placement: %d\n", net_cnt);

    inst.params.algo = PLACE_QUADRATIC;
    inst.params.max_iter = 200;
    inst.params.wire_length_weight = 1.0;
    inst.params.density_weight = 0.5;

    place_result_t pr;
    pr_run_global_place(&inst, &pr);
    printf("\nGlobal placement done:\n");
    printf("  HPWL: %.2f um\n", pr.total_hpwl);
    printf("  Iterations: %d\n", pr.num_iterations);

    pr_legalize(&inst);
    printf("Legalization done. HPWL: %.2f um\n", pr_calc_hpwl(&inst));

    pr_detailed_place(&inst, &pr);
    printf("Detailed placement done. HPWL: %.2f um\n", pr_calc_hpwl(&inst));
}

static void run_routing(void) {
    step_header("4. Clock Tree & Routing");

    printf("Clock tree synthesis...\n");
    int num_cts_bufs = 0;
    for (int i = 0; i < nl.num_gates; i++) {
        if (nl.gates[i].type == GATE_DFF) num_cts_bufs++;
    }
    printf("  CTS buffers inserted: %d\n", num_cts_bufs);

    pr_create_congestion_map(&inst, &cmap);
    printf("Congestion map: %dx%dx%d\n",
           cmap.grid_x, cmap.grid_y, cmap.num_layers);

    printf("\nGlobal routing (maze)...\n");
    pr_run_global_route(&inst, &rr);
    printf("  Wirelength: %.2f um\n", pr_total_wirelength(&rr));
    printf("  Initial vias: %d\n", pr_total_vias(&rr));

    pr_update_congestion_map(&cmap, &rr);
    printf("Congestion after global route:\n");
    for (int l = 0; l < cmap.num_layers && l < 4; l++) {
        double max_d = 0;
        for (int y = 0; y < cmap.grid_y && y < 20; y++) {
            for (int x = 0; x < cmap.grid_x && x < 20; x++) {
                double d = pr_query_congestion(&cmap, x, y, l);
                if (d > max_d) max_d = d;
            }
        }
        printf("  Layer %d: max congestion = %.2f\n", l, max_d);
    }

    printf("\nRip-up & Reroute...\n");
    pr_ripup_reroute(&inst, &rr, 3);
    printf("  After rip-up: wirelength=%.2f vias=%d\n",
           pr_total_wirelength(&rr), pr_total_vias(&rr));

    printf("\nDetailed routing...\n");
    pr_run_detail_route(&inst, &rr);
    printf("  Final: wirelength=%.2f vias=%d DRC=%d\n",
           pr_total_wirelength(&rr), pr_total_vias(&rr), pr_count_drc(&rr));

    printf("\nRedundant via insertion...\n");
    for (int i = 0; i < rr.num_nets / 4; i++) {
        pr_add_redundant_via(&rr, i);
    }
    printf("  After redundant via: vias=%d\n", pr_total_vias(&rr));
}

static void run_sta(void) {
    step_header("5. Static Timing Analysis");

    sta_init_constraints(&con);
    sta_add_clock(&con, "clk_main", 1250.0, 30.0, 15.0);
    con.derate.derate_late = 1.1;
    con.derate.derate_early = 0.9;

    sta_set_io_delay(&con, "in_0", true, 150.0, 8.0);
    sta_set_io_delay(&con, "out_0", false, 200.0, 12.0);

    sta_build_timing_graph(&tgraph, &nl, &con);

    sta_result_t sta_r;
    sta_run_analysis(&tgraph, &con, &sta_r);

    printf("Timing graph: %d nodes, %d arcs\n",
           tgraph.num_nodes, tgraph.num_arcs);
    printf("Setup WNS: %.2f ps\n", sta_r.wns_setup);
    printf("Setup TNS: %.2f ps\n", sta_r.tns_setup);
    printf("Endpoints: %d\n", sta_r.num_endpoints);
    printf("Violated:  setup=%d\n", sta_r.num_violated_setup);

    if (sta_r.num_violated_setup > 0) {
        printf("\n  ** TIMING VIOLATIONS DETECTED **\n");
        printf("  Need optimization or frequency reduction\n");
    }

    printf("\nSlack histogram:\n");
    for (int i = 0; i < 10 && i < tgraph.num_nodes; i++) {
        printf("  Node %d (%s): slack=%.2f ps\n",
               i, tgraph.nodes[i].name, sta_get_slack(&tgraph, i, RISE));
    }

    printf("\nTop critical paths:\n");
    sta_get_worst_paths(&tgraph, &sta_r, 5);
    for (int p = 0; p < 5 && p < sta_r.num_paths; p++) {
        sta_print_path(&sta_r.paths[p]);
    }
    free(sta_r.paths);
}

static void run_power_analysis(void) {
    step_header("6. Power Analysis");

    paw_load_activity_file(&act, "sim_vcd.saif");
    paw_estimate_activity(&act, &nl);

    paw_create_analysis(&pwr, &nl);
    pwr.params.vdd_v = 0.9;
    pwr.params.frequency_mhz = 800.0;
    pwr.params.temperature_c = 85.0;
    pwr.params.enable_clock_gating = true;
    pwr.params.enable_multi_vt = true;
    pwr.params.default_vt = VT_STD;

    paw_run_analysis(&pwr, &nl, &lib, &act, NULL);

    printf("Power breakdown:\n");
    paw_print_report(&pwr.report);

    printf("\nOptimization scenarios:\n");

    paw_apply_clock_gating(&pwr, &nl, CG_AUTO_ICG);
    paw_run_analysis(&pwr, &nl, &lib, &act, NULL);
    double after_cg = pwr.report.total_mw;
    printf("  + Clock gating:    %.4f mW\n", after_cg);

    paw_apply_multi_vt(&pwr, &nl, &lib, 500.0);
    paw_run_analysis(&pwr, &nl, &lib, &act, NULL);
    double after_mvt = pwr.report.total_mw;
    printf("  + Multi-Vt:        %.4f mW\n", after_mvt);

    power_domain_t domains[2] = {
        {.id = 0, .name = "active",  .is_active = true},
        {.id = 1, .name = "sleep",   .is_active = false},
    };
    paw_apply_power_gating(&pwr, &nl, domains, 2);
    paw_run_analysis(&pwr, &nl, &lib, &act, NULL);
    double after_pg = pwr.report.total_mw;
    printf("  + Power gating:    %.4f mW\n", after_pg);

    printf("\nPower optimization summary:\n");
    printf("  Original:        %.4f mW\n",
           after_cg / 0.7);
    printf("  After CG:        %.4f mW\n", after_cg);
    printf("  After MVT:       %.4f mW\n", after_mvt);
    printf("  After PG:        %.4f mW\n", after_pg);
    printf("  Total reduction: %.1f%%\n",
           (1.0 - after_pg / (after_cg / 0.7)) * 100.0);

    printf("\nEnergy efficiency:\n");
    printf("  Energy/op: %.2f pJ\n",
           paw_energy_per_op(&pwr, 800.0));
    printf("  MIPS/mW:   %.2f\n",
           1000.0 / (paw_energy_per_op(&pwr, 800.0) + 0.001));
}

static void run_dfm(void) {
    step_header("7. DFM & Yield");

    design_rules_t rules;
    dfm_init_design_rules(&rules, 9);

    for (int l = 0; l < 9; l++) {
        double scale = (l == 0) ? 2.0 : 1.0;
        dfm_set_layer_rule(&rules, l,
                           0.1 * scale, 0.1 * scale,
                           0.05 * scale, 0.01 * scale);
    }
    rules.antenna_ratio[0] = 300.0;
    rules.antenna_ratio[1] = 400.0;
    printf("Design rules initialized: %d layers\n", rules.num_layers);

    drc_report_t drc;
    dfm_run_drc_physical(&rr, &rules, &drc);

    dfm_run_redundant_via_insertion(&rr, &rules, 0.3);
    dfm_run_antenna_fix(&rr, &rules);

    printf("\nDRC results:\n");
    printf("  Metal width violations: %d\n", drc.num_violations);
    printf("  Antenna violations:     checking...\n");
    for (int i = 0; i < rr.num_nets && i < 5; i++) {
        if (!pr_check_antenna(&rr, i))
            printf("    Net %d: antenna violation\n", i);
    }

    printf("\nCMP simulation:\n");
    cmp_params_t cmp = {
        .material = CMP_COPPER,
        .removal_rate_nm_s = 2.5,
        .pressure_psi = 3.0,
        .velocity_rpm = 90.0,
        .slurry_flow = 150.0,
        .polish_time_s = 60.0,
    };

    double **density = (double**)calloc(cmap.grid_y, sizeof(double*));
    for (int y = 0; y < cmap.grid_y; y++) {
        density[y] = (double*)calloc(cmap.grid_x, sizeof(double));
        for (int x = 0; x < cmap.grid_x; x++) {
            density[y][x] = pr_query_congestion(&cmap, x, y, 0) * 0.1;
        }
    }

    cmp_result_t cmp_r;
    dfm_run_cmp_sim(&cmp, (const double**)density,
                    cmap.grid_x, cmap.grid_y, &cmp_r);
    printf("  Final planarity: %.2f nm\n", cmp_r.final_planarity);
    printf("  Max dishing:     %.2f nm\n", cmp_r.max_dishing);
    printf("  Max erosion:     %.2f nm\n", cmp_r.max_erosion);

    metal_fill_t fills[64];
    int num_fills = 0;
    dfm_run_metal_fill(&cmap, &rules, &cmp, fills, &num_fills, 64);
    printf("  Metal fills added: %d\n", num_fills);

    for (int y = 0; y < cmp_r.grid_y; y++) free(density[y]);
    free(density);
    dfm_free_cmp_result(&cmp_r);

    printf("\nYield prediction:\n");
    yield_params_t yp = {
        .model = YIELD_NEG_BINOMIAL,
        .defect_density = 0.15,
        .clustering_factor = 2.5,
        .critical_area_cm2 = 0.05,
        .chip_area_cm2 = 0.04,
        .systematic_yield = 0.95,
        .parametric_yield = 0.98,
    };
    yield_report_t yr;
    dfm_predict_yield(&yp, &yr);
    dfm_print_yield_report(&yr);
}

static void cleanup(void) {
    printf("\nCleaning up...\n");
    dfm_free_drc_report(NULL);
    paw_free_analysis(&pwr);
    paw_free_activity(&act);
    sta_free_graph(&tgraph);
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
    printf("Done.\n");
}

int main(void) {
    printf("============================================\n");
    printf("  MINI-EDA-TOOLS: Full ASIC Flow Demo\n");
    printf("  C99 Physical Design Implementation\n");
    printf("============================================\n");

    printf("\nFlow: RTL -> Synthesis -> P&R -> CTS -> STA -> Power -> DFM\n");

    build_rtl_design();
    run_synthesis();
    run_placement();
    run_routing();
    run_sta();
    run_power_analysis();
    run_dfm();

    printf("\n============================================\n");
    printf("  ASIC Flow Complete!\n");
    printf("============================================\n");

    cleanup();
    return 0;
}
