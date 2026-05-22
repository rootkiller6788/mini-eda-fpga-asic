#include "verification_ip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
   Verification IP Implementation
   verification_ip.c
   ================================================================ */

/* ----- AXI4 Transaction ----- */
axi4_trans_t* axi4_trans_create(void) {
    axi4_trans_t* t = calloc(1, sizeof(axi4_trans_t));
    return t;
}

void axi4_trans_destroy(axi4_trans_t* t) { free(t); }

void axi4_trans_set_addr(axi4_trans_t* t, uint64_t addr) {
    if (t) t->addr = addr;
}

void axi4_trans_set_burst(axi4_trans_t* t, uint8_t len, uint8_t size, axi_burst_type_t burst) {
    if (!t) return;
    t->len   = len;
    t->size  = size;
    t->burst = burst;
}

void axi4_trans_set_data(axi4_trans_t* t, int beat, uint64_t data, uint64_t strobe) {
    if (!t || beat < 0 || beat >= 256) return;
    t->data[beat]   = data;
    t->strobe[beat] = strobe;
}

uint64_t axi4_trans_get_data(const axi4_trans_t* t, int beat) {
    if (!t || beat < 0 || beat >= 256) return 0;
    return t->data[beat];
}

/* ----- AXI4 BFM ----- */
axi4_bfm_t* axi4_bfm_create(const char* name, bool is_master) {
    axi4_bfm_t* bfm = calloc(1, sizeof(axi4_bfm_t));
    if (!bfm) return NULL;
    strncpy(bfm->name, name, sizeof(bfm->name) - 1);
    bfm->protocol  = VIP_PROTO_AXI4;
    bfm->is_master = is_master;
    bfm->seed      = 0xABCD1234;
    return bfm;
}

void axi4_bfm_destroy(axi4_bfm_t* bfm) { free(bfm); }
void axi4_bfm_set_memory(axi4_bfm_t* bfm, uint64_t* mem, uint64_t size) {
    if (bfm) { bfm->memory = mem; bfm->mem_size = size; }
}
void axi4_bfm_set_callbacks(axi4_bfm_t* bfm, axi4_bfm_write_cb wcb, axi4_bfm_read_cb rcb, void* ctx) {
    if (bfm) { bfm->on_write = wcb; bfm->on_read = rcb; bfm->cb_ctx = ctx; }
}

bool axi4_bfm_write(axi4_bfm_t* bfm, axi4_trans_t* trans) {
    if (!bfm || !trans) return false;
    int burst_len = trans->len + 1;
    for (int i = 0; i < burst_len && i < 256; i++) {
        uint64_t mem_addr = (trans->addr >> 3) + (uint64_t)i;
        if (bfm->memory && mem_addr < bfm->mem_size) {
            if (bfm->enable_backdoor) {
                bfm->memory[mem_addr] = trans->data[i];
            }
        }
    }
    if (bfm->on_write) bfm->on_write(bfm, trans, bfm->cb_ctx);
    printf("[AXI4-BFM %s] WRITE addr=0x%llX len=%d burst=%d\n",
        bfm->name, (unsigned long long)trans->addr, burst_len, trans->burst);
    return true;
}

bool axi4_bfm_read(axi4_bfm_t* bfm, axi4_trans_t* trans) {
    if (!bfm || !trans) return false;
    int burst_len = trans->len + 1;
    for (int i = 0; i < burst_len && i < 256; i++) {
        uint64_t mem_addr = (trans->addr >> 3) + (uint64_t)i;
        if (bfm->memory && mem_addr < bfm->mem_size) {
            trans->data[i] = bfm->memory[mem_addr];
        }
    }
    if (bfm->on_read) bfm->on_read(bfm, trans, bfm->cb_ctx);
    printf("[AXI4-BFM %s] READ  addr=0x%llX len=%d burst=%d\n",
        bfm->name, (unsigned long long)trans->addr, burst_len, trans->burst);
    return true;
}

void axi4_bfm_tick(axi4_bfm_t* bfm) {
    if (bfm) bfm->cycle_count++;
}

/* ----- PCIe TLP ----- */
pcie_tlp_t* pcie_tlp_create(void) { return calloc(1, sizeof(pcie_tlp_t)); }
void pcie_tlp_destroy(pcie_tlp_t* tlp) { free(tlp); }

/* ----- DDR Memory Model ----- */
ddr_mem_t* ddr_mem_create(const char* name, vip_protocol_t proto, uint64_t capacity) {
    ddr_mem_t* mem = calloc(1, sizeof(ddr_mem_t));
    if (!mem) return NULL;
    strncpy(mem->name, name, sizeof(mem->name) - 1);
    mem->protocol     = proto;
    mem->capacity     = capacity;
    mem->storage      = calloc(1, capacity > 0 ? capacity : 1);
    mem->banks        = 8;
    mem->rows         = 16384;
    mem->cols         = 1024;
    mem->data_width   = 64;
    mem->burst_length = 8;
    mem->tRCD         = 14;
    mem->tRP          = 14;
    mem->tRAS         = 35;
    mem->tRFC         = 350;
    mem->tWR          = 14;
    mem->bank_rows    = calloc((size_t)mem->banks, sizeof(int));
    mem->bank_active  = calloc((size_t)mem->banks, sizeof(bool));
    return mem;
}

void ddr_mem_destroy(ddr_mem_t* mem) {
    if (mem) {
        free(mem->storage);
        free(mem->bank_rows);
        free(mem->bank_active);
        free(mem);
    }
}

bool ddr_mem_issue_command(ddr_mem_t* mem, ddr_cmd_t cmd, int bank, int row, int col, uint64_t* data) {
    if (!mem) return false;
    mem->cycle++;
    switch (cmd) {
        case DDR_CMD_ACT:
            if (bank < mem->banks) {
                mem->bank_rows[bank] = row;
                mem->bank_active[bank] = true;
            }
            break;
        case DDR_CMD_WRITE:
            if (bank < mem->banks && row == mem->bank_rows[bank] && data) {
                uint64_t offset = ((uint64_t)bank * mem->rows * mem->cols + (uint64_t)row * mem->cols + (uint64_t)col)
                    * (uint64_t)(mem->data_width / 8);
                if (offset < mem->capacity)
                    memcpy(mem->storage + offset, data, (size_t)(mem->burst_length * mem->data_width / 8));
            }
            break;
        case DDR_CMD_READ:
            if (bank < mem->banks && row == mem->bank_rows[bank] && data) {
                uint64_t offset = ((uint64_t)bank * mem->rows * mem->cols + (uint64_t)row * mem->cols + (uint64_t)col)
                    * (uint64_t)(mem->data_width / 8);
                if (offset < mem->capacity)
                    memcpy(data, mem->storage + offset, (size_t)(mem->burst_length * mem->data_width / 8));
            }
            break;
        case DDR_CMD_PRE:
            if (bank < mem->banks) mem->bank_active[bank] = false;
            break;
        case DDR_CMD_REF:
        case DDR_CMD_NOP:
        case DDR_CMD_MRS:
            break;
    }
    return true;
}

void ddr_mem_tick(ddr_mem_t* mem) {
    if (mem) mem->cycle++;
}

void ddr_mem_initialize(ddr_mem_t* mem, bool random_fill, uint32_t seed) {
    if (!mem || !mem->storage) return;
    uint32_t rng = seed;
    for (uint64_t i = 0; i < mem->capacity && random_fill; i++) {
        rng = rng * 1664525u + 1013904223u;
        mem->storage[i] = (uint8_t)(rng & 0xFF);
    }
}

/* ----- VIP Base ----- */
vip_base_t* vip_base_create(const char* name, vip_protocol_t proto, bool is_master) {
    vip_base_t* vip = calloc(1, sizeof(vip_base_t));
    if (!vip) return NULL;
    strncpy(vip->name, name, sizeof(vip->name) - 1);
    vip->protocol = proto;
    vip->is_master = is_master;
    vip->is_active = true;
    return vip;
}

void vip_base_destroy(vip_base_t* vip) { free(vip); }

/* ----- VIP Scoreboard ----- */
vip_sb_t* vip_sb_create(const char* name, int capacity) {
    vip_sb_t* sb = calloc(1, sizeof(vip_sb_t));
    if (!sb) return NULL;
    strncpy(sb->name, name, sizeof(sb->name) - 1);
    sb->queue_capacity = capacity > 0 ? capacity : 256;
    sb->expected_queue = calloc((size_t)sb->queue_capacity, sizeof(void*));
    return sb;
}

void vip_sb_destroy(vip_sb_t* sb) {
    if (sb) { free(sb->expected_queue); free(sb); }
}

void vip_sb_set_predict(vip_sb_t* sb, vip_sb_predict_fn fn, void* ctx) {
    if (sb) { sb->predict = fn; sb->predict_ctx = ctx; }
}
void vip_sb_set_compare(vip_sb_t* sb, vip_sb_compare_fn fn, void* ctx) {
    if (sb) { sb->compare = fn; sb->compare_ctx = ctx; }
}

void vip_sb_push_expected(vip_sb_t* sb, const void* expected) {
    if (!sb || sb->queue_size >= sb->queue_capacity) return;
    sb->expected_queue[sb->queue_tail] = (void*)expected;
    sb->queue_tail = (sb->queue_tail + 1) % sb->queue_capacity;
    sb->queue_size++;
}

bool vip_sb_check_actual(vip_sb_t* sb, const void* actual) {
    if (!sb) return false;
    if (sb->queue_size <= 0) { sb->underflow_count++; return false; }
    void* exp = sb->expected_queue[sb->queue_head];
    sb->queue_head = (sb->queue_head + 1) % sb->queue_capacity;
    sb->queue_size--;
    bool match = sb->compare ? sb->compare(exp, actual, sb->compare_ctx) : (memcmp(exp, actual, 16) == 0);
    if (match) {
        sb->match_count++;
    } else {
        sb->mismatch_count++;
        snprintf(sb->last_mismatch_msg, sizeof(sb->last_mismatch_msg),
            "mismatch at item %llu", (unsigned long long)(sb->match_count + sb->mismatch_count));
    }
    return match;
}

void vip_sb_report(const vip_sb_t* sb, FILE* fp) {
    if (!sb || !fp) return;
    fprintf(fp, "[Scoreboard %s] match=%llu mismatch=%llu over=%llu under=%llu rate=%.1f%%\n",
        sb->name,
        (unsigned long long)sb->match_count, (unsigned long long)sb->mismatch_count,
        (unsigned long long)sb->overflow_count, (unsigned long long)sb->underflow_count,
        vip_sb_match_rate(sb));
}

double vip_sb_match_rate(const vip_sb_t* sb) {
    if (!sb) return 0.0;
    uint64_t total = sb->match_count + sb->mismatch_count;
    return total ? (double)sb->match_count / (double)total * 100.0 : 100.0;
}

/* ----- Regression Test ----- */
regression_reg_t* regression_reg_create(const char* name, regression_test_fn fn, void* ctx, uint32_t seed) {
    regression_reg_t* r = calloc(1, sizeof(regression_reg_t));
    if (!r) return NULL;
    strncpy(r->name, name, sizeof(r->name) - 1);
    r->test_fn = fn;
    r->ctx     = ctx;
    r->seed    = seed;
    return r;
}

void regression_reg_destroy(regression_reg_t* r) { free(r); }

bool regression_reg_run(regression_reg_t* r) {
    if (!r || !r->test_fn) return false;
    r->run_count++;
    printf("[Regression] Running test '%s' (seed=%u)...\n", r->name, r->seed);
    r->passed = r->test_fn(r->ctx);
    if (!r->passed) r->fail_count++;
    return r->passed;
}

/* ----- Regression Suite ----- */
regression_suite_t* regression_suite_create(const char* name) {
    regression_suite_t* s = calloc(1, sizeof(regression_suite_t));
    if (!s) return NULL;
    strncpy(s->name, name, sizeof(s->name) - 1);
    s->test_capacity = 16;
    s->tests = calloc((size_t)s->test_capacity, sizeof(regression_reg_t*));
    s->global_seed = 0xFEEDBEEF;
    return s;
}

void regression_suite_destroy(regression_suite_t* suite) {
    if (!suite) return;
    for (int i = 0; i < suite->test_count; i++)
        regression_reg_destroy(suite->tests[i]);
    free(suite->tests);
    free(suite);
}

void regression_suite_add_test(regression_suite_t* suite, regression_reg_t* test) {
    if (!suite || !test) return;
    if (suite->test_count >= suite->test_capacity) {
        suite->test_capacity *= 2;
        suite->tests = realloc(suite->tests,
            (size_t)suite->test_capacity * sizeof(regression_reg_t*));
    }
    suite->tests[suite->test_count++] = test;
}

void regression_suite_set_seed(regression_suite_t* suite, uint32_t seed) {
    if (suite) suite->global_seed = seed;
}

void regression_suite_run(regression_suite_t* suite) {
    if (!suite) return;
    printf("\n====== Regression Suite: %s ======\n", suite->name);
    printf("Total tests: %d  Seed: 0x%08X\n", suite->test_count, suite->global_seed);
    suite->total_runs = 0;
    suite->total_passed = 0;
    suite->total_failed = 0;
    for (int i = 0; i < suite->test_count; i++) {
        regression_reg_t* t = suite->tests[i];
        if (suite->random_order) t->seed = suite->global_seed + (uint32_t)(i * 31337);
        bool pass = regression_reg_run(t);
        suite->total_runs++;
        if (pass) suite->total_passed++; else suite->total_failed++;
        if (!pass && suite->stop_on_fail) {
            printf("[Regression] Stopping on first failure.\n");
            break;
        }
    }
}

void regression_suite_report(const regression_suite_t* suite, FILE* fp) {
    if (!suite || !fp) return;
    fprintf(fp, "\n===== Regression Report: %s =====\n", suite->name);
    fprintf(fp, "  Total:  %d\n", suite->total_runs);
    fprintf(fp, "  Passed: %d (%.1f%%)\n",
        suite->total_passed,
        suite->total_runs ? (double)suite->total_passed / (double)suite->total_runs * 100.0 : 0.0);
    fprintf(fp, "  Failed: %d\n", suite->total_failed);
    for (int i = 0; i < suite->test_count; i++) {
        regression_reg_t* t = suite->tests[i];
        fprintf(fp, "    [%s] %s (seed=0x%08X)\n",
            t->passed ? "PASS" : "FAIL", t->name, t->seed);
        if (!t->passed && t->fail_msg[0])
            fprintf(fp, "      -> %s\n", t->fail_msg);
    }
}

void regression_suite_export_html(const regression_suite_t* suite, const char* dir) {
    if (!suite || !dir) return;
    printf("[Regression] Exporting HTML report to %s\n", dir);
}

/* ----- Seed Manager ----- */
seed_mgr_t* seed_mgr_create(uint32_t master_seed) {
    seed_mgr_t* mgr = calloc(1, sizeof(seed_mgr_t));
    if (!mgr) return NULL;
    mgr->master_seed    = master_seed;
    mgr->current_seed   = master_seed;
    mgr->rng_state      = master_seed;
    mgr->history_capacity = 64;
    mgr->seed_history   = calloc((size_t)mgr->history_capacity, sizeof(uint32_t));
    return mgr;
}

void seed_mgr_destroy(seed_mgr_t* mgr) {
    if (mgr) { free(mgr->seed_history); free(mgr); }
}

uint32_t seed_mgr_next(seed_mgr_t* mgr) {
    if (!mgr) return 0;
    mgr->rng_state = mgr->rng_state * 1664525u + 1013904223u;
    if (mgr->history_count < mgr->history_capacity)
        mgr->seed_history[mgr->history_count++] = mgr->rng_state;
    mgr->current_seed = mgr->rng_state;
    return mgr->rng_state;
}

uint32_t seed_mgr_get_current(const seed_mgr_t* mgr) {
    return mgr ? mgr->current_seed : 0;
}

void seed_mgr_reset(seed_mgr_t* mgr) {
    if (mgr) {
        mgr->current_seed = mgr->master_seed;
        mgr->rng_state    = mgr->master_seed;
    }
}

void seed_mgr_save_state(seed_mgr_t* mgr) {
    if (mgr && mgr->history_count < mgr->history_capacity)
        mgr->seed_history[mgr->history_count++] = mgr->current_seed;
}

uint32_t seed_mgr_load_state(seed_mgr_t* mgr, int index) {
    if (!mgr || index < 0 || index >= mgr->history_count) return 0;
    mgr->current_seed = mgr->seed_history[index];
    mgr->rng_state    = mgr->current_seed;
    return mgr->current_seed;
}

/* ----- UVC ----- */
uvc_t* uvc_create(const char* name, vip_protocol_t proto) {
    uvc_t* uvc = calloc(1, sizeof(uvc_t));
    if (!uvc) return NULL;
    strncpy(uvc->name, name, sizeof(uvc->name) - 1);
    uvc->protocol = proto;
    return uvc;
}

void uvc_destroy(uvc_t* uvc) { free(uvc); }
void uvc_configure(uvc_t* uvc)  { if (uvc) uvc->is_configured = true; }
void uvc_run(uvc_t* uvc) {
    if (!uvc) return;
    printf("[UVC %s] running (protocol=%s)\n", uvc->name, vip_protocol_name(uvc->protocol));
}
void uvc_report(const uvc_t* uvc, FILE* fp) {
    if (!uvc || !fp) return;
    fprintf(fp, "[UVC %s] protocol=%s configured=%d\n",
        uvc->name, vip_protocol_name(uvc->protocol), uvc->is_configured);
}

/* ----- Pass/Fail Tracker ----- */
pf_tracker_t* pf_tracker_create(const char* name) {
    pf_tracker_t* t = calloc(1, sizeof(pf_tracker_t));
    if (!t) return NULL;
    strncpy(t->name, name, sizeof(t->name) - 1);
    t->fail_msg_capacity = 64;
    t->fail_messages = calloc((size_t)t->fail_msg_capacity, sizeof(char*));
    return t;
}

void pf_tracker_destroy(pf_tracker_t* tracker) {
    if (!tracker) return;
    for (int i = 0; i < tracker->fail_msg_count; i++)
        free(tracker->fail_messages[i]);
    free(tracker->fail_messages);
    free(tracker);
}

void pf_tracker_record_pass(pf_tracker_t* t) {
    if (t) { t->total++; t->passed++; t->pass_rate = t->total ? (double)t->passed / (double)t->total * 100.0 : 0.0; }
}
void pf_tracker_record_fail(pf_tracker_t* t, const char* msg) {
    if (!t) return;
    t->total++; t->failed++;
    t->pass_rate = t->total ? (double)t->passed / (double)t->total * 100.0 : 0.0;
    if (msg && t->fail_msg_count < t->fail_msg_capacity) {
        t->fail_messages[t->fail_msg_count++] = strdup(msg);
    }
}
void pf_tracker_record_warning(pf_tracker_t* t) { if (t) t->warnings++; }
void pf_tracker_record_abort(pf_tracker_t* t)   { if (t) t->aborted++; }

void pf_tracker_report(const pf_tracker_t* t, FILE* fp) {
    if (!t || !fp) return;
    fprintf(fp, "\n[Tracker %s] total=%d pass=%d fail=%d warn=%d abort=%d rate=%.1f%%\n",
        t->name, t->total, t->passed, t->failed, t->warnings, t->aborted, t->pass_rate);
    for (int i = 0; i < t->fail_msg_count; i++)
        fprintf(fp, "  FAIL[%d]: %s\n", i, t->fail_messages[i]);
}

bool pf_tracker_all_passed(const pf_tracker_t* t) {
    return t && t->failed == 0 && t->aborted == 0;
}

/* ----- Verification Config ----- */
vcfg_t* verif_config_create(void) {
    vcfg_t* cfg = calloc(1, sizeof(vcfg_t));
    if (!cfg) return NULL;
    cfg->seed              = 42;
    cfg->tests_to_run      = 100;
    cfg->enable_coverage   = true;
    cfg->enable_assertions = true;
    cfg->enable_formal     = false;
    cfg->enable_scoreboard = true;
    cfg->timeout_seconds   = 3600.0;
    cfg->verbosity         = 2;
    return cfg;
}

void verif_config_destroy(vcfg_t* cfg) { free(cfg); }

void verif_config_load(vcfg_t* cfg, const char* filename) {
    if (!cfg || !filename) return;
    printf("[Config] Loading from %s\n", filename);
}

void verif_config_save(const vcfg_t* cfg, const char* filename) {
    if (!cfg || !filename) return;
    printf("[Config] Saving to %s\n", filename);
}

/* ----- Protocol Analyzer ----- */
proto_analyzer_t* proto_analyzer_create(const char* name, vip_protocol_t proto) {
    proto_analyzer_t* pa = calloc(1, sizeof(proto_analyzer_t));
    if (!pa) return NULL;
    strncpy(pa->name, name, sizeof(pa->name) - 1);
    pa->protocol = proto;
    pa->hist_bins = 64;
    pa->latency_histogram = calloc((size_t)pa->hist_bins, sizeof(int));
    pa->min_latency = UINT64_MAX;
    return pa;
}

void proto_analyzer_destroy(proto_analyzer_t* pa) {
    if (pa) {
        if (pa->log_fp) fclose(pa->log_fp);
        free(pa->latency_histogram);
        free(pa);
    }
}

void proto_analyzer_log(proto_analyzer_t* pa, const char* direction, const void* txn) {
    if (!pa) return;
    pa->txn_count++;
    if (pa->enable_logging && pa->log_fp && txn) {
        fprintf(pa->log_fp, "[%s] %s txn=%llu\n", direction, vip_protocol_name(pa->protocol),
            (unsigned long long)pa->txn_count);
    }
}

void proto_analyzer_record_latency(proto_analyzer_t* pa, uint64_t latency) {
    if (!pa) return;
    pa->latency_sum += latency;
    if (latency > pa->max_latency) pa->max_latency = latency;
    if (latency < pa->min_latency) pa->min_latency = latency;
    int bin = latency < (uint64_t)pa->hist_bins ? (int)latency : pa->hist_bins - 1;
    pa->latency_histogram[bin]++;
}

void proto_analyzer_report(const proto_analyzer_t* pa, FILE* fp) {
    if (!pa || !fp) return;
    fprintf(fp, "[Analyzer %s] protocol=%s txns=%llu bytes=%llu avg_lat=%.1f min=%llu max=%llu\n",
        pa->name, vip_protocol_name(pa->protocol),
        (unsigned long long)pa->txn_count, (unsigned long long)pa->byte_count,
        pa->txn_count ? (double)pa->latency_sum / (double)pa->txn_count : 0.0,
        (unsigned long long)pa->min_latency, (unsigned long long)pa->max_latency);
    fprintf(fp, "  Latency histogram: [");
    for (int i = 0; i < pa->hist_bins; i++)
        fprintf(fp, "%c", pa->latency_histogram[i] > 0 ? '#' : '.');
    fprintf(fp, "]\n");
}
