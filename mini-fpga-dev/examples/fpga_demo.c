/* examples/fpga_demo.c - Complete FPGA Implementation Demo */
#include "fpga_flow.h"
#include <stdio.h>

int main(void) {
    printf("===================================================\n");
    printf("  Mini-FPGA-Dev: FPGA Implementation Flow Demo\n");
    printf("===================================================\n\n");

    /* Configure the flow */
    FpgaFlowConfig cfg;
    flow_config_defaults(&cfg);
    cfg.grid_width = 8;
    cfg.grid_height = 8;
    cfg.channel_width = 8;
    cfg.lut_size = 4;
    cfg.target_freq_mhz = 100.0;
    cfg.verbose = false;

    printf("[Config] Grid: %dx%d, K=%d, W=%d, Target: %.0f MHz\n\n",
           cfg.grid_width, cfg.grid_height, cfg.lut_size,
           cfg.channel_width, cfg.target_freq_mhz);

    /* Run the complete flow */
    FpgaFlow flow;
    flow_init(&flow, &cfg);

    printf("[Step 1] Running synthesis...\n");
    if (flow_run_synthesis(&flow)) {
        printf("  -> Created boolean network with %d nodes (%d PIs, %d POs)\n",
               flow.bool_net.num_nodes, flow.bool_net.num_pi, flow.bool_net.num_po);
    }

    printf("[Step 2] Running technology mapping (FlowMap)...\n");
    if (flow_run_techmap(&flow)) {
        printf("  -> Mapped to %d LUTs (max depth=%d)\n",
               flow.lut_map.total_luts, flow.lut_map.max_depth);
    }

    printf("[Step 3] Running CLB packing (T-VPack)...\n");
    if (flow_run_packing(&flow)) {
        printf("  -> Packed into %d CLBs (%d LUTs, %d FFs)\n",
               flow.total_clbs, flow.total_luts, flow.total_ffs);
    }

    printf("[Step 4] Running placement (Simulated Annealing)...\n");
    if (flow_run_placement(&flow)) {
        printf("  -> Placed %d blocks, wirelength=%.1f\n",
               flow.placement.num_blocks, flow.wirelength);
    }

    printf("[Step 5] Running routing (PathFinder)...\n");
    if (flow_run_routing(&flow)) {
        printf("  -> Routed %d nets (%d nodes in RR-graph)\n",
               flow.routed_nets, flow.rr_graph->num_nodes);
    }

    printf("[Step 6] Running static timing analysis...\n");
    if (flow_run_timing(&flow)) {
        printf("  -> CP delay=%.2f ns, Fmax=%.1f MHz, Slack=%.2f ns\n",
               flow.sta_result.critical_path_delay,
               flow.sta_result.fmax,
               flow.sta_result.worst_slack);
    }

    printf("[Step 7] Generating bitstream...\n");
    if (flow_run_bitstream(&flow)) {
        printf("  -> Generated %d configuration frames (%u bits)\n",
               flow.bitstream->num_frames,
               flow.bitstream->num_frames * FPGA_FRAME_BITS);
    }

    printf("[Step 8] Running I/O planning...\n");
    if (flow_run_io_planning(&flow)) {
        printf("  -> Planned %d I/O pads (%d signal, %d power, %d ground)\n",
               flow.io_ring.num_pads,
               flow.io_ring.total_signal_pads,
               flow.io_ring.total_power_pads,
               flow.io_ring.total_ground_pads);
    }

    printf("\n=== Full Report ===\n");
    flow_print_report(&flow);

    flow_destroy(&flow);
    printf("\n===================================================\n");
    printf("  Demo Complete!\n");
    printf("===================================================\n");
    return 0;
}
