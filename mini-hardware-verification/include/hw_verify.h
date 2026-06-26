/**
 * mini-hardware-verification — Core Verification Framework
 *
 * Covers L1-L9 knowledge layers for hardware functional verification.
 * Key concepts: UVM-alike testbench architecture, constrained-random stimulus,
 * coverage-driven methodology, formal property checking, event-driven simulation.
 *
 * Course Mapping:
 *   MIT 6.004 (Computation Structures) — verified RTL
 *   UT Austin ECE 382V (VLSI) — functional verification
 *   CMU 18-240 (Digital Systems) — testbench methodology
 *   Stanford EE 272 (Design Projects) — verification planning
 *   ETH 263-0006 (Computer Architecture) — RTL verification
 */

#ifndef HW_VERIFY_H
#define HW_VERIFY_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

/* ============================================================================
 * L1: Core Definitions — Data Structures & API
 * ============================================================================ */

/* --- Verification Result (IEEE 1800-2017 UVM-aligned) --- */
typedef enum {
    VERIFY_PASS       = 0,
    VERIFY_FAIL       = 1,
    VERIFY_TIMEOUT    = 2,
    VERIFY_ERROR      = 3,
    VERIFY_NOT_RUN    = 4,
} verify_result_t;

/* --- Severity levels (matching UVM verbosity) --- */
typedef enum {
    SEV_INFO    = 0,
    SEV_WARNING = 1,
    SEV_ERROR   = 2,
    SEV_FATAL   = 3,
} severity_t;

/* --- Simulation time type --- */
typedef uint64_t sim_time_t;

/* --- Signal value types (4-state VHDL/Verilog logic) --- */
typedef enum {
    LOGIC_0 = 0,   /* strong 0 */
    LOGIC_1 = 1,   /* strong 1 */
    LOGIC_X = 2,   /* unknown  */
    LOGIC_Z = 3,   /* high-impedance */
} logic_value_t;

/* --- Signal edge transition type --- */
typedef enum {
    EDGE_NONE  = 0,
    EDGE_RISE  = 1,  /* 0 → 1 or X/Z → 1 */
    EDGE_FALL  = 2,  /* 1 → 0 or X/Z → 0 */
    EDGE_ANY   = 3,
} edge_type_t;

/* --- Signal handle (forward-declared opaque pointer) --- */
typedef struct hv_signal hv_signal_t;

/* --- DUT Port direction --- */
typedef enum {
    PORT_INPUT     = 0,
    PORT_OUTPUT    = 1,
    PORT_INOUT     = 2,
    PORT_INTERNAL  = 3,
} port_dir_t;

/* --- DUT (Device Under Test) descriptor --- */
typedef struct hv_dut {
    char            name[128];
    uint32_t        num_inputs;
    uint32_t        num_outputs;
    uint32_t        num_internals;
    uint32_t        num_signals;       /* total = inputs + outputs + internals */
    hv_signal_t   **signals;           /* array of signal pointers */
    /* callback for cycle-accurate model evaluation */
    void          (*eval_cb)(struct hv_dut *dut);
    void           *user_data;
} hv_dut_t;

/* --- Test phase (UVM phase-aligned) --- */
typedef enum {
    PHASE_BUILD           = 0,
    PHASE_CONNECT         = 1,
    PHASE_END_OF_ELAB     = 2,
    PHASE_START_OF_SIM    = 3,
    PHASE_RUN             = 4,
    PHASE_POST_RUN        = 5,
    PHASE_EXTRACT         = 6,
    PHASE_CHECK           = 7,
    PHASE_REPORT          = 8,
    PHASE_FINAL           = 9,
} test_phase_t;

/* --- Testbench component base --- */
typedef struct hv_component {
    char            name[64];
    void          (*build_phase)(struct hv_component *comp);
    void          (*connect_phase)(struct hv_component *comp);
    void          (*run_phase)(struct hv_component *comp);
    void          (*report_phase)(struct hv_component *comp);
    void           *impl;              /* component-specific data */
} hv_component_t;

/* --- Verification plan item --- */
typedef struct hv_verify_plan_item {
    char            feature[256];
    char            description[512];
    bool            is_covered;
    bool            is_tested;
    uint32_t        priority;          /* 0 = highest, 255 = lowest */
    verify_result_t last_result;
} hv_verify_plan_item_t;

/* --- Verification Plan --- */
typedef struct hv_verify_plan {
    char                    name[128];
    hv_verify_plan_item_t  *items;
    size_t                  num_items;
    size_t                  capacity;
    float                   coverage_pct;   /* 0.0 - 100.0 */
} hv_verify_plan_t;

/* --- Configuration database key-value --- */
typedef struct hv_config_kv {
    char key[128];
    char value[256];
    struct hv_config_kv *next;
} hv_config_kv_t;

/* --- Configuration database --- */
typedef struct hv_config_db {
    hv_config_kv_t *head;
    size_t          count;
} hv_config_db_t;

/* ============================================================================
 * L2: Core Concepts — Verification Environment API
 * ============================================================================ */

/* --- DUT Management --- */
hv_dut_t       *hv_dut_create(const char *name);
void            hv_dut_destroy(hv_dut_t *dut);
hv_signal_t    *hv_dut_add_port(hv_dut_t *dut, const char *sig_name,
                                 port_dir_t dir, uint32_t width);
void            hv_dut_set_eval_cb(hv_dut_t *dut, void (*cb)(hv_dut_t*));

/* --- Signal operations --- */
void            hv_signal_drive(hv_signal_t *sig, uint32_t value);
uint32_t        hv_signal_read(const hv_signal_t *sig);
uint32_t        hv_signal_get_width(const hv_signal_t *sig);
const char     *hv_signal_get_name(const hv_signal_t *sig);
void            hv_signal_force(hv_signal_t *sig, uint32_t value, sim_time_t duration);
void            hv_signal_release(hv_signal_t *sig);

/* --- Verification Plan --- */
hv_verify_plan_t *hv_verify_plan_create(const char *name);
void              hv_verify_plan_destroy(hv_verify_plan_t *plan);
void              hv_verify_plan_add_item(hv_verify_plan_t *plan,
                      const char *feature, const char *desc, uint32_t priority);
void              hv_verify_plan_mark_tested(hv_verify_plan_t *plan,
                      size_t idx, verify_result_t result);
float             hv_verify_plan_calc_coverage(const hv_verify_plan_t *plan);
void              hv_verify_plan_report(const hv_verify_plan_t *plan, FILE *fp);

/* --- Config DB --- */
hv_config_db_t  *hv_config_db_create(void);
void             hv_config_db_destroy(hv_config_db_t *db);
void             hv_config_db_set(hv_config_db_t *db, const char *k, const char *v);
const char      *hv_config_db_get(const hv_config_db_t *db, const char *k,
                                  const char *default_val);

/* --- Utility --- */
const char      *verify_result_str(verify_result_t r);
const char      *severity_str(severity_t s);

#endif /* HW_VERIFY_H */
