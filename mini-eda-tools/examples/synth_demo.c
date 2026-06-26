/**
 * mini-eda-tools Logic Synthesis Demo
 * 演示逻辑综合优化和 Quine-McCluskey 算法
 */
#include "logic_synth.h"
#include <stdio.h>

int main(void) {
    printf("====== Logic Synthesis Demo ======\n\n");

    printf("--- Network Optimization ---\n");
    LogicNetwork net;
    network_init(&net, "demo_circuit");
    int inputs1[] = {0, 1};
    int inputs2[] = {2, 3};
    network_add_gate(&net, GATE_AND, inputs1, 2);
    network_add_gate(&net, GATE_OR, inputs2, 2);
    int in_nand[] = {4, 5};
    network_add_gate(&net, GATE_NAND, in_nand, 2);
    int in_not[] = {6};
    network_add_gate(&net, GATE_NOT, in_not, 1);
    network_print(&net);

    printf("\n--- After Optimization ---\n");
    network_optimize(&net);
    network_constant_fold(&net);
    network_print(&net);

    printf("\n--- Quine-McCluskey Minimization ---\n");
    MintermTable mt;
    minterm_init(&mt, 3);
    int m1[] = {0,0,0}, m2[] = {0,0,1}, m3[] = {0,1,1}, m4[] = {1,1,1};
    minterm_add(&mt, m1, 3);
    minterm_add(&mt, m2, 3);
    minterm_add(&mt, m3, 3);
    minterm_add(&mt, m4, 3);
    int dc[] = {1,0,0};
    minterm_dont_care(&mt, dc, 3);
    minterm_print(&mt);

    MintermTable result;
    if (synth_quine_mccluskey(&mt, &result)) {
        printf("\n--- After Reduction ---\n");
        minterm_print(&result);
        int essential[MAX_IMPLICANTS];
        int n = minterm_essential_prime(&result, essential);
        printf("Essential prime implicants: %d\n", n);
    }

    return 0;
}
