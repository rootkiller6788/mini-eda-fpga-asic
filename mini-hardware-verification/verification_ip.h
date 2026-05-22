#ifndef VERIFICATION_IP_H
#define VERIFICATION_IP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ================================================================
   Verification IP — Protocol VIPs, BFM, Scoreboard (C99)
   verification_ip.h
   ================================================================ */

/* ----- Protocol Enumeration ----- */
typedef enum {
    VIP_PROTO_AXI4      = 0,
    VIP_PROTO_AXI4_LITE = 1,
    VIP_PROTO_AXI4_STREAM = 2,
    VIP_PROTO_PCIE_GEN3 = 3,
    VIP_PROTO_PCIE_GEN4 = 4,
    VIP_PROTO_PCIE_GEN5 = 5,
    VIP_PROTO_DDR4      = 6,
    VIP_PROTO_DDR5      = 7,
    VIP_PROTO_APB       = 8,
    VIP_PROTO_AHB       = 9,
    VIP_PROTO_WISHBONE  = 10,
    VIP_PROTO_I2C       = 11,
    VIP_PROTO_SPI       = 12,
    VIP_PROTO_UART      = 13,
    VIP_PROTO_ETHERNET  = 14,
    VIP_PROTO_USB3      = 15,
    VIP_PROTO_COUNT
} vip_protocol_t;

static inline const char* vip_protocol_name(vip_protocol_t proto) {
    static const char* names[] = {
        "AXI4", "AXI4-Lite", "AXI4-Stream",
        "PCIe-Gen3", "PCIe-Gen4", "PCIe-Gen5",
        "DDR4", "DDR5", "APB", "AHB", "Wishbone",
        "I2C", "SPI", "UART", "Ethernet", "USB3"
    };
    return (proto < VIP_PROTO_COUNT) ? names[proto] : "unknown";
}

/* ----- AXI4 Transaction ----- */
typedef struct axi4_transaction axi4_trans_t;

typedef enum {
    AXI_BURST_FIXED = 0,
    AXI_BURST_INCR  = 1,
    AXI_BURST_WRAP  = 2
} axi_burst_type_t;

typedef enum {
    AXI_RESP_OKAY   = 0,
    AXI_RESP_EXOKAY = 1,
    AXI_RESP_SLVERR = 2,
    AXI_RESP_DECERR = 3
} axi_resp_t;

#define AXI_ADDR_WIDTH  64
#define AXI_DATA_WIDTH  64
#define AXI_ID_WIDTH    8

struct axi4_transaction {
    uint64_t        addr;
    uint8_t         id;
    uint8_t         len;          /* burst length (1-256, encoded as 0-255) */
    uint8_t         size;         /* bytes per beat (2^size, up to 128) */
    axi_burst_type_t burst;
    bool            is_write;
    uint64_t        data[256];    /* max 256 beats */
    uint64_t        strobe[256];  /* write strobes */
    axi_resp_t      resp;
    bool            last;
    uint32_t        latency;      /* response latency in cycles */
};

axi4_trans_t* axi4_trans_create(void);
void axi4_trans_destroy(axi4_trans_t* t);
void axi4_trans_set_addr(axi4_trans_t* t, uint64_t addr);
void axi4_trans_set_burst(axi4_trans_t* t, uint8_t len,
    uint8_t size, axi_burst_type_t burst);
void axi4_trans_set_data(axi4_trans_t* t, int beat,
    uint64_t data, uint64_t strobe);
uint64_t axi4_trans_get_data(const axi4_trans_t* t, int beat);

/* ----- AXI4 Bus Functional Model (BFM) ----- */
typedef struct axi4_bfm axi4_bfm_t;

typedef void (*axi4_bfm_write_cb)(axi4_bfm_t* bfm,
    const axi4_trans_t* trans, void* ctx);
typedef void (*axi4_bfm_read_cb)(axi4_bfm_t* bfm,
    const axi4_trans_t* trans, void* ctx);

struct axi4_bfm {
    char             name[64];
    vip_protocol_t   protocol;
    uint64_t*        memory;         /* simulated memory space */
    uint64_t         mem_size;       /* size in 64-bit words */
    int              outstanding_tx; /* pending transactions */
    axi4_bfm_write_cb on_write;
    axi4_bfm_read_cb  on_read;
    void*            cb_ctx;
    uint64_t         cycle_count;
    uint32_t         seed;
    bool             is_master;      /* master or slave BFM */
    bool             enable_backdoor;
};

axi4_bfm_t* axi4_bfm_create(const char* name, bool is_master);
void axi4_bfm_destroy(axi4_bfm_t* bfm);
void axi4_bfm_set_memory(axi4_bfm_t* bfm, uint64_t* mem, uint64_t size);
void axi4_bfm_set_callbacks(axi4_bfm_t* bfm,
    axi4_bfm_write_cb wcb, axi4_bfm_read_cb rcb, void* ctx);
bool axi4_bfm_write(axi4_bfm_t* bfm, axi4_trans_t* trans);
bool axi4_bfm_read(axi4_bfm_t* bfm, axi4_trans_t* trans);
void axi4_bfm_tick(axi4_bfm_t* bfm);

/* ----- PCIe Transaction Layer Packet (TLP) ----- */
typedef struct pcie_tlp pcie_tlp_t;

typedef enum {
    PCIE_TLP_MEM_RD    = 0,
    PCIE_TLP_MEM_WR    = 1,
    PCIE_TLP_CFG_RD    = 2,
    PCIE_TLP_CFG_WR    = 3,
    PCIE_TLP_MSG       = 4,
    PCIE_TLP_CPL       = 5,
    PCIE_TLP_CPLD      = 6
} pcie_tlp_type_t;

struct pcie_tlp {
    pcie_tlp_type_t  type;
    uint16_t         requester_id;
    uint8_t          tag;
    uint64_t         addr;
    uint32_t         data[256];     /* max payload 1024 bytes */
    uint16_t         length;        /* in dwords */
    uint8_t          tc;            /* traffic class */
    bool             is_posted;
    int              max_payload;
};

pcie_tlp_t* pcie_tlp_create(void);
void pcie_tlp_destroy(pcie_tlp_t* tlp);

/* ----- DDR Memory Model ----- */
typedef struct ddr_mem_model ddr_mem_t;

typedef enum {
    DDR_CMD_READ   = 0,
    DDR_CMD_WRITE  = 1,
    DDR_CMD_ACT    = 2,  /* activate */
    DDR_CMD_PRE    = 3,  /* precharge */
    DDR_CMD_REF    = 4,  /* refresh */
    DDR_CMD_NOP    = 5,
    DDR_CMD_MRS    = 6   /* mode register set */
} ddr_cmd_t;

struct ddr_mem_model {
    char         name[64];
    vip_protocol_t protocol;
    uint64_t     capacity;       /* in bytes */
    uint8_t*     storage;
    int          banks;
    int          rows;
    int          cols;
    int          data_width;
    int          burst_length;
    int          tRCD;           /* RAS-to-CAS delay */
    int          tRP;            /* precharge time */
    int          tRAS;           /* row active time */
    int          tRFC;           /* refresh cycle time */
    int          tWR;            /* write recovery */
    int         *bank_rows;      /* currently active row per bank */
    bool         *bank_active;
    uint64_t     cycle;
};

ddr_mem_t* ddr_mem_create(const char* name, vip_protocol_t proto,
    uint64_t capacity);
void ddr_mem_destroy(ddr_mem_t* mem);
bool ddr_mem_issue_command(ddr_mem_t* mem, ddr_cmd_t cmd,
    int bank, int row, int col, uint64_t* data);
void ddr_mem_tick(ddr_mem_t* mem);
void ddr_mem_initialize(ddr_mem_t* mem, bool random_fill,
    uint32_t seed);

/* ----- Generic VIP Base ----- */
typedef struct vip_base vip_base_t;

struct vip_base {
    char            name[64];
    vip_protocol_t  protocol;
    bool            is_active;
    bool            is_master;
    uint64_t        cycle_count;
    char            version[16];
    void*           user_data;
    void*           agent_ref;
};

vip_base_t* vip_base_create(const char* name, vip_protocol_t proto,
    bool is_master);
void vip_base_destroy(vip_base_t* vip);

/* ----- Scoreboard ----- */
typedef struct vip_scoreboard vip_sb_t;

typedef bool (*vip_sb_predict_fn)(const void* stimulus,
    void* predicted, void* ctx);
typedef bool (*vip_sb_compare_fn)(const void* predicted,
    const void* actual, void* ctx);

struct vip_scoreboard {
    char               name[64];
    vip_sb_predict_fn  predict;
    vip_sb_compare_fn  compare;
    void*              predict_ctx;
    void*              compare_ctx;
    void**             expected_queue;
    int                queue_size;
    int                queue_head;
    int                queue_tail;
    int                queue_capacity;
    uint64_t           match_count;
    uint64_t           mismatch_count;
    uint64_t           overflow_count;
    uint64_t           underflow_count;
    bool               halt_on_mismatch;
    char               last_mismatch_msg[512];
};

vip_sb_t* vip_sb_create(const char* name, int capacity);
void vip_sb_destroy(vip_sb_t* sb);
void vip_sb_set_predict(vip_sb_t* sb, vip_sb_predict_fn fn,
    void* ctx);
void vip_sb_set_compare(vip_sb_t* sb, vip_sb_compare_fn fn,
    void* ctx);
void vip_sb_push_expected(vip_sb_t* sb, const void* expected);
bool vip_sb_check_actual(vip_sb_t* sb, const void* actual);
void vip_sb_report(const vip_sb_t* sb, FILE* fp);
double vip_sb_match_rate(const vip_sb_t* sb);

/* ----- Regression Testing ----- */
typedef struct regression_reg regression_reg_t;

typedef bool (*regression_test_fn)(void* ctx);

struct regression_reg {
    char               name[64];
    regression_test_fn test_fn;
    void*              ctx;
    uint32_t           seed;
    bool               passed;
    uint64_t           duration_us;
    char               fail_msg[256];
    int                fail_count;
    int                run_count;
};

regression_reg_t* regression_reg_create(const char* name,
    regression_test_fn fn, void* ctx, uint32_t seed);
void regression_reg_destroy(regression_reg_t* reg);
bool regression_reg_run(regression_reg_t* reg);

/* ----- Regression Suite ----- */
typedef struct regression_suite regression_suite_t;

struct regression_suite {
    char              name[64];
    regression_reg_t** tests;
    int               test_count;
    int               test_capacity;
    uint32_t          global_seed;
    int               total_runs;
    int               total_passed;
    int               total_failed;
    bool              stop_on_fail;
    bool              random_order;
    double            timeout_per_test_sec;
    char              report_dir[256];
};

regression_suite_t* regression_suite_create(const char* name);
void regression_suite_destroy(regression_suite_t* suite);
void regression_suite_add_test(regression_suite_t* suite,
    regression_reg_t* test);
void regression_suite_set_seed(regression_suite_t* suite,
    uint32_t seed);
void regression_suite_run(regression_suite_t* suite);
void regression_suite_report(const regression_suite_t* suite,
    FILE* fp);
void regression_suite_export_html(const regression_suite_t* suite,
    const char* dir);

/* ----- Random Seed Management ----- */
typedef struct random_seed_mgr seed_mgr_t;

typedef uint32_t (*seed_random_fn)(uint32_t* state);

struct random_seed_mgr {
    uint32_t        master_seed;
    uint32_t        current_seed;
    uint32_t        rng_state;
    seed_random_fn  rng;
    uint32_t*       seed_history;
    int             history_count;
    int             history_capacity;
};

seed_mgr_t* seed_mgr_create(uint32_t master_seed);
void seed_mgr_destroy(seed_mgr_t* mgr);
uint32_t seed_mgr_next(seed_mgr_t* mgr);
uint32_t seed_mgr_get_current(const seed_mgr_t* mgr);
void seed_mgr_reset(seed_mgr_t* mgr);
void seed_mgr_save_state(seed_mgr_t* mgr);
uint32_t seed_mgr_load_state(seed_mgr_t* mgr, int index);

/* ----- Universal Verification Component (UVC) ----- */
typedef struct uvc_component uvc_t;

struct uvc_component {
    char           name[64];
    vip_protocol_t protocol;
    vip_base_t*    base;
    void*          bfm;            /* protocol-specific BFM */
    vip_sb_t*      scoreboard;
    void*          monitor;
    void*          coverage_group;
    bool           is_configured;
    void*          env_ref;
};

uvc_t* uvc_create(const char* name, vip_protocol_t proto);
void uvc_destroy(uvc_t* uvc);
void uvc_configure(uvc_t* uvc);
void uvc_run(uvc_t* uvc);
void uvc_report(const uvc_t* uvc, FILE* fp);

/* ----- Pass / Fail Tracking ----- */
typedef struct pass_fail_tracker pf_tracker_t;

struct pass_fail_tracker {
    char     name[64];
    int      total;
    int      passed;
    int      failed;
    int      warnings;
    int      aborted;
    double   pass_rate;
    char**   fail_messages;
    int      fail_msg_count;
    int      fail_msg_capacity;
};

pf_tracker_t* pf_tracker_create(const char* name);
void pf_tracker_destroy(pf_tracker_t* tracker);
void pf_tracker_record_pass(pf_tracker_t* t);
void pf_tracker_record_fail(pf_tracker_t* t, const char* msg);
void pf_tracker_record_warning(pf_tracker_t* t);
void pf_tracker_record_abort(pf_tracker_t* t);
void pf_tracker_report(const pf_tracker_t* t, FILE* fp);
bool pf_tracker_all_passed(const pf_tracker_t* t);

/* ----- Verification Environment Config ----- */
typedef struct verif_config vcfg_t;

struct verif_config {
    uint32_t    seed;
    uint32_t    tests_to_run;
    bool        enable_coverage;
    bool        enable_assertions;
    bool        enable_formal;
    bool        enable_scoreboard;
    double      timeout_seconds;
    char        log_file[256];
    char        wave_file[256];
    int         verbosity;
};

vcfg_t* verif_config_create(void);
void verif_config_destroy(vcfg_t* cfg);
void verif_config_load(vcfg_t* cfg, const char* filename);
void verif_config_save(const vcfg_t* cfg, const char* filename);

/* ----- Protocol Analyzer ----- */
typedef struct protocol_analyzer proto_analyzer_t;

struct protocol_analyzer {
    char            name[64];
    vip_protocol_t  protocol;
    FILE*           log_fp;
    bool            enable_logging;
    uint64_t        txn_count;
    uint64_t        byte_count;
    uint64_t        latency_sum;
    double          throughput_bps;
    int             *latency_histogram;
    int             hist_bins;
    uint64_t        max_latency;
    uint64_t        min_latency;
};

proto_analyzer_t* proto_analyzer_create(const char* name,
    vip_protocol_t proto);
void proto_analyzer_destroy(proto_analyzer_t* pa);
void proto_analyzer_log(proto_analyzer_t* pa,
    const char* direction, const void* txn);
void proto_analyzer_record_latency(proto_analyzer_t* pa,
    uint64_t latency);
void proto_analyzer_report(const proto_analyzer_t* pa, FILE* fp);

#endif /* VERIFICATION_IP_H */
