#include "verification_ip.h"
#include "assertion_check.h"
#include "coverage_mdl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
   Demo: Regression Testing — Multiple Tests, VIP, Seed Mgmt
   AXI4 BFM, DDR model, scoreboard, regression suite
   demo_regression.c
   ================================================================ */

/* ----- AXI4 memory ----- */
#define MEM_SIZE 256
static uint64_t g_mem[MEM_SIZE];

/* ----- AXI4 write callback ----- */
void axi_write_log(axi4_bfm_t* bfm, const axi4_trans_t* trans, void* ctx) {
    (void)ctx;
    printf("  [BFM %s] WRITE addr=0x%04llX len=%d\n",
        bfm->name, (unsigned long long)trans->addr, trans->len + 1);
}

void axi_read_log(axi4_bfm_t* bfm, const axi4_trans_t* trans, void* ctx) {
    (void)ctx;
    printf("  [BFM %s] READ  addr=0x%04llX len=%d\n",
        bfm->name, (unsigned long long)trans->addr, trans->len + 1);
}

/* ----- Test 1: AXI4 single write/read ----- */
typedef struct {
    axi4_bfm_t* bfm;
    vip_sb_t*   sb;
    pf_tracker_t* tracker;
    bool        halt_on_fail;
} test1_ctx_t;

bool test_axi_single(void* ctx) {
    test1_ctx_t* tc = (test1_ctx_t*)ctx;
    printf("\n--- Test: AXI4 Single Write/Read ---\n");

    /* Write */
    axi4_trans_t* wr = axi4_trans_create();
    axi4_trans_set_addr(wr, 0x100);
    axi4_trans_set_burst(wr, 0, 3, AXI_BURST_INCR);
    axi4_trans_set_data(wr, 0, 0xDEADBEEFCAFE1234ull, 0xFFFFFFFFFFFFFFFFull);
    axi4_bfm_write(tc->bfm, wr);

    /* Expected read result */
    uint64_t expected = 0xDEADBEEFCAFE1234ull;
    vip_sb_push_expected(tc->sb, &expected);

    /* Read */
    axi4_trans_t* rd = axi4_trans_create();
    axi4_trans_set_addr(rd, 0x100);
    axi4_trans_set_burst(rd, 0, 3, AXI_BURST_INCR);
    axi4_bfm_read(tc->bfm, rd);
    bool match = (rd->data[0] == expected);
    printf("  Read data: 0x%016llX %s\n",
        (unsigned long long)rd->data[0], match ? "OK" : "FAIL");

    bool sb_match = vip_sb_check_actual(tc->sb, &rd->data[0]);
    if (sb_match) pf_tracker_record_pass(tc->tracker);
    else pf_tracker_record_fail(tc->tracker, "AXI single RW mismatch");

    axi4_trans_destroy(wr);
    axi4_trans_destroy(rd);
    return match && sb_match;
}

/* ----- Test 2: AXI4 burst write/read ----- */
bool test_axi_burst(void* ctx) {
    test1_ctx_t* tc = (test1_ctx_t*)ctx;
    printf("\n--- Test: AXI4 Burst Write/Read ---\n");

    /* Burst write 4 beats */
    axi4_trans_t* wr = axi4_trans_create();
    axi4_trans_set_addr(wr, 0x200);
    axi4_trans_set_burst(wr, 3, 3, AXI_BURST_INCR); /* len=3 => 4 beats */
    uint64_t vals[] = { 0xAAAAAAAA00000001ull, 0xBBBBBBBB00000002ull,
                        0xCCCCCCCC00000003ull, 0xDDDDDDDD00000004ull };
    for (int b = 0; b < 4; b++) {
        axi4_trans_set_data(wr, b, vals[b], 0xFFFFFFFFFFFFFFFFull);
        vip_sb_push_expected(tc->sb, &vals[b]);
    }
    axi4_bfm_write(tc->bfm, wr);

    /* Burst read back */
    axi4_trans_t* rd = axi4_trans_create();
    axi4_trans_set_addr(rd, 0x200);
    axi4_trans_set_burst(rd, 3, 3, AXI_BURST_INCR);
    axi4_bfm_read(tc->bfm, rd);

    bool all_match = true;
    for (int b = 0; b < 4; b++) {
        bool m = (rd->data[b] == vals[b]);
        all_match = all_match && m;
        printf("  Beat %d: 0x%016llX %s\n", b,
            (unsigned long long)rd->data[b], m ? "OK" : "MISMATCH");
        if (m) pf_tracker_record_pass(tc->tracker);
        else pf_tracker_record_fail(tc->tracker, "Burst data mismatch");

        bool sb_m = vip_sb_check_actual(tc->sb, &rd->data[b]);
        if (!sb_m) all_match = false;
    }

    axi4_trans_destroy(wr);
    axi4_trans_destroy(rd);
    return all_match;
}

/* ----- Test 3: DDR memory model basic ----- */
bool test_ddr_basic(void* ctx) {
    test1_ctx_t* tc = (test1_ctx_t*)ctx;
    printf("\n--- Test: DDR Memory Model Basic ---\n");

    ddr_mem_t* ddr = ddr_mem_create("ddr_sim", VIP_PROTO_DDR4,
        1024ull * 1024ull * 1024ull); /* 1 GB */
    ddr_mem_initialize(ddr, true, 0xCAFE);

    uint64_t wr_data = 0xFEEDF00D12345678ull;
    bool ok = ddr_mem_issue_command(ddr, DDR_CMD_ACT, 0, 0, 0, NULL);
    ok = ok && ddr_mem_issue_command(ddr, DDR_CMD_WRITE, 0, 0, 0, &wr_data);

    uint64_t rd_data = 0;
    ok = ok && ddr_mem_issue_command(ddr, DDR_CMD_READ, 0, 0, 0, &rd_data);

    bool match = (rd_data == wr_data);
    printf("  DDR write: 0x%016llX, read: 0x%016llX %s\n",
        (unsigned long long)wr_data, (unsigned long long)rd_data,
        match ? "OK" : "MISMATCH");

    if (match) pf_tracker_record_pass(tc->tracker);
    else pf_tracker_record_fail(tc->tracker, "DDR data mismatch");

    ddr_mem_issue_command(ddr, DDR_CMD_PRE, 0, 0, 0, NULL);
    ddr_mem_destroy(ddr);
    return match && ok;
}

/* ----- Test 4: Protocol analyzer latency tracking ----- */
bool test_proto_analyzer(void* ctx) {
    (void)ctx;
    printf("\n--- Test: Protocol Analyzer ---\n");

    proto_analyzer_t* pa = proto_analyzer_create("axi_analyzer", VIP_PROTO_AXI4);
    pa->enable_logging = false;

    for (int i = 0; i < 50; i++) {
        proto_analyzer_log(pa, "TX", NULL);
        proto_analyzer_record_latency(pa, (uint64_t)(i % 20 + 1));
    }
    proto_analyzer_report(pa, stdout);
    proto_analyzer_destroy(pa);
    return true;
}

/* ----- Test 5: Random seed reproducibility ----- */
bool test_seed_reproduce(void* ctx) {
    (void)ctx;
    printf("\n--- Test: Random Seed Reproducibility ---\n");

    seed_mgr_t* mgr = seed_mgr_create(0xABCDEF01);

    uint32_t seq1[5], seq2[5];
    for (int i = 0; i < 5; i++) seq1[i] = seed_mgr_next(mgr);

    seed_mgr_reset(mgr);
    for (int i = 0; i < 5; i++) seq2[i] = seed_mgr_next(mgr);

    bool match = true;
    for (int i = 0; i < 5; i++) {
        printf("  seed[%d]: 0x%08X vs 0x%08X %s\n",
            i, seq1[i], seq2[i], seq1[i] == seq2[i] ? "OK" : "FAIL");
        if (seq1[i] != seq2[i]) match = false;
    }

    seed_mgr_save_state(mgr);
    printf("  history count: %d\n", mgr->history_count);

    seed_mgr_destroy(mgr);
    return match;
}

/* ----- Test 6: UVC configuration ----- */
bool test_uvc_config(void* ctx) {
    (void)ctx;
    printf("\n--- Test: UVC Configuration ---\n");

    uvc_t* axi_uvc = uvc_create("axi4_uvc", VIP_PROTO_AXI4);
    uvc_configure(axi_uvc);
    uvc_run(axi_uvc);
    uvc_report(axi_uvc, stdout);

    uvc_t* pcie_uvc = uvc_create("pcie_uvc", VIP_PROTO_PCIE_GEN4);
    uvc_configure(pcie_uvc);
    uvc_report(pcie_uvc, stdout);

    uvc_destroy(axi_uvc);
    uvc_destroy(pcie_uvc);
    return true;
}

int main(void) {
    printf("================================================\n");
    printf(" Demo: Regression Testing\n");
    printf("  AXI4 BFM + DDR + Scoreboard + Seed Mgmt\n");
    printf("================================================\n");

    /* Initialize memory and BFM */
    memset(g_mem, 0, sizeof(g_mem));
    axi4_bfm_t* bfm = axi4_bfm_create("axi_master", true);
    axi4_bfm_set_memory(bfm, g_mem, MEM_SIZE);
    axi4_bfm_set_callbacks(bfm, axi_write_log, axi_read_log, NULL);
    bfm->enable_backdoor = true;

    vip_sb_t* sb = vip_sb_create("regression_sb", 256);
    pf_tracker_t* tracker = pf_tracker_create("regression_pf");

    test1_ctx_t tc = { bfm, sb, tracker, true };

    /* Build regression suite */
    regression_suite_t* suite = regression_suite_create("hardware_regression");
    regression_suite_set_seed(suite, (uint32_t)time(NULL));
    suite->stop_on_fail = false;

    regression_reg_t* t1 = regression_reg_create("axi_single_rw",    test_axi_single,    &tc, 1001);
    regression_reg_t* t2 = regression_reg_create("axi_burst_rw",     test_axi_burst,     &tc, 1002);
    regression_reg_t* t3 = regression_reg_create("ddr_basic",        test_ddr_basic,     &tc, 1003);
    regression_reg_t* t4 = regression_reg_create("proto_analyzer",   test_proto_analyzer, NULL, 1004);
    regression_reg_t* t5 = regression_reg_create("seed_reproduce",   test_seed_reproduce, NULL, 1005);
    regression_reg_t* t6 = regression_reg_create("uvc_config",       test_uvc_config,    NULL, 1006);

    regression_suite_add_test(suite, t1);
    regression_suite_add_test(suite, t2);
    regression_suite_add_test(suite, t3);
    regression_suite_add_test(suite, t4);
    regression_suite_add_test(suite, t5);
    regression_suite_add_test(suite, t6);

    /* Run regression */
    regression_suite_run(suite);

    /* Report */
    vip_sb_report(sb, stdout);
    pf_tracker_report(tracker, stdout);
    regression_suite_report(suite, stdout);

    /* Coverage for regression */
    printf("\n--- Regression Coverage ---\n");
    cov_code_db_t* cov_db = cov_code_db_create();
    for (int i = 1; i <= 15; i++) {
        char name[128];
        snprintf(name, sizeof(name), "test_%d", i);
        cov_code_item_t* item = cov_code_item_create(name, COV_CODE_LINE, "regression.c", i);
        cov_code_db_add(cov_db, item);
    }
    for (int i = 0; i < 12; i++) cov_code_item_hit(cov_db->items[i]);
    cov_code_db_update_metrics(cov_db);
    cov_code_db_report(cov_db, stdout);

    /* Summary */
    printf("\n========================================\n");
    printf(" REGRESSION SUMMARY\n");
    printf("========================================\n");
    printf("  Tests: %d/%d passed\n", suite->total_passed, suite->total_runs);
    printf("  Scoreboard: %.1f%% match rate\n", vip_sb_match_rate(sb));
    printf("  Code Coverage: %.1f%%\n", cov_db->overall_coverage);
    printf("  All passed: %s\n",
        pf_tracker_all_passed(tracker) ? "YES" : "NO");
    printf("========================================\n");

    /* Cleanup */
    cov_code_db_destroy(cov_db);
    regression_suite_destroy(suite);
    vip_sb_destroy(sb);
    pf_tracker_destroy(tracker);
    axi4_bfm_destroy(bfm);

    printf("\nDemo complete.\n");
    return pf_tracker_all_passed(tracker) ? 0 : 1;
}
