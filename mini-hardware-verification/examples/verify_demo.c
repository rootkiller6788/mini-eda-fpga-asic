#include "testbench.h"
#include "assertion.h"
#include "coverage.h"
#include "formal_check.h"
#include "uvm_like.h"
#include <stdio.h>
#include <stdlib.h>

static bool check_alu_output(void) { return 1; }
static bool check_always_positive(void) { return 1; }
static FmState alu_next(FmState *s) { FmState ns = *s; fm_state_set(&ns, 0, fm_state_get(s, 0) + 1); return ns; }
static bool alu_prop(FmState *s) { return fm_state_get(s, 0) >= 0; }

int main(void) {
    printf("====== Hardware Verification Demo ======\n\n");

    printf("--- Testbench ---\n");
    Testbench tb; tb_init(&tb, "alu_tb");
    tb.verbose = true;
    tb_add_signal(&tb, "clk", 1);
    tb_add_signal(&tb, "rst_n", 1);
    tb_add_signal(&tb, "result", 8);
    tb_drive(&tb, "clk", 1);
    tb_drive(&tb, "rst_n", 0);
    tb_cycle(&tb);
    tb_drive(&tb, "rst_n", 1);
    tb_compare(&tb, "result", 0);
    tb_run(&tb, 5);
    tb_report(&tb);

    printf("\n--- Assertions ---\n");
    AssertEngine ae; assert_init(&ae);
    assert_add_property(&ae, "alu_out_valid", ASSERT_CONCURRENT, check_alu_output);
    assert_add_property(&ae, "always_pos", ASSERT_IMMEDIATE, check_always_positive);
    assert_check(&ae, "alu_out_valid");
    assert_report(&ae);

    printf("\n--- Coverage ---\n");
    Coverage cov; cov_init(&cov);
    int g0 = cov_new_group(&cov, "alu_ops", COV_FUNCTIONAL);
    cov_add_bin(&cov, g0, 0); cov_add_bin(&cov, g0, 1); cov_add_bin(&cov, g0, 2);
    cov_sample(&cov, g0, 0); cov_sample(&cov, g0, 1);
    int g1 = cov_new_group(&cov, "alu_fsm", COV_FSM);
    cov_add_bin(&cov, g1, 0); cov_add_bin(&cov, g1, 1);
    cov_sample(&cov, g1, 0);
    cov_report(&cov);

    printf("\n--- Formal Check ---\n");
    FmState init; fm_state_init(&init, 2);
    fm_state_set(&init, 0, 0); fm_state_set(&init, 1, 0);
    formal_bmc(&init, alu_next, alu_prop, 10);
    formal_induction(&init, alu_next, alu_prop, 10);

    printf("\n--- UVM Framework ---\n");
    UvmEnv env; uvm_init(&env);
    uvm_create_component(&env, "test_top", "test", NULL);
    uvm_create_component(&env, "env", "uvm_env", env.root);
    uvm_create_component(&env, "alu_agent", "uvm_agent", env.root);
    uvm_run_test(&env, "alu_basic_test");

    return 0;
}
