#include "synthesis.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== EDA Synthesis Example ===\n\n");

    netlist_t nl;
    syn_create_netlist("alu_core", "alpha_alu", &nl);

    int a  = 0; syn_add_gate(&nl, GATE_INV, "a");  syn_set_pi(&nl, a);
    int b  = 1; syn_add_gate(&nl, GATE_INV, "b");  syn_set_pi(&nl, b);
    int c  = 2; syn_add_gate(&nl, GATE_INV, "c");  syn_set_pi(&nl, c);
    int clk= 3; syn_add_gate(&nl, GATE_INV, "clk"); syn_set_clock(&nl,clk);

    int n1 = 4; syn_add_gate(&nl, GATE_NAND, "u_nand");
    int n2 = 5; syn_add_gate(&nl, GATE_NOR,  "u_nor");
    int n3 = 6; syn_add_gate(&nl, GATE_AND,  "u_and");
    int n4 = 7; syn_add_gate(&nl, GATE_OR,   "u_or");
    int n5 = 8; syn_add_gate(&nl, GATE_XOR,  "u_xor");
    int n6 = 9; syn_add_gate(&nl, GATE_INV,  "u_inv");

    int f1 = 10; syn_add_gate(&nl, GATE_DFF, "ff_a");
    int f2 = 11; syn_add_gate(&nl, GATE_DFF, "ff_b");

    int y1 = 12; syn_add_gate(&nl, GATE_BUF, "y1"); syn_set_po(&nl, y1);
    int y2 = 13; syn_add_gate(&nl, GATE_BUF, "y2"); syn_set_po(&nl, y2);

    syn_connect(&nl, a,0, n1,0);
    syn_connect(&nl, b,0, n1,1);
    syn_connect(&nl, c,0, n2,0);
    syn_connect(&nl, a,0, n2,1);
    syn_connect(&nl, n1,0, n5,0);
    syn_connect(&nl, n2,0, n5,1);
    syn_connect(&nl, n5,0, n6,0);
    syn_connect(&nl, n6,0, f1,0);
    syn_connect(&nl, n3,0, f2,0);
    syn_connect(&nl, f1,0, y1,0);
    syn_connect(&nl, f2,0, y2,0);

    printf("Original netlist:\n");
    syn_print_netlist(&nl);

    tech_lib_t lib;
    syn_load_tech_lib("stdcell_28nm.lib", &lib);
    printf("\nTech library: '%s'  cells=%d  process=%dnm\n",
           lib.name, lib.num_cells, (int)lib.process_nm);

    printf("\nRunning boolean optimization...\n");
    syn_run_boolean_optimize(&nl);

    synth_params_t params = {
        .target_freq_mhz = 500.0,
        .max_area_um2 = 1000.0,
        .max_leakage_uw = 100.0,
        .allow_multi_vt = true,
        .allow_clock_gating = true,
        .preserve_hierarchy = false,
        .optimize_timing = true,
        .optimize_area = true,
        .optimize_power = true,
        .max_fanout = 16,
        .effort_level = 3,
    };

    synth_result_t result;
    syn_synthesize(&nl, &lib, &params, &result);

    printf("\nSynthesis complete!\n");
    syn_print_stats(&result);
    printf("Estimated critical path: %.2f ps\n",
           syn_estimate_delay(&nl, &lib));

    syn_write_verilog(&nl, "build/synth_output.v");
    printf("\nVerilog netlist written to build/synth_output.v\n");

    syn_free_netlist(&nl);
    free(lib.cells);

    return 0;
}
