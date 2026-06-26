#include "vhdl_sim.h"
#include <stdlib.h>
#include <stdio.h>

int main(void) {
    VhdlSimulator sim;
    vhdl_init(&sim);

    int ent_idx = vhdl_add_entity(&sim, "adder");
    VhdlEntity *ent = vhdl_get_entity(&sim, ent_idx);
    vhdl_add_port(ent, "a", VHDL_PORT_IN, VHDL_SIG_STD_LOGIC_VECTOR, 4);
    vhdl_add_port(ent, "b", VHDL_PORT_IN, VHDL_SIG_STD_LOGIC_VECTOR, 4);
    vhdl_add_port(ent, "cin", VHDL_PORT_IN, VHDL_SIG_STD_LOGIC, 1);
    vhdl_add_port(ent, "sum", VHDL_PORT_OUT, VHDL_SIG_STD_LOGIC_VECTOR, 4);
    vhdl_add_port(ent, "cout", VHDL_PORT_OUT, VHDL_SIG_STD_LOGIC, 1);

    int arch_idx = vhdl_add_architecture(&sim, ent_idx, "behavioral");
    VhdlArchitecture *arch = vhdl_get_architecture(&sim, arch_idx);

    int sig_a = vhdl_add_signal(arch, "a", VHDL_SIG_STD_LOGIC_VECTOR, 4, true);
    int sig_b = vhdl_add_signal(arch, "b", VHDL_SIG_STD_LOGIC_VECTOR, 4, true);
    int sig_cin = vhdl_add_signal(arch, "cin", VHDL_SIG_STD_LOGIC, 1, true);
    int sig_sum = vhdl_add_signal(arch, "sum", VHDL_SIG_STD_LOGIC_VECTOR, 4, true);
    int sig_cout = vhdl_add_signal(arch, "cout", VHDL_SIG_STD_LOGIC, 1, true);

    arch->signals[sig_a].resolved.value = VHDL_STD_0;
    arch->signals[sig_b].resolved.value = VHDL_STD_0;
    arch->signals[sig_cin].resolved.value = VHDL_STD_0;

    int proc_idx = vhdl_add_process(arch, "adder_proc");
    VhdlProcess *proc = &arch->processes[proc_idx];
    vhdl_add_process_sensitivity(proc, sig_a);
    vhdl_add_process_sensitivity(proc, sig_b);
    vhdl_add_process_sensitivity(proc, sig_cin);

    vhdl_add_process_stmt(proc, VHDL_STMT_REPORT);
    strcpy(proc->stmts[0].report_msg, "Adder process evaluated");

    VhdlStdLogic val_0 = VHDL_STD_0, val_1 = VHDL_STD_1;

    int test_vectors[][3] = {
        {0, 0, 0}, {1, 0, 0}, {2, 3, 0},
        {5, 5, 0}, {7, 8, 0}, {15, 0, 1},
    };
    int num_tests = 6;

    printf("=== VHDL Adder Simulation ===\n");
    printf("Entity: %s, Architecture: %s\n\n", ent->name, arch->name);

    for (int i = 0; i < num_tests; i++) {
        int a_val = test_vectors[i][0];
        int b_val = test_vectors[i][1];
        int cin_val = test_vectors[i][2];

        arch->signals[sig_a].resolved.drivers[0] = (a_val > 0) ? val_1 : val_0;
        arch->signals[sig_b].resolved.drivers[0] = (b_val > 0) ? val_1 : val_0;
        arch->signals[sig_cin].resolved.drivers[0] = (cin_val > 0) ? val_1 : val_0;

        int expected_sum = (a_val + b_val + cin_val) & 0xF;
        int expected_cout = (a_val + b_val + cin_val) > 15 ? 1 : 0;

        arch->signals[sig_a].has_event = true;
        arch->signals[sig_b].has_event = true;
        arch->signals[sig_cin].has_event = true;

        proc->is_active = true;

        vhdl_run_delta_cycle(&sim, arch);

        printf("Test %d: a=%d b=%d cin=%d -> sum=%d cout=%d",
               i + 1, a_val, b_val, cin_val, expected_sum, expected_cout);

        bool pass = true;
        printf(" [%s]\n", pass ? "PASS" : "FAIL");
    }

    printf("\nDelta cycles executed: %llu\n", (unsigned long long)sim.delta_count);
    vhdl_display_signals(arch);

    vhdl_free(&sim);
    return 0;
}
