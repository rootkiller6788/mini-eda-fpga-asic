/* ================================================================
 * src/fpga_flow.c - Complete FPGA Implementation Flow
 * L3: End-to-end compilation pipeline
 * L6: Canonical FPGA flow: synthesis, map, pack, place, route, timing, bitstream
 * L7: Application - design download package generation
 * L8: Partial reconfiguration flow
 * L9: AI-assisted parameter auto-tuning
 * References: Vivado Design Suite, Quartus Prime, VPR Flow
 * ================================================================ */

#include "fpga_flow.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

void flow_config_defaults(FpgaFlowConfig *cfg) {
    assert(cfg);
    memset(cfg, 0, sizeof(FpgaFlowConfig));
    cfg->grid_width = 8;
    cfg->grid_height = 8;
    cfg->channel_width = 8;
    cfg->lut_size = 4;
    cfg->tech_node_nm = 28;
    cfg->target_freq_mhz = 100.0;
    cfg->clock_period_ns = 10.0;
    cfg->place_params.T_start = 100.0;
    cfg->place_params.T_end = 0.01;
    cfg->place_params.alpha = 0.95;
    cfg->place_params.moves_per_temp = 100;
    cfg->place_params.max_iterations = 10000;
    cfg->place_params.timing_tradeoff = 0.3;
    cfg->place_params.seed = 42;
    cfg->pres_fac = 0.5;
    cfg->hist_fac = 1.0;
    cfg->route_max_iter = 50;
    cfg->die_width_um = 5000.0;
    cfg->die_height_um = 5000.0;
    cfg->pad_pitch_um = 100.0;
    cfg->io_banks = 4;
    cfg->enable_timing_driven = true;
    cfg->enable_bitstream_compress = true;
    cfg->enable_partial_reconfig = false;
    cfg->verbose = false;
    strcpy(cfg->design_name, "unnamed");
    strcpy(cfg->part_name, "xc7a35t");
    strcpy(cfg->output_dir, ".");
}

void flow_init(FpgaFlow *flow, const FpgaFlowConfig *cfg) {
    assert(flow && cfg);
    memset(flow, 0, sizeof(FpgaFlow));
    flow->stage = FLOW_INPUT;
    flow->config = *cfg;
    flow->success = false;
    flow->log_len = 0;
    flow->log[0] = '\0';
    flow->max_nets = 1024;
    flow->nets = (FpgaNet*)calloc(flow->max_nets, sizeof(FpgaNet));
    assert(flow->nets);
    flow->num_nets = 0;

    /* Initialize sub-modules */
    bool_network_init(&flow->bool_net);
    lut_mapping_init(&flow->lut_map, FPGA_MAX_BOOL_NODES);
    atom_netlist_init(&flow->atom_nl);

    /* Create fabric */
    flow->fabric = fpga_fabric_create(cfg->grid_width, cfg->grid_height,
                                       cfg->channel_width, cfg->lut_size);
    assert(flow->fabric);

    placement_init(&flow->placement, cfg->grid_width, cfg->grid_height);
    io_ring_init(&flow->io_ring, cfg->die_width_um, cfg->die_height_um,
                  cfg->pad_pitch_um);
    timing_result_init(&flow->sta_result);
}

void flow_destroy(FpgaFlow *flow) {
    if (!flow) return;
    lut_mapping_destroy(&flow->lut_map);
    fpga_fabric_destroy(flow->fabric);
    placement_destroy(&flow->placement);
    if (flow->rr_graph) rr_graph_destroy(flow->rr_graph);
    if (flow->timing_graph) timing_graph_destroy(flow->timing_graph);
    if (flow->bitstream) bitstream_destroy(flow->bitstream);
    timing_result_destroy(&flow->sta_result);
    free(flow->nets);
}

void flow_log_msg(FpgaFlow *flow, const char *msg) {
    assert(flow && msg);
    int len = (int)strlen(msg);
    if (flow->log_len + len < (int)sizeof(flow->log) - 1) {
        strcpy(flow->log + flow->log_len, msg);
        flow->log_len += len;
    }
    if (flow->config.verbose) {
        printf("[FLOW] %s\n", msg);
    }
}

/* L3: Example design creation - simple adder circuit
 * Creates a 4-bit ripple-carry adder boolean network.
 * 4 full adders = 4 * 5 gates = 20 nodes */
static int flow_create_example_adder(FpgaFlow *flow) {
    FpgaBoolNetwork *bn = &flow->bool_net;
    bool_network_init(bn);

    /* Primary inputs: a[0..3], b[0..3], cin = 9 PIs */
    int pi_nodes[9];
    for (int i = 0; i < 9; i++) {
        pi_nodes[i] = bool_network_add_node(bn, GATE_BUF);
        bool_network_set_pi(bn, pi_nodes[i]);
    }

    /* For each bit position, create: sum = a XOR b XOR cin, cout = MAJ3 */
    int carry = pi_nodes[8];  /* cin */
    int sum_nodes[4], cout_nodes[4];

    for (int bit = 0; bit < 4; bit++) {
        /* XOR gate 1: a ^ b */
        int xor1 = bool_network_add_node(bn, GATE_XOR);
        bool_network_add_edge(bn, pi_nodes[bit], xor1, 0);
        bool_network_add_edge(bn, pi_nodes[bit+4], xor1, 1);

        /* XOR gate 2: (a^b) ^ cin = sum */
        int sum = bool_network_add_node(bn, GATE_XOR);
        bool_network_add_edge(bn, xor1, sum, 0);
        bool_network_add_edge(bn, carry, sum, 1);
        sum_nodes[bit] = sum;
        bool_network_set_po(bn, sum);

        /* Majority: cout = (a&b) | (a&cin) | (b&cin) using NAND+NAND+NAND */
        int and1 = bool_network_add_node(bn, GATE_NAND);
        bool_network_add_edge(bn, pi_nodes[bit], and1, 0);
        bool_network_add_edge(bn, pi_nodes[bit+4], and1, 1);

        int and2 = bool_network_add_node(bn, GATE_NAND);
        bool_network_add_edge(bn, pi_nodes[bit], and2, 0);
        bool_network_add_edge(bn, carry, and2, 1);

        int and3 = bool_network_add_node(bn, GATE_NAND);
        bool_network_add_edge(bn, pi_nodes[bit+4], and3, 0);
        bool_network_add_edge(bn, carry, and3, 1);

        /* NAND of all three NANDs = OR of ANDs */
        int cout = bool_network_add_node(bn, GATE_NAND);
        bool_network_add_edge(bn, and1, cout, 0);
        bool_network_add_edge(bn, and2, cout, 1);
        bool_network_add_edge(bn, and3, cout, 2);
        cout_nodes[bit] = cout;
        carry = cout;
    }
    bool_network_set_po(bn, carry);

    (void)sum_nodes; (void)cout_nodes;
    bool_network_levelize(bn);
    flow->stage = FLOW_SYNTHESIS;
    return 0;
}

bool flow_run_synthesis(FpgaFlow *flow) {
    assert(flow);
    flow_log_msg(flow, "Running synthesis...");

    if (flow->bool_net.num_nodes == 0) {
        flow_create_example_adder(flow);
    }

    bool_network_levelize(&flow->bool_net);
    if (flow->config.verbose) {
        bool_network_print(&flow->bool_net);
    }

    flow->stage = FLOW_SYNTHESIS;
    return true;
}

bool flow_run_techmap(FpgaFlow *flow) {
    assert(flow);
    flow_log_msg(flow, "Running technology mapping...");

    int n = flowmap_mapping(&flow->bool_net, flow->config.lut_size,
                             &flow->lut_map);
    if (n <= 0) return false;

    flow->total_luts = flow->lut_map.total_luts;
    flow->stage = FLOW_TECHMAP;
    return true;
}

bool flow_run_packing(FpgaFlow *flow) {
    assert(flow);
    flow_log_msg(flow, "Running CLB packing...");

    /* Export mapped LUTs to atom netlist */
    atom_netlist_init(&flow->atom_nl);
    int n = lut_mapping_export_to_atoms(&flow->lut_map, &flow->atom_nl);
    if (n <= 0) return false;

    FpgaPackStats pack_stats;
    pack_stats_init(&pack_stats);

    int clbs = clb_pack_timing_driven(&flow->atom_nl, flow->fabric,
                                       &pack_stats, 0.5);
    if (clbs <= 0) return false;

    flow->total_clbs = clbs;
    flow->total_luts = pack_stats.total_luts_packed;
    flow->total_ffs = pack_stats.total_ffs_packed;

    if (flow->config.verbose) {
        pack_stats_print(&pack_stats);
    }

    flow->stage = FLOW_PACKING;
    return true;
}

bool flow_run_placement(FpgaFlow *flow) {
    assert(flow);
    flow_log_msg(flow, "Running placement...");

    /* Initialize placement from packed CLBs */
    placement_init(&flow->placement, flow->config.grid_width,
                    flow->config.grid_height);

    for (int x = 0; x < flow->fabric->grid_width; x++) {
        for (int y = 0; y < flow->fabric->grid_height; y++) {
            if (flow->fabric->tiles[x][y].clb.used) {
                placement_add_block(&flow->placement,
                                    y * flow->fabric->grid_width + x);
            }
        }
    }

    if (flow->config.enable_timing_driven) {
        place_timing_driven(&flow->placement, flow->nets, flow->num_nets,
                            &flow->config.place_params, flow->timing_graph);
    } else {
        place_simulated_annealing(&flow->placement, flow->nets,
                                   flow->num_nets, &flow->config.place_params);
    }

    flow->wirelength = flow->placement.wirelength;
    flow->stage = FLOW_PLACEMENT;
    return true;
}

bool flow_run_routing(FpgaFlow *flow) {
    assert(flow);
    flow_log_msg(flow, "Running routing...");

    flow->rr_graph = rr_graph_create(flow->fabric);
    if (!flow->rr_graph) return false;

    /* Build RR graph manually for now */
    for (int x = 0; x < flow->fabric->grid_width; x++) {
        for (int y = 0; y < flow->fabric->grid_height; y++) {
            int src = rr_graph_add_node(flow->rr_graph, RR_SOURCE, x, y);
            int sink = rr_graph_add_node(flow->rr_graph, RR_SINK, x, y);
            for (int t = 0; t < flow->fabric->channel_width; t++) {
                int chan_h = rr_graph_add_node(flow->rr_graph, RR_CHANX, x, y);
                int chan_v = rr_graph_add_node(flow->rr_graph, RR_CHANY, x, y);
                rr_graph_add_edge(flow->rr_graph, src, chan_h);
                rr_graph_add_edge(flow->rr_graph, chan_h, chan_v);
                rr_graph_add_edge(flow->rr_graph, chan_v, sink);
            }
        }
    }

    if (flow->num_nets > 0 && flow->rr_graph->num_nodes > 0) {
        flow->routed_nets = route_all_nets(flow->rr_graph, flow->fabric,
                                            flow->nets, flow->num_nets);
    } else {
        flow->routed_nets = 0;
    }

    flow->stage = FLOW_ROUTING;
    return true;
}

bool flow_run_timing(FpgaFlow *flow) {
    assert(flow);
    flow_log_msg(flow, "Running static timing analysis...");

    flow->timing_graph = timing_graph_create();
    if (!flow->timing_graph) return false;

    /* Build timing graph from LUT mapping
     * Each LUT becomes a timing node */
    FpgaTimingConstraints tc;
    timing_constraints_init(&tc);
    tc.default_period = flow->config.clock_period_ns;

    int prev_node = -1;
    for (int i = 0; i < flow->lut_map.num_entries && i < 16; i++) {
        int nid = timing_graph_add_node(flow->timing_graph, TNODE_COMB);
        timing_graph_set_node_delay(flow->timing_graph, nid, 0.5);

        if (prev_node >= 0) {
            timing_graph_add_edge(flow->timing_graph, prev_node, nid, 0.5, 0.1);
        }
        prev_node = nid;
    }

    sta_analyze(flow->timing_graph, &tc, &flow->sta_result);
    flow->achieved_freq_mhz = flow->sta_result.fmax;

    if (flow->config.verbose) {
        timing_result_print(&flow->sta_result);
    }

    flow->stage = FLOW_TIMING;
    flow->success = (flow->sta_result.num_setup_violations == 0);
    return true;
}

bool flow_run_bitstream(FpgaFlow *flow) {
    assert(flow);
    flow_log_msg(flow, "Generating bitstream...");

    flow->bitstream = bitstream_create();
    if (!flow->bitstream) return false;

    /* Generate configuration frames for each used CLB */
    for (int x = 0; x < flow->fabric->grid_width; x++) {
        for (int y = 0; y < flow->fabric->grid_height; y++) {
            if (!flow->fabric->tiles[x][y].clb.used) continue;
            bitstream_add_frame(flow->bitstream, FRAME_TYPE_CLB, x, y);
        }
    }

    if (flow->config.enable_bitstream_compress) {
        bitstream_compress(flow->bitstream);
    }

    flow->stage = FLOW_BITSTREAM;
    return true;
}

bool flow_run_io_planning(FpgaFlow *flow) {
    assert(flow);
    flow_log_msg(flow, "Running I/O planning...");

    io_plan_full_ring(&flow->io_ring, &flow->placement,
                       flow->nets, flow->num_nets, 4);

    if (flow->placement.num_blocks > 0) {
        io_assign_pins(&flow->io_ring, &flow->placement,
                        flow->nets, flow->num_nets);
    }

    flow->stage = FLOW_PROGRAMMING;
    return true;
}

/* L6: Complete end-to-end flow */
bool flow_run_all(FpgaFlow *flow) {
    assert(flow);
    flow_log_msg(flow, "=== Starting Full FPGA Flow ===");

    if (!flow_run_synthesis(flow))  { flow->stage = FLOW_ERROR; return false; }
    if (!flow_run_techmap(flow))    { flow->stage = FLOW_ERROR; return false; }
    if (!flow_run_packing(flow))    { flow->stage = FLOW_ERROR; return false; }
    if (!flow_run_placement(flow))  { flow->stage = FLOW_ERROR; return false; }
    if (!flow_run_routing(flow))    { flow->stage = FLOW_ERROR; return false; }
    if (!flow_run_timing(flow))     { flow->stage = FLOW_ERROR; return false; }
    if (!flow_run_bitstream(flow))  { flow->stage = FLOW_ERROR; return false; }
    if (!flow_run_io_planning(flow)){ flow->stage = FLOW_ERROR; return false; }

    flow->stage = FLOW_COMPLETE;
    flow->success = true;
    flow_log_msg(flow, "=== Flow Complete ===");
    return true;
}

/* L7: Generate complete download package */
bool flow_generate_download(FpgaFlow *flow, const char *filename) {
    assert(flow && filename);
    if (!flow->bitstream) return false;

    char bin_file[512];
    snprintf(bin_file, sizeof(bin_file), "%s.bin", filename);
    bitstream_write_file(flow->bitstream, bin_file, BITSTREAM_BIN);

    char xdc_file[512];
    snprintf(xdc_file, sizeof(xdc_file), "%s.xdc", filename);
    io_write_pinout(&flow->io_ring, xdc_file, PINOUT_XDC);

    return true;
}

/* L8: Partial Reconfiguration Flow
 * Reconfigure a sub-region while the rest of FPGA operates.
 * Reference: Xilinx UG909 Partial Reconfiguration */
bool flow_partial_reconfig(FpgaFlow *flow, int region_x, int region_y,
                            int region_w, int region_h) {
    assert(flow);
    flow_log_msg(flow, "Running partial reconfiguration...");

    if (!flow->config.enable_partial_reconfig) return false;
    if (!flow->bitstream) return false;

    int nframes = bitstream_generate_partial(flow->bitstream, flow->fabric,
                                              region_x, region_y,
                                              region_w, region_h);
    return nframes > 0;
}

/* L9: AI-assisted parameter auto-tuning
 * Simple hill-climbing to find good placement parameters.
 * Varies T_start and alpha to minimize wirelength.
 * This is a simplified version of what ML-based tools like
 * Xilinx ML Strategy do. */
void flow_auto_tune_place_params(FpgaFlow *flow) {
    assert(flow);
    flow_log_msg(flow, "Auto-tuning placement parameters...");

    double best_wl = 1e9;
    double best_T = flow->config.place_params.T_start;
    double best_alpha = flow->config.place_params.alpha;

    /* Grid search over annealing parameters */
    double T_try[] = {50.0, 100.0, 200.0, 500.0};
    double alpha_try[] = {0.90, 0.95, 0.98};

    for (int t = 0; t < 4; t++) {
        for (int a = 0; a < 3; a++) {
            flow->config.place_params.T_start = T_try[t];
            flow->config.place_params.alpha = alpha_try[a];
            flow->config.place_params.moves_per_temp = 50;  /* fast eval */

            place_simulated_annealing(&flow->placement, flow->nets,
                                       flow->num_nets,
                                       &flow->config.place_params);

            double wl = flow->placement.wirelength;
            if (wl < best_wl) {
                best_wl = wl;
                best_T = T_try[t];
                best_alpha = alpha_try[a];
            }
        }
    }

    flow->config.place_params.T_start = best_T;
    flow->config.place_params.alpha = best_alpha;
    flow->config.place_params.moves_per_temp = 100;  /* restore */

    if (flow->config.verbose) {
        printf("Auto-tuned: T_start=%.1f alpha=%.2f (WL=%.1f)\n",
               best_T, best_alpha, best_wl);
    }
}

/* Report generation */
void flow_print_report(const FpgaFlow *flow) {
    assert(flow);
    printf("\n========================================\n");
    printf("  FPGA Implementation Flow Report\n");
    printf("========================================\n");
    printf("Design:          %s\n", flow->config.design_name);
    printf("Part:            %s\n", flow->config.part_name);
    printf("Grid:            %d x %d (K=%d, W=%d)\n",
           flow->config.grid_width, flow->config.grid_height,
           flow->config.lut_size, flow->config.channel_width);
    printf("Stage:           ");
    const char *stages[] = {"INPUT","SYNTH","TECHMAP","PACK","PLACE",
                             "ROUTE","TIMING","BITSTREAM","PROG","COMPLETE","ERROR"};
    if (flow->stage <= FLOW_ERROR)
        printf("%s\n", stages[flow->stage]);
    else
        printf("UNKNOWN\n");
    printf("Result:          %s\n", flow->success ? "SUCCESS" : "FAILED");
    flow_print_area_report(flow);
    flow_print_timing_report(flow);
    printf("========================================\n");
}

void flow_print_area_report(const FpgaFlow *flow) {
    assert(flow);
    printf("\n--- Area Report ---\n");
    printf("Total LUTs:      %d\n", flow->total_luts);
    printf("Total FFs:       %d\n", flow->total_ffs);
    printf("Total CLBs:      %d\n", flow->total_clbs);
    printf("CLB capacity:    %d\n",
           flow->config.grid_width * flow->config.grid_height);
    printf("Utilization:     %.1f%%\n",
           (double)flow->total_clbs /
           (flow->config.grid_width * flow->config.grid_height) * 100.0);
    printf("Routed nets:     %d\n", flow->routed_nets);
    printf("Wirelength:      %.1f (HPWL)\n", flow->wirelength);
}

void flow_print_timing_report(const FpgaFlow *flow) {
    assert(flow);
    printf("\n--- Timing Report ---\n");
    printf("Target freq:     %.1f MHz\n", flow->config.target_freq_mhz);
    printf("Achieved freq:   %.1f MHz\n", flow->achieved_freq_mhz);
    printf("CP delay:        %.3f ns\n", flow->sta_result.critical_path_delay);
    printf("Worst slack:     %.3f ns\n", flow->sta_result.worst_slack);
    printf("Setup viol:      %d\n", flow->sta_result.num_setup_violations);
    printf("Met timing:      %s\n",
           flow->sta_result.num_setup_violations == 0 ? "YES" : "NO");
}

void flow_print_power_estimate(const FpgaFlow *flow) {
    assert(flow);
    printf("\n--- Power Estimate ---\n");
    /* Simple power model: P = 0.5 * C * V^2 * f * alpha
     * Approximate based on CLB count and frequency */
    double C_per_clb = 5e-12;  /* 5 pF per CLB */
    double V = 1.0;           /* 1V core */
    double f = flow->achieved_freq_mhz * 1e6;
    double alpha = 0.1;       /* activity factor */

    double dynamic_pwr = 0.5 * C_per_clb * V * V * f * alpha
                         * flow->total_clbs;
    double static_pwr = flow->total_clbs * 1e-6;  /* 1 uW per CLB leakage */
    double io_pwr = flow->io_ring.total_signal_pads * 0.005;  /* 5 mW per I/O */

    printf("Dynamic power:   %.3f mW\n", dynamic_pwr * 1000.0);
    printf("Static power:    %.3f mW\n", static_pwr * 1000.0);
    printf("I/O power:       %.3f mW\n", io_pwr);
    printf("Total power:     %.3f mW\n",
           (dynamic_pwr + static_pwr) * 1000.0 + io_pwr);
}

/* L3: Load design from a simple boolean netlist file
 * Format: each line is "GATE_TYPE input1 input2 ... output" */
bool flow_load_bool(FpgaFlow *flow, const char *filename) {
    assert(flow && filename);
    FILE *fp = fopen(filename, "r");
    if (!fp) return false;

    bool_network_init(&flow->bool_net);
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char gtype[16];
        int in1, in2, out;
        if (sscanf(line, "%s %d %d %d", gtype, &in1, &in2, &out) >= 3) {
            FpgaGateType gt = GATE_AND;
            if (strcmp(gtype, "AND") == 0) gt = GATE_AND;
            else if (strcmp(gtype, "OR") == 0) gt = GATE_OR;
            else if (strcmp(gtype, "XOR") == 0) gt = GATE_XOR;
            else if (strcmp(gtype, "NAND") == 0) gt = GATE_NAND;
            else if (strcmp(gtype, "NOT") == 0) gt = GATE_NOT;

            int nid = bool_network_add_node(&flow->bool_net, gt);
            bool_network_add_edge(&flow->bool_net, in1, nid, 0);
            if (gt != GATE_NOT) {
                bool_network_add_edge(&flow->bool_net, in2, nid, 1);
            }
            (void)out;
        }
    }

    fclose(fp);
    bool_network_levelize(&flow->bool_net);
    return true;
}

bool flow_load_blif(FpgaFlow *flow, const char *filename) {
    assert(flow && filename);
    /* BLIF format parser stub - would parse .blif files */
    (void)filename;
    return true;
}
