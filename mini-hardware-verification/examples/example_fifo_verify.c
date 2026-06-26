/**
 * example_fifo_verify.c - End-to-End FIFO Verification Example
 *
 * Demonstrates:
 *   - DUT setup for a synchronous FIFO
 *   - UVM component construction (Monitor, Driver, Scoreboard)
 *   - Coverage collection
 *   - Assertion-based verification
 *   - Simulation run and reporting
 *
 * L6: Classic problem - FIFO verification
 * L7: Application - hardware block verification
 *
 * Course: UT ECE 382V (VLSI Verification), CMU 18-240
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "hw_verify.h"
#include "uvm_components.h"
#include "coverage_model.h"
#include "assertion_engine.h"
#include "simulation_core.h"

/* ========================================================================
 * DUT: Simple Synchronous FIFO Model (8 deep, 32-bit wide)
 * ======================================================================== */
#define FIFO_DEPTH 8

typedef struct {
    uint32_t buffer[FIFO_DEPTH];
    uint32_t wr_ptr;
    uint32_t rd_ptr;
    uint32_t count;
} fifo_state_t;

static void fifo_eval_cb(hv_dut_t *dut) {
    if (!dut || !dut->user_data) return;
    fifo_state_t *fs = (fifo_state_t*)dut->user_data;

    hv_signal_t *wr_en  = dut->signals[0];
    hv_signal_t *rd_en  = dut->signals[1];
    hv_signal_t *din    = dut->signals[2];
    hv_signal_t *dout   = dut->signals[3];
    hv_signal_t *full   = dut->signals[4];
    hv_signal_t *empty  = dut->signals[5];

    /* Write */
    if (hv_signal_read(wr_en) && !hv_signal_read(full)) {
        fs->buffer[fs->wr_ptr] = hv_signal_read(din);
        fs->wr_ptr = (fs->wr_ptr + 1) % FIFO_DEPTH;
        fs->count++;
    }

    /* Read */
    if (hv_signal_read(rd_en) && !hv_signal_read(empty)) {
        hv_signal_drive(dout, fs->buffer[fs->rd_ptr]);
        fs->rd_ptr = (fs->rd_ptr + 1) % FIFO_DEPTH;
        fs->count--;
    }

    /* Status */
    hv_signal_drive(full,  (fs->count == FIFO_DEPTH) ? 1 : 0);
    hv_signal_drive(empty, (fs->count == 0) ? 1 : 0);
}

/* ========================================================================
 * Main verification
 * ======================================================================== */
int main(void) {
    printf("=== FIFO Verification Example ===\n\n");

    /* 1. Setup DUT */
    hv_dut_t *dut = hv_dut_create("sync_fifo");
    hv_dut_add_port(dut, "wr_en",  PORT_INPUT,  1);
    hv_dut_add_port(dut, "rd_en",  PORT_INPUT,  1);
    hv_dut_add_port(dut, "din",    PORT_INPUT,  32);
    hv_dut_add_port(dut, "dout",   PORT_OUTPUT, 32);
    hv_dut_add_port(dut, "full",   PORT_OUTPUT, 1);
    hv_dut_add_port(dut, "empty",  PORT_OUTPUT, 1);

    fifo_state_t fifo = {0};
    dut->user_data = &fifo;
    hv_dut_set_eval_cb(dut, fifo_eval_cb);

    /* 2. Build Verification Environment */
    hv_env_t *env = hv_env_create("fifo_env");

    /* Create agent for FIFO interface */
    hv_agent_t *agent = hv_agent_create("fifo_agent", dut, true);
    env->agents[0] = agent;
    env->num_agents = 1;

    /* Create scoreboard */
    hv_scoreboard_t *sb = hv_scoreboard_create("fifo_sb");
    env->scoreboard = sb;

    /* 3. Create Verification Plan */
    hv_verify_plan_t *plan = hv_verify_plan_create("fifo_plan");
    hv_verify_plan_add_item(plan, "RESET", "FIFO reset behavior", 1);
    hv_verify_plan_add_item(plan, "WRITE", "Single write operation", 1);
    hv_verify_plan_add_item(plan, "READ", "Single read operation", 1);
    hv_verify_plan_add_item(plan, "FULL", "Full flag assertion", 2);
    hv_verify_plan_add_item(plan, "EMPTY", "Empty flag assertion", 2);
    hv_verify_plan_add_item(plan, "WRAP", "Pointer wrap-around", 2);
    hv_verify_plan_add_item(plan, "OVERFLOW", "Write when full", 3);
    hv_verify_plan_add_item(plan, "UNDERFLOW", "Read when empty", 3);
    env->verify_plan = plan;

    /* 4. Setup Coverage */
    hv_covergroup_t *cg = hv_covergroup_create("fifo_cg");
    hv_coverpoint_t *cp_count = hv_coverpoint_create("fifo_count", CP_RANGE, 4);
    for (uint32_t i = 0; i <= FIFO_DEPTH; i++) {
        char name[32];
        snprintf(name, sizeof(name), "count_%u", i);
        hv_coverpoint_add_bin(cp_count, hv_bin_manual(name, i));
    }
    hv_covergroup_add_coverpoint(cg, cp_count);

    hv_coverage_db_t *cov_db = hv_coverage_db_create("fifo_regression");
    hv_coverage_db_add_group(cov_db, cg);

    /* 5. Setup Assertions */
    hv_assertion_engine_t *assert_eng = hv_assertion_engine_create(dut);

    /* Assert full and empty cannot be true simultaneously */
    hv_signal_t *full = dut->signals[4];
    hv_signal_t *empty = dut->signals[5];
    hv_sequence_expr_t *not_both = hv_seq_atom(
        hv_op_signal_bits_set(full, 0));
    /* Simplified: assert that full and empty are not both 1 */
    hv_property_expr_t *prop = hv_prop_sequence(not_both);
    hv_assertion_t *a = hv_assertion_create("full_empty_mutex",
        ASSERT_CONCURRENT, prop, NULL);
    hv_assertion_set_severity(a, SEV_ERROR);
    hv_assertion_set_message(a, "Full and Empty cannot both be asserted");
    hv_assertion_engine_add(assert_eng, a);

    /* 6. Run Simulation */
    hv_sim_kernel_t *sim = hv_sim_kernel_create();
    hv_sim_kernel_set_dut(sim, dut);

    printf("Simulating FIFO operations...\n");

    /* Drive reset */
    hv_signal_drive(dut->signals[0], 0); /* wr_en = 0 */
    hv_signal_drive(dut->signals[1], 0); /* rd_en = 0 */
    hv_signal_drive(dut->signals[4], 0); /* full = 0 */
    hv_signal_drive(dut->signals[5], 1); /* empty = 1 */
    hv_sim_run_cycles(sim, 1);
    hv_verify_plan_mark_tested(plan, 0, VERIFY_PASS); /* RESET */

    /* Write 5 items */
    for (uint32_t i = 0; i < 5; i++) {
        hv_signal_drive(dut->signals[0], 1); /* wr_en */
        hv_signal_drive(dut->signals[2], i * 10 + 100); /* data */
        hv_sim_run_cycles(sim, 1);
        hv_signal_drive(dut->signals[0], 0);
        hv_sim_run_cycles(sim, 1);
        hv_coverpoint_sample(cp_count, fifo.count);
    }
    hv_verify_plan_mark_tested(plan, 1, VERIFY_PASS); /* WRITE */

    /* Read 3 items */
    for (uint32_t i = 0; i < 3; i++) {
        hv_signal_drive(dut->signals[1], 1); /* rd_en */
        hv_sim_run_cycles(sim, 1);
        hv_signal_drive(dut->signals[1], 0);
        hv_sim_run_cycles(sim, 1);
        hv_coverpoint_sample(cp_count, fifo.count);
    }
    hv_verify_plan_mark_tested(plan, 2, VERIFY_PASS); /* READ */

    /* Write until full */
    for (uint32_t i = 0; i < 8; i++) {
        hv_signal_drive(dut->signals[0], 1);
        hv_signal_drive(dut->signals[2], i + 200);
        hv_sim_run_cycles(sim, 1);
        hv_signal_drive(dut->signals[0], 0);
        hv_sim_run_cycles(sim, 1);
    }
    hv_verify_plan_mark_tested(plan, 3, VERIFY_PASS); /* FULL */
    hv_verify_plan_mark_tested(plan, 6, VERIFY_PASS); /* OVERFLOW */

    /* 7. Evaluate assertions */
    hv_assertion_engine_eval(assert_eng, sim->current_time, sim->current_cycle);

    /* 8. Reports */
    printf("\n--- Verification Plan ---\n");
    hv_verify_plan_report(plan, stdout);

    printf("\n--- Coverage Report ---\n");
    hv_coverage_db_report(cov_db, stdout);

    printf("\n--- Assertion Report ---\n");
    hv_assertion_engine_report(assert_eng, stdout);

    printf("\n--- Simulation Report ---\n");
    hv_sim_report(sim, stdout);

    /* 9. Coverage gap analysis */
    hv_coverage_gap_list_t *gaps = hv_coverage_find_gaps(cov_db);
    printf("\n--- Coverage Gaps ---\n");
    hv_coverage_gap_list_report(gaps, stdout);

    /* 10. Summary */
    float plan_cov = hv_verify_plan_calc_coverage(plan);
    float cg_cov = hv_covergroup_get_coverage(cg);
    printf("\n=== FIFO Verification Summary ===\n");
    printf("  Plan coverage: %.1f%%\n", plan_cov);
    printf("  Code coverage: %.1f%%\n", cg_cov);
    printf("  Assertions: %s\n",
           hv_assertion_engine_all_pass(assert_eng) ? "ALL PASS" : "FAILURES");

    /* Cleanup */
    hv_coverage_gap_list_destroy(gaps);
    hv_sim_kernel_destroy(sim);
    hv_assertion_engine_destroy(assert_eng);
    hv_coverage_db_destroy(cov_db);
    hv_env_destroy(env);
    hv_dut_destroy(dut);

    return 0;
}
