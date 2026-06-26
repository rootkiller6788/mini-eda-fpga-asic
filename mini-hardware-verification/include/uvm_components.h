/**
 * uvm_components.h - UVM-alike Verification Component Hierarchy
 *
 * Implements IEEE 1800.2-2020 UVM class hierarchy in C:
 *   Monitor -> passive bus observer
 *   Driver  -> pin-level stimulus driver
 *   Sequencer -> sequence arbitration
 *   Scoreboard -> golden reference checker
 *   Agent -> monitor+driver+sequencer container
 *   Env -> top-level verification environment
 *
 * Coverage: L1(structs), L2(phasing), L3(TLM), L5(seq-arbitration), L7(RISC-V env)
 * Course: UT Austin ECE 382V, CMU 18-240, ETH 263-0006
 */

#ifndef UVM_COMPONENTS_H
#define UVM_COMPONENTS_H

#include "hw_verify.h"
#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 * L1: Transaction-Level Types
 * ======================================================================== */

typedef struct hv_transaction {
    uint32_t        addr;
    uint32_t        data;
    uint32_t        size;
    uint8_t         cmd;       /* 0=read, 1=write, 2=idle, 3=nop */
    uint8_t         resp;      /* 0=OK, 1=ERROR, 2=RETRY, 3=DECERR */
    sim_time_t      timestamp;
    uint64_t        trans_id;
    uint32_t        user_data[4];
} hv_transaction_t;

typedef struct hv_sequence_item {
    hv_transaction_t trans;
    bool             is_done;
    sim_time_t       start_time;
    sim_time_t       end_time;
    struct hv_sequence_item *next;
    struct hv_sequence_item *prev;
} hv_sequence_item_t;

typedef struct hv_sequence hv_sequence_t;
struct hv_sequence {
    char              name[64];
    hv_sequence_item_t *head;
    hv_sequence_item_t *tail;
    size_t            count;
    bool              is_running;
    void            (*body)(hv_sequence_t *seq);
    hv_sequence_item_t *(*get_next_item)(hv_sequence_t *seq);
    void             *user_data;
};

/* ========================================================================
 * L1: Component type tags
 * ======================================================================== */

typedef enum {
    COMP_MONITOR    = 0,
    COMP_DRIVER     = 1,
    COMP_SEQUENCER  = 2,
    COMP_SCOREBOARD = 3,
    COMP_AGENT      = 4,
    COMP_ENV        = 5,
    COMP_TEST       = 6,
} uvm_component_type_t;

/* ========================================================================
 * L2: TLM Port / Export / Imp (Transaction-Level Modeling)
 * ======================================================================== */

typedef struct hv_tlm_port     hv_tlm_port_t;
typedef struct hv_tlm_export   hv_tlm_export_t;
typedef struct hv_tlm_imp      hv_tlm_imp_t;

struct hv_tlm_port {
    char              name[64];
    hv_tlm_export_t  *connected_export;
    void            (*write)(hv_tlm_port_t *port, hv_transaction_t *t);
    hv_transaction_t *(*read)(hv_tlm_port_t *port);
    void             *owner;
};

struct hv_tlm_export {
    char              name[64];
    hv_tlm_imp_t     *connected_imp;
    hv_tlm_port_t    *bound_port;
    void             *owner;
};

struct hv_tlm_imp {
    char              name[64];
    void            (*write_impl)(hv_tlm_imp_t *imp, hv_transaction_t *t);
    hv_transaction_t *(*read_impl)(hv_tlm_imp_t *imp);
    void             *owner;
};

/* ========================================================================
 * L1 & L2: Concrete UVM Component Structs
 * ======================================================================== */

/* --- Monitor: passive observer of bus transactions --- */
typedef struct hv_monitor {
    hv_component_t    base;
    hv_dut_t         *dut;
    hv_tlm_port_t    *analysis_port;       /* broadcasts observed items */
    hv_sequence_item_t *pending_items;
    size_t            pending_count;
    bool              watch_read;
    bool              watch_write;
    bool              watch_idle;
    uint64_t          tx_seen;
    uint64_t          tx_error;
    void            (*collect_transaction)(struct hv_monitor *m);
} hv_monitor_t;

/* --- Driver: pin-level stimulus driver --- */
typedef struct hv_driver {
    hv_component_t    base;
    hv_dut_t         *dut;
    hv_tlm_imp_t     *seq_item_port;       /* receives from sequencer */
    hv_signal_t      *clk;
    hv_signal_t      *rst_n;
    hv_sequence_item_t *current_item;
    bool              is_busy;
    uint64_t          items_driven;
    uint64_t          cycles_driven;
    void            (*drive_transaction)(struct hv_driver *drv,
                                          hv_transaction_t *tx);
} hv_driver_t;

/* --- Sequencer: arbitrates multiple sequences --- */
typedef struct hv_sequencer {
    hv_component_t    base;
    hv_sequence_t    *active_sequences[16];
    size_t            num_active;
    hv_sequence_t    *pending_sequences;
    size_t            num_pending;
    hv_tlm_port_t    *seq_item_port;
    uint64_t          items_processed;
    uint32_t          rr_index;
    hv_sequence_item_t *(*arbitrate)(struct hv_sequencer *sqr);
} hv_sequencer_t;

/* --- Scoreboard: reference model + golden checker --- */
struct hv_scoreboard_expected {
    hv_transaction_t  tx;
    bool              matched;
    struct hv_scoreboard_expected *next;
};

typedef struct hv_scoreboard {
    hv_component_t    base;
    hv_tlm_imp_t     *monitor_imp;
    struct hv_scoreboard_expected *expected_list;
    size_t            expected_count;
    uint64_t          tx_checked;
    uint64_t          tx_matched;
    uint64_t          tx_mismatch;
    uint64_t          tx_unexpected;
    hv_transaction_t  (*predict)(struct hv_scoreboard *sb, hv_transaction_t *in);
    void            (*add_expected)(struct hv_scoreboard *sb, hv_transaction_t *tx);
    bool            (*check)(struct hv_scoreboard *sb, hv_transaction_t *actual);
} hv_scoreboard_t;

/* --- Agent: container for monitor + driver + sequencer --- */
typedef struct hv_agent {
    hv_component_t    base;
    uvm_component_type_t agent_type;
    hv_monitor_t     *monitor;
    hv_driver_t      *driver;
    hv_sequencer_t   *sequencer;
    hv_dut_t         *dut;
    bool              is_active;
} hv_agent_t;

/* --- Environment: top-level container --- */
typedef struct hv_env {
    hv_component_t    base;
    hv_agent_t       *agents[8];
    size_t            num_agents;
    hv_scoreboard_t  *scoreboard;
    hv_config_db_t   *config_db;
    hv_verify_plan_t *verify_plan;
} hv_env_t;

/* ========================================================================
 * L2 & L3: TLM Connection API
 * ======================================================================== */

hv_tlm_port_t   *hv_tlm_port_create(const char *name, void *owner);
void             hv_tlm_port_destroy(hv_tlm_port_t *port);
hv_tlm_export_t *hv_tlm_export_create(const char *name, void *owner);
void             hv_tlm_export_destroy(hv_tlm_export_t *export_);
hv_tlm_imp_t    *hv_tlm_imp_create(const char *name, void *owner,
                                    void (*write_cb)(hv_tlm_imp_t*, hv_transaction_t*));
void             hv_tlm_imp_destroy(hv_tlm_imp_t *imp);
void             hv_tlm_connect(hv_tlm_port_t *port, hv_tlm_export_t *export_);
void             hv_tlm_bind(hv_tlm_export_t *export_, hv_tlm_imp_t *imp);

/* ========================================================================
 * L2: Component Factory API
 * ======================================================================== */

hv_monitor_t    *hv_monitor_create(const char *name, hv_dut_t *dut);
void             hv_monitor_destroy(hv_monitor_t *m);
hv_driver_t     *hv_driver_create(const char *name, hv_dut_t *dut);
void             hv_driver_destroy(hv_driver_t *drv);
hv_sequencer_t  *hv_sequencer_create(const char *name);
void             hv_sequencer_destroy(hv_sequencer_t *sqr);
hv_scoreboard_t *hv_scoreboard_create(const char *name);
void             hv_scoreboard_destroy(hv_scoreboard_t *sb);
hv_agent_t      *hv_agent_create(const char *name, hv_dut_t *dut, bool active);
void             hv_agent_destroy(hv_agent_t *agent);
hv_env_t        *hv_env_create(const char *name);
void             hv_env_destroy(hv_env_t *env);

/* ========================================================================
 * L5: Sequence Generation API
 * ======================================================================== */

hv_sequence_t   *hv_sequence_create(const char *name,
                                     void (*body)(hv_sequence_t*));
void             hv_sequence_destroy(hv_sequence_t *seq);
void             hv_sequence_add_item(hv_sequence_t *seq, hv_sequence_item_t *item);
hv_sequence_item_t *hv_sequence_item_create(void);
void             hv_sequence_item_destroy(hv_sequence_item_t *item);
hv_sequence_item_t *hv_sequencer_rr_arbitrate(hv_sequencer_t *sqr);

/* ========================================================================
 * L2: Phase execution API
 * ======================================================================== */

void hv_monitor_run_phase(hv_component_t *comp);
void hv_driver_run_phase(hv_component_t *comp);
void hv_sequencer_run_phase(hv_component_t *comp);
void hv_scoreboard_run_phase(hv_component_t *comp);
void hv_agent_run_phase(hv_component_t *comp);
void hv_env_run_phase(hv_component_t *comp);

#endif /* UVM_COMPONENTS_H */
