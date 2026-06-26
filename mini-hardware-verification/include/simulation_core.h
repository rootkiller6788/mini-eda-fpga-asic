/**
 * simulation_core.h - Event-Driven Simulation Kernel
 *
 * Implements a discrete-time, event-driven simulation engine for RTL verification.
 * Based on IEEE 1364 (Verilog) and IEEE 1800 (SystemVerilog) simulation semantics:
 *   - Stratified event queue (Active, Inactive, NBA, Monitor, Future)
 *   - Delta-cycle semantics (zero-delay infinite loop detection)
 *   - VPI/DPI-alike foreign interface
 *   - Waveform dumping (VCD / FSDB-style)
 *   - Timing model (gate-level, RTL, mixed)
 *
 * L1: event types, simulation time, signal struct
 * L2: stratified event queue - the core Verilog simulation semantics
 * L3: event-driven simulation kernel architecture
 * L5: delta-cycle detection, timing wheel algorithm
 * L7: waveform dump (VCD), RISC-V core simulation
 *
 * Course: MIT 6.004 (Digital Systems Sim), CMU 18-240 (RTL Sim)
 * Ref: IEEE Std 1364-2001, Section 5 (Scheduling Semantics)
 */

#ifndef SIMULATION_CORE_H
#define SIMULATION_CORE_H

#include "hw_verify.h"
#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 * L1: Event Queue Stratification (IEEE 1364-2001 Sec 5.3)
 * ======================================================================== */

typedef enum {
    EVENT_ACTIVE         = 0,  /* blocking assignments, continuous assigns */
    EVENT_INACTIVE       = 1,  /* #0 delays */
    EVENT_NBA            = 2,  /* non-blocking assignments (<=) */
    EVENT_MONITOR        = 3,  /* $monitor, $strobe */
    EVENT_FUTURE         = 4,  /* future timed events */
    EVENT_POSTPONED      = 5,  /* post-simulation step */
} event_region_t;

/* ========================================================================
 * L1: Event structure
 * ======================================================================== */

typedef enum {
    EVT_SIGNAL_UPDATE = 0,
    EVT_EVAL_CB       = 1,   /* evaluation callback */
    EVT_ASSERTION      = 2,
    EVT_TIMEOUT        = 3,
    EVT_USER           = 4,
} event_type_t;

struct hv_signal;  /* forward declaration */

typedef struct hv_sim_event {
    event_type_t      type;
    event_region_t    region;
    sim_time_t        time;
    struct hv_signal *signal;
    uint32_t          new_value;
    void            (*callback)(void *ctx);
    void             *cb_ctx;
    struct hv_sim_event *next;
    struct hv_sim_event *prev;
} hv_sim_event_t;

/* ========================================================================
 * L1: Signal Implementation
 * ======================================================================== */

typedef struct hv_signal {
    char              name[64];
    port_dir_t        direction;
    uint32_t          width;         /* 1-64 bits */
    uint32_t          current_value;
    uint32_t          next_value;
    uint32_t          forced_value;
    bool              is_forced;
    bool              has_x;         /* unknown bits */
    bool              has_z;         /* high-Z bits */
    uint32_t          x_mask;        /* which bits are X */
    uint32_t          z_mask;        /* which bits are Z */
    /* event queues containing this signal */
    hv_sim_event_t   *pending_nba;
    hv_dut_t         *owner;
    /* waveform recording */
    struct hv_waveform_entry *waveform_head;
    struct hv_waveform_entry *waveform_tail;
    uint32_t          waveform_count;
} hv_signal_t;

/* ========================================================================
 * L3: Waveform Entry
 * ======================================================================== */

typedef struct hv_waveform_entry {
    sim_time_t  time;
    uint32_t    value;
    struct hv_waveform_entry *next;
} hv_waveform_entry_t;

/* ========================================================================
 * L2 & L3: Simulation Kernel
 * ======================================================================== */

#define MAX_DELTA_CYCLES 10000  /* safety limit */

typedef struct hv_sim_kernel {
    /* stratified event queues (doubly-linked lists) */
    hv_sim_event_t *queue_heads[6];  /* indexed by event_region_t */
    hv_sim_event_t *queue_tails[6];
    size_t          queue_sizes[6];
    /* current simulation state */
    sim_time_t      current_time;
    uint32_t        current_cycle;
    uint32_t        delta_count;     /* delta cycles in current timestep */
    bool            is_running;
    bool            is_finished;
    /* DUT instance */
    hv_dut_t       *dut;
    /* waveform recording */
    bool            record_waveform;
    char            waveform_file[256];
    FILE           *waveform_fp;
    /* statistics */
    uint64_t        total_events;
    uint64_t        total_delta_cycles;
    uint64_t        max_delta_in_timestep;
    sim_time_t      sim_start;
    sim_time_t      sim_end;
} hv_sim_kernel_t;

/* ========================================================================
 * L1 & L2: API - Signal Operations
 * ======================================================================== */

hv_signal_t *hv_signal_create(const char *name, port_dir_t dir, uint32_t width);
void         hv_signal_destroy(hv_signal_t *sig);
void         hv_signal_set(hv_signal_t *sig, uint32_t value);
uint32_t     hv_signal_get(const hv_signal_t *sig);
bool         hv_signal_is_x(const hv_signal_t *sig);
bool         hv_signal_is_z(const hv_signal_t *sig);
void         hv_signal_set_x(hv_signal_t *sig);
void         hv_signal_set_z(hv_signal_t *sig);
const char  *hv_signal_value_str(const hv_signal_t *sig, char *buf, size_t len);

/* ========================================================================
 * L2 & L3: API - Simulation Kernel
 * ======================================================================== */

hv_sim_kernel_t *hv_sim_kernel_create(void);
void             hv_sim_kernel_destroy(hv_sim_kernel_t *sim);
void             hv_sim_kernel_set_dut(hv_sim_kernel_t *sim, hv_dut_t *dut);

/* Schedule an event */
void hv_sim_schedule(hv_sim_kernel_t *sim, hv_sim_event_t *evt);
void hv_sim_schedule_signal_update(hv_sim_kernel_t *sim, hv_signal_t *sig,
                                    uint32_t value, sim_time_t delay,
                                    event_region_t region);
void hv_sim_schedule_callback(hv_sim_kernel_t *sim, void (*cb)(void*),
                               void *ctx, sim_time_t delay);

/* Run simulation */
void hv_sim_run(hv_sim_kernel_t *sim, sim_time_t duration);
void hv_sim_run_cycles(hv_sim_kernel_t *sim, uint32_t num_cycles);
void hv_sim_reset(hv_sim_kernel_t *sim);

/* Delta cycle processing (L5) */
bool hv_sim_process_delta(hv_sim_kernel_t *sim);

/* Waveform */
void hv_sim_enable_waveform(hv_sim_kernel_t *sim, const char *filename);
void hv_sim_disable_waveform(hv_sim_kernel_t *sim);
void hv_sim_dump_waveform_header(hv_sim_kernel_t *sim);
void hv_sim_dump_waveform_timestep(hv_sim_kernel_t *sim);

/* Report */
void hv_sim_report(const hv_sim_kernel_t *sim, FILE *fp);

/* ========================================================================
 * L5: Timing Wheel (for efficient future event management)
 *     Uses hierarchical timing wheels per Varghese & Lauck (1987)
 * ======================================================================== */

#define TIMING_WHEEL_SLOTS 256

typedef struct hv_timing_wheel {
    hv_sim_event_t *slots[TIMING_WHEEL_SLOTS];
    uint32_t        current_slot;
    sim_time_t      resolution;    /* time per slot */
    uint64_t        total_events_processed;
} hv_timing_wheel_t;

hv_timing_wheel_t *hv_timing_wheel_create(sim_time_t resolution);
void               hv_timing_wheel_destroy(hv_timing_wheel_t *tw);
void               hv_timing_wheel_insert(hv_timing_wheel_t *tw,
                                           hv_sim_event_t *evt);
hv_sim_event_t    *hv_timing_wheel_dequeue(hv_timing_wheel_t *tw);
void               hv_timing_wheel_advance(hv_timing_wheel_t *tw);

#endif /* SIMULATION_CORE_H */
