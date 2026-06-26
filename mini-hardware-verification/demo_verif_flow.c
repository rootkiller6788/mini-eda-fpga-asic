#include "uvm_methodology.h"
#include "assertion_check.h"
#include "formal_verify.h"
#include "coverage_mdl.h"
#include "verification_ip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
   Demo: Full Verification Flow
   End-to-end demo: UVM test → assertions → coverage → formal
   demo_verif_flow.c
   ================================================================ */

/* ----- DUT: simple 8-bit counter with enable, load, overflow ----- */
typedef struct {
    uint8_t  count;
    uint8_t  load_val;
    bool     enable;
    bool     load;
    bool     reset;
    bool     overflow;
    bool     zero;
    bool     clk;
    int      cycle;
} counter_dut_t;

void counter_dut_reset(counter_dut_t* d) {
    d->count    = 0;
    d->overflow = false;
    d->zero     = true;
    d->cycle    = 0;
}

void counter_dut_tick(counter_dut_t* d) {
    d->clk = true;
    d->cycle++;
    if (d->reset) {
        d->count = 0;
    } else if (d->load) {
        d->count = d->load_val;
    } else if (d->enable) {
        if (d->count == 255) {
            d->overflow = true;
            d->count    = 0;
        } else {
            d->count++;
        }
    }
    d->zero = (d->count == 0);
    d->clk  = false;
}

/* ----- Assertion checks for counter ----- */
typedef struct {
    counter_dut_t* dut;
    assertion_def_t* a_no_overflow_without_enable;
    assertion_def_t* a_count_lt_256;
    assertion_def_t* a_reset_clears;
} counter_assert_ctx_t;

bool check_count_range(assertion_def_t* a, void* ctx) {
    counter_assert_ctx_t* c = (counter_assert_ctx_t*)ctx;
    return c->dut->count <= 255;
}

/* ----- Scoreboard predict for counter ----- */
bool counter_predict(const void* stimulus, void* predicted, void* ctx) {
    const counter_dut_t* stim = (const counter_dut_t*)stimulus;
    counter_dut_t* pred = (counter_dut_t*)predicted;
    (void)ctx;
    pred->count = stim->count;
    if (stim->reset) {
        pred->count = 0;
    } else if (stim->load) {
        pred->count = stim->load_val;
    } else if (stim->enable) {
        pred->count = (stim->count == 255) ? 0 : (uint8_t)(stim->count + 1);
    }
    pred->overflow = (stim->enable && stim->count == 255 && !stim->reset && !stim->load);
    pred->zero     = (pred->count == 0);
    return true;
}

bool counter_compare(const void* predicted, const void* actual, void* ctx) {
    (void)ctx;
    const counter_dut_t* p = (const counter_dut_t*)predicted;
    const counter_dut_t* a = (const counter_dut_t*)actual;
    return (p->count == a->count) && (p->overflow == a->overflow) && (p->zero == a->zero);
}

/* ----- UVM Test: counter verification test ----- */
static counter_dut_t g_test_dut;
static vip_sb_t* g_sb = NULL;
static pf_tracker_t* g_tracker = NULL;
static cov_covergroup_t* g_cg = NULL;
static cov_coverpoint_t* g_cp_count = NULL;
static cov_coverpoint_t* g_cp_overflow = NULL;
static cov_coverpoint_t* g_cp_zero = NULL;

void test_counter_run(uvm_test_t* test) {
    printf("\n  [Test %s] Running counter verification\n", test->name);

    counter_dut_reset(&g_test_dut);
    uint32_t rng = test->seed;

    for (int i = 0; i < 200; i++) {
        rng = rng * 1664525u + 1013904223u;

        counter_dut_t predicted_state;
        memcpy(&predicted_state, &g_test_dut, sizeof(counter_dut_t));
        counter_predict(&g_test_dut, &predicted_state, NULL);

        /* Randomize stimuli */
        g_test_dut.enable = true;
        g_test_dut.reset  = ((rng & 0xF) == 0);   /* ~6% reset */
        g_test_dut.load   = ((rng & 0x3F) == 0);   /* ~1.5% load */
        if (g_test_dut.load) g_test_dut.load_val = (uint8_t)((rng >> 8) & 0xFF);

        counter_dut_tick(&g_test_dut);

        /* Predict + compare in scoreboard */
        vip_sb_push_expected(g_sb, &predicted_state);
        bool match = vip_sb_check_actual(g_sb, &g_test_dut);

        /* Coverage sampling */
        cov_coverpoint_sample(g_cp_count, g_test_dut.count);
        cov_coverpoint_sample(g_cp_overflow, g_test_dut.overflow ? 1u : 0u);
        cov_coverpoint_sample(g_cp_zero, g_test_dut.zero ? 1u : 0u);
        cov_covergroup_sample(g_cg);

        /* Track pass/fail */
        if (match) {
            pf_tracker_record_pass(g_tracker);
        } else {
            pf_tracker_record_fail(g_tracker, "Scoreboard mismatch");
        }
    }
}

/* ----- Formal property: counter never exceeds 255 ----- */
bool prop_count_never_above_255(const formal_state_t* state, void* ctx) {
    (void)ctx;
    for (size_t i = 0; i < 8; i++) {
        if (formal_state_get_bit(state, 8 + i)) return false;
    }
    return true;
}

bool init_counter_reset(formal_state_t* state, void* ctx) {
    (void)ctx;
    for (size_t i = 0; i < 64; i++)
        formal_state_set_bit(state, i, false);
    return true;
}

void counter_transition(const formal_state_t* current, formal_state_t* next, void* ctx) {
    (void)ctx;
    uint8_t count = 0;
    for (size_t i = 0; i < 8; i++)
        if (formal_state_get_bit(current, i))
            count |= (uint8_t)(1u << i);
    count++;
    for (size_t i = 0; i < 8; i++)
        formal_state_set_bit(next, i, (count >> i) & 1u);
    for (size_t i = 8; i < 16; i++)
        formal_state_set_bit(next, i, false);
    if (count == 0) formal_state_set_bit(next, 16, true);
}

int main(void) {
    printf("================================================\n");
    printf(" Demo: Full Verification Flow\n");
    printf("  UVM + Assertions + Coverage + Formal\n");
    printf("================================================\n\n");

    /* =========================================
       Phase 1: UVM Testbench Setup
       ========================================= */
    printf("=== Phase 1: UVM Testbench Setup ===\n");

    uvm_test_t* test = uvm_test_create("counter_full_test");
    test->seed = (uint32_t)time(NULL);
    test->verbosity = UVM_HIGH;
    uvm_test_set_run_handler(test, test_counter_run);

    uvm_env_t* env = uvm_env_create("counter_env");

    uvm_agent_t* ag = uvm_agent_create("counter_agent", UVM_AGENT_ACTIVE);
    ag->sequencer = uvm_sequencer_create("counter_sqr");
    ag->driver    = uvm_driver_create("counter_drv", ag->sequencer);
    ag->monitor   = uvm_monitor_create("counter_mon");
    uvm_agent_connect(ag);
    uvm_env_add_agent(env, ag);
    uvm_test_set_env(test, env);

    /* Scoreboard */
    g_sb = vip_sb_create("counter_sb", 256);
    vip_sb_set_predict(g_sb, (vip_sb_predict_fn)counter_predict, NULL);
    vip_sb_set_compare(g_sb, (vip_sb_compare_fn)counter_compare, NULL);

    /* Pass/Fail tracker */
    g_tracker = pf_tracker_create("counter_regression");

    /* =========================================
       Phase 2: Coverage Model Setup
       ========================================= */
    printf("\n=== Phase 2: Coverage Model Setup ===\n");

    g_cg = cov_covergroup_create("cg_counter");
    g_cg->goal = 95.0;

    g_cp_count = cov_coverpoint_create("cp_count_value", 16);
    cov_coverpoint_set_range(g_cp_count, 0.0, 255.0);
    cov_coverpoint_set_goal(g_cp_count, 90.0);
    cov_covergroup_add_coverpoint(g_cg, g_cp_count);

    g_cp_overflow = cov_coverpoint_create("cp_overflow", 2);
    cov_coverpoint_set_range(g_cp_overflow, 0.0, 1.0);
    cov_covergroup_add_coverpoint(g_cg, g_cp_overflow);

    g_cp_zero = cov_coverpoint_create("cp_zero", 2);
    cov_coverpoint_set_range(g_cp_zero, 0.0, 1.0);
    cov_covergroup_add_coverpoint(g_cg, g_cp_zero);

    cov_cross_t* cross_count_overflow = cov_cross_create("cross_count_x_overflow");
    cov_cross_add_coverpoint(cross_count_overflow, g_cp_count);
    cov_cross_add_coverpoint(cross_count_overflow, g_cp_overflow);
    cov_cross_build(cross_count_overflow);
    cov_covergroup_add_cross(g_cg, cross_count_overflow);

    /* =========================================
       Phase 3: Assertion Setup
       ========================================= */
    printf("\n=== Phase 3: Assertion Setup ===\n");

    assertion_bank_t* assert_bank = assertion_bank_create();

    counter_assert_ctx_t a_ctx;
    a_ctx.dut = &g_test_dut;

    assertion_def_t* a_range = assertion_create("count_never_above_255", ASSERT_IMMEDIATE);
    assertion_set_severity(a_range, ASSERT_SEVERITY_FATAL);
    assertion_set_message(a_range, "Counter count must always be <= 255");
    a_range->check = check_count_range;
    a_range->ctx   = &a_ctx;
    assertion_bank_add(assert_bank, a_range);

    assertion_def_t* a_reset = assertion_create("reset_clears_count", ASSERT_IMMEDIATE);
    assertion_set_severity(a_reset, ASSERT_SEVERITY_ERROR);
    assertion_set_message(a_reset, "After reset, count must be 0");
    assertion_bank_add(assert_bank, a_reset);

    /* =========================================
       Phase 4: Run UVM Test
       ========================================= */
    printf("\n=== Phase 4: Run UVM Test ===\n");
    uvm_test_run(test);

    /* =========================================
       Phase 5: Report Scoreboard & Tracker
       ========================================= */
    printf("\n=== Phase 5: Results ===\n");
    vip_sb_report(g_sb, stdout);
    pf_tracker_report(g_tracker, stdout);

    /* =========================================
       Phase 6: Coverage Report
       ========================================= */
    printf("\n=== Phase 6: Coverage Report ===\n");

    cov_code_db_t* code_db = cov_code_db_create();
    for (int i = 1; i <= 30; i++) {
        char name[128];
        snprintf(name, sizeof(name), "counter.v:line%d", i);
        cov_code_db_add(code_db, cov_code_item_create(name, COV_CODE_LINE, "counter.v", i));
    }
    for (int j = 0; j < 30; j++) {
        cov_code_item_hit(code_db->items[j]);
    }
    cov_code_db_update_metrics(code_db);

    cov_covergroup_report(g_cg, stdout);

    cov_closure_t* closure = cov_closure_create();
    cov_closure_add_group(closure, g_cg);
    cov_closure_set_code_db(closure, code_db);
    cov_closure_report(closure, stdout);

    /* =========================================
       Phase 7: Formal Verification
       ========================================= */
    printf("\n=== Phase 7: Formal Verification ===\n");

    formal_state_t* init_state = formal_state_create(64);
    for (size_t i = 0; i < 64; i++) formal_state_set_bit(init_state, i, false);

    formal_transition_t* trans = formal_trans_create("counter_inc", counter_transition, NULL);

    formal_init_t init;
    strncpy(init.name, "counter_reset", sizeof(init.name) - 1);
    init.is_init = init_counter_reset;
    init.ctx     = NULL;

    formal_property_t* safety_prop = formal_property_create(
        "counter_no_overflow_9bit", FORMAL_PROP_SAFETY,
        prop_count_never_above_255, NULL);
    formal_property_set_smt(safety_prop,
        "(assert (bvule count #x00000000000000ff))");

    formal_bmc_t* bmc = formal_bmc_create();
    formal_bmc_set_bound(bmc, 100);
    formal_bmc_set_transition(bmc, trans);
    formal_bmc_set_initial(bmc, &init);
    formal_bmc_add_property(bmc, safety_prop);

    formal_result_t result = formal_bmc_run(bmc);
    printf("BMC result: ");
    formal_print_result(result, stdout);
    printf("\n");

    /* Try induction */
    formal_ind_t* ind = formal_induction_create();
    formal_induction_set_depth(ind, 50);
    formal_induction_set_transition(ind, trans);
    formal_induction_set_initial(ind, &init);
    formal_induction_add_property(ind, safety_prop);
    formal_result_t ind_result = formal_induction_prove(ind);
    printf("Induction result: ");
    formal_print_result(ind_result, stdout);
    printf("\n");

    /* =========================================
       Phase 8: Regression Summary
       ========================================= */
    printf("\n=== Phase 8: Regression Summary ===\n");

    regression_suite_t* suite = regression_suite_create("counter_full");
    regression_suite_run(suite);

    double total_cov = cov_closure_get_overall_coverage(closure);
    printf("\n========================================\n");
    printf(" FINAL SUMMARY\n");
    printf("========================================\n");
    printf("  UVM Test:     %s\n", test->pass ? "PASSED" : "FAILED");
    printf("  Scoreboard:   %.1f%% match\n", vip_sb_match_rate(g_sb));
    printf("  Coverage:     %.1f%%\n", total_cov);
    printf("  BMC:          ");
    formal_print_result(result, stdout); printf("\n");
    printf("  Induction:    ");
    formal_print_result(ind_result, stdout); printf("\n");
    printf("  Overall:      %s\n",
        (test->pass && pf_tracker_all_passed(g_tracker) && result == FORMAL_RESULT_PASS)
        ? "PASSED" : "NEEDS WORK");
    printf("========================================\n");

    /* Cleanup */
    formal_induction_destroy(ind);
    formal_bmc_destroy(bmc);
    formal_state_destroy(init_state);
    formal_trans_destroy(trans);
    formal_property_destroy(safety_prop);
    cov_closure_destroy(closure);
    assertion_bank_destroy(assert_bank);
    vip_sb_destroy(g_sb);
    pf_tracker_destroy(g_tracker);
    regression_suite_destroy(suite);
    uvm_test_destroy(test);

    printf("\nDemo complete.\n");
    return 0;
}
