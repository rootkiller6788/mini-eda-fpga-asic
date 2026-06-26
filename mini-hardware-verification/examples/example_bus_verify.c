/**
 * example_bus_verify.c - AXI Bus Protocol Verification Demo
 *
 * Demonstrates AXI4-Lite write channel verification with handshake checking,
 * coverage collection, and assertion-based protocol monitoring.
 *
 * L6: Bus protocol verification  L7: SoC interconnect verification
 * Course: Cambridge Part II Concurrent Systems, UT ECE 382V
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "hw_verify.h"
#include "coverage_model.h"
#include "assertion_engine.h"

/* AXI4-Lite Write Channel signal bundle */
typedef struct {
    hv_signal_t *awvalid, *awready, *awaddr;
    hv_signal_t *wvalid,  *wready,  *wdata;
    hv_signal_t *bvalid,  *bready;
} axi_wr_ch_t;

static int chk_aw(axi_wr_ch_t *c) {
    return hv_signal_read(c->awvalid) && hv_signal_read(c->awready);
}
static int chk_w(axi_wr_ch_t *c) {
    return hv_signal_read(c->wvalid) && hv_signal_read(c->wready);
}
static int chk_b(axi_wr_ch_t *c) {
    return hv_signal_read(c->bvalid) && hv_signal_read(c->bready);
}

int main(void) {
    printf("=== AXI4-Lite Bus Protocol Verification ===\n\n");

    /* DUT: AXI4-Lite slave write channel */
    hv_dut_t *dut = hv_dut_create("axi4_slave");
    hv_signal_t *clk = hv_dut_add_port(dut, "clk", PORT_INPUT, 1);
    hv_signal_t *awv = hv_dut_add_port(dut, "awvalid", PORT_INPUT, 1);
    hv_signal_t *awr = hv_dut_add_port(dut, "awready", PORT_OUTPUT, 1);
    hv_signal_t *awa = hv_dut_add_port(dut, "awaddr", PORT_INPUT, 32);
    hv_signal_t *wv  = hv_dut_add_port(dut, "wvalid", PORT_INPUT, 1);
    hv_signal_t *wr  = hv_dut_add_port(dut, "wready", PORT_OUTPUT, 1);
    hv_signal_t *wd  = hv_dut_add_port(dut, "wdata", PORT_INPUT, 32);
    hv_signal_t *bv  = hv_dut_add_port(dut, "bvalid", PORT_OUTPUT, 1);
    hv_signal_t *br  = hv_dut_add_port(dut, "bready", PORT_INPUT, 1);
    axi_wr_ch_t ch = {awv, awr, awa, wv, wr, wd, bv, br};

    /* Verification Plan */
    hv_verify_plan_t *plan = hv_verify_plan_create("axi_plan");
    hv_verify_plan_add_item(plan, "AW_HS", "AW handshake", 1);
    hv_verify_plan_add_item(plan, "W_HS",  "W handshake", 1);
    hv_verify_plan_add_item(plan, "B_HS",  "B handshake", 1);

    /* Coverage: address ranges */
    hv_covergroup_t *cg = hv_covergroup_create("axi_cg");
    hv_coverpoint_t *cp_a = hv_coverpoint_create("addr", CP_RANGE, 32);
    hv_coverpoint_add_bin(cp_a, hv_bin_auto("LOW", 0, 0x0FFF));
    hv_coverpoint_add_bin(cp_a, hv_bin_auto("MID", 0x1000, 0xFFFF));
    hv_coverpoint_add_bin(cp_a, hv_bin_auto("HI", 0x10000, 0xFFFFFFFF));
    hv_covergroup_add_coverpoint(cg, cp_a);

    hv_coverpoint_t *cp_s = hv_coverpoint_create("hs", CP_RANGE, 4);
    hv_coverpoint_add_bin(cp_s, hv_bin_manual("IDLE", 0));
    hv_coverpoint_add_bin(cp_s, hv_bin_manual("AW", 1));
    hv_coverpoint_add_bin(cp_s, hv_bin_manual("W", 2));
    hv_coverpoint_add_bin(cp_s, hv_bin_manual("B", 3));
    hv_covergroup_add_coverpoint(cg, cp_s);

    /* Assertion */
    hv_assertion_engine_t *eng = hv_assertion_engine_create(dut);
    hv_sequence_expr_t *seq = hv_seq_atom(hv_op_signal_eq(awv, 1));
    hv_property_expr_t *prop = hv_prop_sequence(seq);
    hv_assertion_t *a = hv_assertion_create("aw_chk", ASSERT_CONCURRENT, prop, clk);
    hv_assertion_engine_add(eng, a);

    /* Simulate */
    printf("Running AXI4 write transactions...\n");
    uint32_t n = 0;
    uint32_t addrs[] = {0, 0x1000, 0x8000, 0xFFFF, 0x10000, 0x1F000};

    for (uint32_t t = 0; t < 30; t++) {
        uint32_t addr = addrs[t % 6] + (t * 4);
        hv_signal_drive(awv, 1); hv_signal_drive(awa, addr); hv_signal_drive(awr, 1);
        hv_coverpoint_sample(cp_a, addr); hv_coverpoint_sample(cp_s, 1);
        if (chk_aw(&ch)) hv_verify_plan_mark_tested(plan, 0, VERIFY_PASS);
        hv_signal_drive(awv, 0);
        hv_signal_drive(wv, 1); hv_signal_drive(wd, t*100); hv_signal_drive(wr, 1);
        hv_coverpoint_sample(cp_s, 2);
        if (chk_w(&ch)) hv_verify_plan_mark_tested(plan, 1, VERIFY_PASS);
        hv_signal_drive(wv, 0);
        hv_signal_drive(bv, 1); hv_signal_drive(br, 1);
        hv_coverpoint_sample(cp_s, 3);
        if (chk_b(&ch)) hv_verify_plan_mark_tested(plan, 2, VERIFY_PASS);
        hv_signal_drive(bv, 0);
        n++;
    }

    hv_assertion_engine_eval(eng, 100, 100);

    /* Reports */
    printf("\n--- Verification Plan ---\n");
    hv_verify_plan_report(plan, stdout);
    printf("\n--- Coverage ---\n");
    hv_covergroup_report(cg, stdout);
    printf("\n--- Assertions ---\n");
    hv_assertion_engine_report(eng, stdout);
    printf("\n=== Summary: %u txns, plan %.1f%%, code %.1f%%, assert %s ===\n",
           n, hv_verify_plan_calc_coverage(plan),
           hv_covergroup_get_coverage(cg),
           hv_assertion_engine_all_pass(eng) ? "PASS" : "FAIL");

    /* Cleanup */
    hv_assertion_engine_destroy(eng);
    hv_covergroup_destroy(cg);
    hv_verify_plan_destroy(plan);
    hv_dut_destroy(dut);
    return 0;
}
