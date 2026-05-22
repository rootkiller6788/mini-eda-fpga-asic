#include "assertion_check.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
   Example: Assertion Check — Immediate & Concurrent
   Simulated DUT with assertions: FIFO overflow, protocol check, etc.
   example_assertion.c
   ================================================================ */

/* ----- Simulated DUT context ----- */
typedef struct {
    int     fifo_depth;
    int     fifo_count;
    int     state;
    bool    rd_en;
    bool    wr_en;
    bool    full;
    bool    empty;
    int     clk_cycle;
    bool    clk;
} dut_ctx_t;

/* ----- SVA expression evaluators ----- */
bool eval_full(sva_expr_t* e, void* ctx) {
    (void)e;
    dut_ctx_t* d = (dut_ctx_t*)ctx;
    return d->fifo_count >= d->fifo_depth;
}

bool eval_empty(sva_expr_t* e, void* ctx) {
    (void)e;
    dut_ctx_t* d = (dut_ctx_t*)ctx;
    return d->fifo_count == 0;
}

bool eval_wr_en(sva_expr_t* e, void* ctx) {
    (void)e;
    dut_ctx_t* d = (dut_ctx_t*)ctx;
    return d->wr_en;
}

bool eval_rd_en(sva_expr_t* e, void* ctx) {
    (void)e;
    dut_ctx_t* d = (dut_ctx_t*)ctx;
    return d->rd_en;
}

bool eval_state_idle(sva_expr_t* e, void* ctx) {
    (void)e;
    dut_ctx_t* d = (dut_ctx_t*)ctx;
    return d->state == 0;
}

/* ----- Immediate assertion check: no write when full ----- */
bool check_no_write_when_full(assertion_def_t* a, void* ctx) {
    dut_ctx_t* d = (dut_ctx_t*)ctx;
    return !(d->full && d->wr_en);
}

/* ----- Clock edge detector ----- */
bool my_clock_posedge(assertion_clock_t* clk) {
    dut_ctx_t* d = (dut_ctx_t*)clk->ctx;
    return d->clk && (d->clk_cycle % 2 == 0);
}

int main(void) {
    printf("========================================\n");
    printf(" Assertion Check Example\n");
    printf("========================================\n\n");

    dut_ctx_t dut;
    memset(&dut, 0, sizeof(dut));
    dut.fifo_depth = 8;

    /* Create assertion bank */
    assertion_bank_t* bank = assertion_bank_create();

    /* Create clock */
    assertion_clock_t* clk = assertion_clock_create("dut_clk", my_clock_posedge);
    clk->ctx = &dut;
    assertion_bank_set_default_clock(bank, clk);

    /* --- Immediate Assertions --- */
    assertion_def_t* a_no_wr_full = assertion_create("no_write_when_full", ASSERT_IMMEDIATE);
    assertion_set_severity(a_no_wr_full, ASSERT_SEVERITY_ERROR);
    assertion_set_message(a_no_wr_full, "FIFO: write when full is illegal");
    a_no_wr_full->check = check_no_write_when_full;
    a_no_wr_full->ctx   = &dut;
    assertion_bank_add(bank, a_no_wr_full);

    assertion_def_t* a_no_rd_empty = assertion_create("no_read_when_empty", ASSERT_IMMEDIATE);
    assertion_set_severity(a_no_rd_empty, ASSERT_SEVERITY_ERROR);
    assertion_set_message(a_no_rd_empty, "FIFO: read when empty is illegal");
    assertion_bank_add(bank, a_no_rd_empty);

    assertion_def_t* a_count_positive = assertion_create("fifo_count_never_negative", ASSERT_IMMEDIATE);
    assertion_set_severity(a_count_positive, ASSERT_SEVERITY_FATAL);
    assertion_set_message(a_count_positive, "FIFO: count must be >= 0");
    assertion_bank_add(bank, a_count_positive);

    /* --- Concurrent Assertions (SVA-style) --- */
    sva_expr_t* e_wr   = sva_expr_create("wr_en", eval_wr_en, &dut);
    sva_expr_t* e_rd   = sva_expr_create("rd_en", eval_rd_en, &dut);
    sva_expr_t* e_full = sva_expr_create("full", eval_full, &dut);
    sva_expr_t* e_empty = sva_expr_create("empty", eval_empty, &dut);

    assertion_def_t* a_no_wr_rd_same = assertion_create("no_simultaneous_wr_rd", ASSERT_CONCURRENT);
    assertion_set_severity(a_no_wr_rd_same, ASSERT_SEVERITY_WARNING);
    assertion_set_message(a_no_wr_rd_same, "FIFO: simultaneous write and read should be rare");
    assertion_set_clock(a_no_wr_rd_same, clk);
    sva_sequence_step_t* ant1 = sva_seq_step_create(e_wr, SEQ_CYCLE_DELAY, 1);
    sva_sequence_step_t* con1 = sva_seq_step_create(e_rd, SEQ_CYCLE_DELAY, 0);
    ant1->next = con1;
    a_no_wr_rd_same->antecedent = ant1;
    a_no_wr_rd_same->consequent = con1;
    assertion_bank_add(bank, a_no_wr_rd_same);

    /* --- PSL-style properties --- */
    psl_property_t* p_never_overflow = psl_property_create("never_fifo_overflow", PSL_NEVER);
    p_never_overflow->left  = e_full;
    p_never_overflow->right = e_wr;
    p_never_overflow->clock = clk;

    psl_property_t* p_eventually_empty = psl_property_create("eventually_empty", PSL_EVENTUALLY);
    p_eventually_empty->left  = e_empty;
    p_eventually_empty->right = e_rd;

    /* --- Coverage --- */
    assertion_coverage_t* cov_full  = assertion_coverage_create("cover_full_condition");
    assertion_coverage_t* cov_empty = assertion_coverage_create("cover_empty_condition");
    assertion_bank_add_coverage(bank, cov_full);
    assertion_bank_add_coverage(bank, cov_empty);

    /* --- Simulation: exercise the FIFO --- */
    printf("Simulation scenarios:\n\n");

    /* Scenario 1: Normal writes */
    printf("--- Scenario 1: Normal writes (8 cycles) ---\n");
    for (int i = 0; i < 8; i++) {
        dut.clk = true;
        dut.wr_en = true;
        dut.rd_en = false;
        dut.fifo_count++;
        dut.full  = (dut.fifo_count >= dut.fifo_depth);
        dut.empty = (dut.fifo_count == 0);
        assertion_bank_check_all(bank, &dut);
        printf("  Cycle %d: count=%d full=%d empty=%d\n",
            i, dut.fifo_count, dut.full, dut.empty);
        dut.clk = false;
    }

    /* Scenario 2: Write when full (should trigger error) */
    printf("\n--- Scenario 2: Write when full (triggers assertion) ---\n");
    dut.wr_en = true;
    dut.rd_en = false;
    dut.full  = true;
    ASSERT_IMMEDIATE_COND(a_no_wr_full, !(dut.full && dut.wr_en),
        ASSERT_SEVERITY_ERROR, "write asserted while FIFO full");
    printf("  After write-to-full: count=%d\n", dut.fifo_count);

    /* Scenario 3: Normal reads */
    printf("\n--- Scenario 3: Normal reads (4 cycles) ---\n");
    for (int i = 0; i < 4; i++) {
        dut.clk = true;
        dut.wr_en = false;
        dut.rd_en = (dut.fifo_count > 0);
        if (dut.rd_en) dut.fifo_count--;
        dut.full  = (dut.fifo_count >= dut.fifo_depth);
        dut.empty = (dut.fifo_count == 0);
        assertion_bank_check_all(bank, &dut);
        printf("  Cycle %d: count=%d\n", i, dut.fifo_count);
        dut.clk = false;
    }

    /* Scenario 4: PSL checks */
    printf("\n--- Scenario 4: PSL checks ---\n");
    bool psl1 = psl_property_check(p_never_overflow, &dut);
    printf("  PSL never_fifo_overflow: %s\n", psl1 ? "HOLD" : "FAIL");
    bool psl2 = psl_property_check(p_eventually_empty, &dut);
    printf("  PSL eventually_empty: %s\n", psl2 ? "HOLD" : "FAIL");

    /* --- Formal check --- */
    printf("\n--- Formal Verification Attempt ---\n");
    assertion_to_formal(a_no_wr_full, "temp_assert.smt2", "smt2");
    assertion_prove(a_no_wr_full, "z3", 100);

    /* --- Report --- */
    assertion_bank_report(bank);

    /* --- Cleanup --- */
    psl_property_destroy(p_never_overflow);
    psl_property_destroy(p_eventually_empty);
    assertion_coverage_destroy(cov_full);
    assertion_coverage_destroy(cov_empty);
    assertion_clock_destroy(clk);
    assertion_bank_destroy(bank);
    sva_expr_destroy(e_wr);
    sva_expr_destroy(e_rd);
    sva_expr_destroy(e_full);
    sva_expr_destroy(e_empty);
    sva_seq_step_destroy(ant1);
    sva_seq_step_destroy(con1);

    printf("\nDone.\n");
    return 0;
}
