#include "synthesis.h"
#include "static_timing.h"
#include "power_analysis.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== EDA STA & Power Analysis Example ===\n\n");

    netlist_t nl;
    syn_create_netlist("cpu_core", "riscv_core", &nl);

    int clk=0; syn_add_gate(&nl, GATE_BUF, "clk"); syn_set_clock(&nl,clk);
    int a=1;   syn_add_gate(&nl, GATE_INV, "a");  syn_set_pi(&nl,a);
    int b=2;   syn_add_gate(&nl, GATE_INV, "b");  syn_set_pi(&nl,b);
    int n1=3;  syn_add_gate(&nl, GATE_NAND, "u1");
    int n2=4;  syn_add_gate(&nl, GATE_NOR,  "u2");
    int n3=5;  syn_add_gate(&nl, GATE_XOR,  "u3");
    int n4=6;  syn_add_gate(&nl, GATE_AND,  "u4");
    int f1=7;  syn_add_gate(&nl, GATE_DFF,  "ff1");
    int f2=8;  syn_add_gate(&nl, GATE_DFF,  "ff2");
    int y=9;   syn_add_gate(&nl, GATE_BUF,  "y");  syn_set_po(&nl,y);

    syn_connect(&nl, a,0, n1,0);
    syn_connect(&nl, b,0, n1,1);
    syn_connect(&nl, n1,0, n2,0);
    syn_connect(&nl, a,0, n2,1);
    syn_connect(&nl, n2,0, n3,0);
    syn_connect(&nl, b,0, n3,1);
    syn_connect(&nl, n3,0, n4,0);
    syn_connect(&nl, n4,0, f1,0);
    syn_connect(&nl, f1,0, f2,0);
    syn_connect(&nl, f2,0, y,0);

    printf("Netlist: %d gates\n", nl.num_gates);

    printf("\n--- Static Timing Analysis ---\n");
    timing_graph_t graph;
    constraints_t con;
    sta_init_constraints(&con);
    sta_add_clock(&con, "sys_clk", 2000.0, 50.0, 20.0);
    sta_set_io_delay(&con, "a", true, 200.0, 10.0);
    sta_set_io_delay(&con, "b", true, 200.0, 10.0);
    sta_set_io_delay(&con, "y", false, 300.0, 15.0);

    sta_build_timing_graph(&graph, &nl, &con);
    printf("Timing graph: %d nodes, %d arcs\n",
           graph.num_nodes, graph.num_arcs);

    sta_result_t sta_r;
    sta_run_analysis(&graph, &con, &sta_r);
    printf("\nSTA Results:\n");
    sta_print_result(&sta_r);

    printf("\nCritical path:\n");
    sta_print_path(&sta_r.worst_setup);

    printf("\nWorst 3 paths:\n");
    sta_get_worst_paths(&graph, &sta_r, 3);
    for (int i = 0; i < 3 && i < sta_r.num_paths; i++) {
        sta_print_path(&sta_r.paths[i]);
    }

    printf("\n--- Power Analysis ---\n");
    activity_file_t act;
    paw_load_activity_file(&act, "activity.saif");
    paw_estimate_activity(&act, &nl);

    tech_lib_t lib;
    syn_load_tech_lib("stdcell_28nm.lib", &lib);

    power_analysis_t pow_a;
    paw_create_analysis(&pow_a, &nl);
    pow_a.params.vdd_v = 1.0;
    pow_a.params.frequency_mhz = 500.0;
    pow_a.params.temperature_c = 85.0;

    paw_run_analysis(&pow_a, &nl, &lib, &act, &sta_r);
    paw_print_report(&pow_a.report);

    printf("\n--- Power Optimization ---\n");

    printf("Applying clock gating...\n");
    paw_apply_clock_gating(&pow_a, &nl, CG_AUTO_ICG);
    paw_run_analysis(&pow_a, &nl, &lib, &act, &sta_r);
    printf("After clock gating: %.4f mW\n", pow_a.report.total_mw);

    printf("Applying multi-Vt optimization...\n");
    paw_apply_multi_vt(&pow_a, &nl, &lib, 1000.0);
    paw_run_analysis(&pow_a, &nl, &lib, &act, &sta_r);
    printf("After multi-Vt: %.4f mW\n", pow_a.report.total_mw);

    printf("\nEnergy per operation: %.2f pJ\n",
           paw_energy_per_op(&pow_a, 500.0));

    free(sta_r.paths);
    sta_free_graph(&graph);
    sta_free_constraints(&con);
    paw_free_analysis(&pow_a);
    paw_free_activity(&act);
    syn_free_netlist(&nl);
    free(lib.cells);

    return 0;
}
