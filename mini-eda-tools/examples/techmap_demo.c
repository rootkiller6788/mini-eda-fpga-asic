/**
 * mini-eda-tools Technology Mapping Demo
 * 演示工艺映射流程
 */
#include "tech_map.h"
#include <stdio.h>

int main(void) {
    printf("====== Technology Mapping Demo ======\n\n");

    printf("--- Building Cell Library ---\n");
    CellLibrary lib;
    cell_lib_init(&lib, "nangate45");
    cell_lib_add(&lib, "INV_X1",  GATE_NOT,  1.0, 0.02, 0.5, 2);
    cell_lib_add(&lib, "NAND2_X1", GATE_NAND, 2.0, 0.04, 1.0, 3);
    cell_lib_add(&lib, "NOR2_X1", GATE_NOR,  2.5, 0.05, 1.2, 3);
    cell_lib_add(&lib, "AND2_X1", GATE_AND,  3.0, 0.06, 1.5, 3);
    cell_lib_add(&lib, "OR2_X1",  GATE_OR,   3.0, 0.07, 1.5, 3);
    cell_lib_add(&lib, "XOR2_X1", GATE_XOR,  4.0, 0.10, 2.0, 3);
    cell_lib_print(&lib);

    printf("\n--- Building Logic Network ---\n");
    LogicNetwork net;
    network_init(&net, "example");
    int in1[] = {0,1};
    int in2[] = {2,3};
    int in3[] = {4,5};
    network_add_gate(&net, GATE_AND, in1, 2);
    network_add_gate(&net, GATE_OR,  in2, 2);
    network_add_gate(&net, GATE_NAND, in3, 2);
    network_print(&net);

    printf("\n--- Technology Mapping ---\n");
    TechMappedDesign design;
    techmap_init(&design, &net, &lib);
    int matched = techmap_match(&design);
    printf("Found %d matches\n", matched);
    int covered = techmap_cover(&design);
    printf("Covered %d gates\n", covered);
    techmap_print_mapping(&design);
    techmap_report(&design);

    return 0;
}
