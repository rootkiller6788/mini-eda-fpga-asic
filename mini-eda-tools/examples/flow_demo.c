/**
 * mini-eda-tools Full EDA Flow Demo
 * 演示完整的 RTL → GDSII EDA 流程
 */
#include "eda_flow.h"
#include <stdio.h>

int main(void) {
    printf("====== Full EDA Flow Demo ======\n\n");

    EdaFlow flow;
    flow_init(&flow);

    LogicNetwork net;
    network_init(&net, "alu4");
    int in1[] = {0,1};
    int in2[] = {2,3};
    int in3[] = {4,5};
    int in4[] = {6,7};
    int in5[] = {8,1};
    network_add_gate(&net, GATE_AND,  in1, 2);
    network_add_gate(&net, GATE_OR,   in2, 2);
    network_add_gate(&net, GATE_NAND, in3, 2);
    network_add_gate(&net, GATE_XOR,  in4, 2);
    network_add_gate(&net, GATE_NOR,  in5, 2);

    cell_lib_init(&flow.lib, "example_lib");
    cell_lib_add(&flow.lib, "INV_X1",  GATE_NOT,  1.0, 0.02, 0.5, 2);
    cell_lib_add(&flow.lib, "NAND2_X1", GATE_NAND, 2.0, 0.04, 1.0, 3);
    cell_lib_add(&flow.lib, "NOR2_X1", GATE_NOR,  2.5, 0.05, 1.2, 3);
    cell_lib_add(&flow.lib, "AND2_X1", GATE_AND,  3.0, 0.06, 1.5, 3);
    cell_lib_add(&flow.lib, "OR2_X1",  GATE_OR,   3.0, 0.07, 1.5, 3);
    cell_lib_add(&flow.lib, "XOR2_X1", GATE_XOR,  4.0, 0.10, 2.0, 3);

    printf("Running full EDA flow...\n\n");
    flow_run_synthesis(&flow, &net);
    flow_run_techmap(&flow, NULL);
    flow_run_place(&flow, 100.0, 100.0);
    flow_run_route(&flow);
    flow_run_sta(&flow, 5.0);
    flow.stage = FLOW_DONE;
    flow.success = true;

    flow_print_report(&flow);

    return 0;
}
