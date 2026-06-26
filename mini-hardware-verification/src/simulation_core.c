/**
 * simulation_core.c - Event-Driven Simulation Kernel Implementation
 *
 * Implements IEEE 1364-2001 stratified event queue simulation:
 *   - Event queues per region (Active, Inactive, NBA, Monitor, Future, Postponed)
 *   - Delta-cycle processing with infinite-loop detection
 *   - Signal operations (drive, read, force, release, X/Z propagation)
 *   - VCD-style waveform recording
 *   - Timing wheel for efficient future event scheduling
 *
 * L2: Verilog simulation semantics (stratified event queue)
 *     The stratification ensures deterministic behavior:
 *       1. Active events (blocking assignments, continuous assigns)
 *       2. Inactive events (#0 delays)
 *       3. NBA events (non-blocking assignments)
 *       4. Monitor events ($monitor, $strobe)
 *       5. Future events (next time step)
 *
 * L3: Event-driven simulation kernel architecture
 *     Core loop: while time < end_time:
 *       1. Process all events at current time
 *       2. For each delta cycle: process events by region
 *       3. Advance time to next future event
 *
 * L5: Delta-cycle detection & Timing wheel
 *     Delta-cycle: multiple rounds of event processing at the same
 *     simulation time (zero delay). If delta_count exceeds threshold,
 *     we have a combinational loop (infinite zero-delay oscillation).
 *
 *     Timing wheel: O(1) insert/dequeue for future events at known times.
 *     Varghese & Lauck, "Hashed and Hierarchical Timing Wheels" (1987).
 */

#include "simulation_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * Signal Implementation (L1, L2)
 * ======================================================================== */

hv_signal_t *hv_signal_create(const char *name, port_dir_t dir, uint32_t width) {
    hv_signal_t *sig = (hv_signal_t*)calloc(1, sizeof(hv_signal_t));
    if (!sig) return NULL;
    if (name) strncpy(sig->name, name, sizeof(sig->name) - 1);
    sig->direction = dir;
    sig->width = (width > 0 && width <= 64) ? width : 1;
    sig->current_value = 0;
    sig->next_value = 0;
    sig->forced_value = 0;
    sig->is_forced = false;
    sig->has_x = false;
    sig->has_z = false;
    sig->x_mask = 0;
    sig->z_mask = 0;
    sig->pending_nba = NULL;
    sig->owner = NULL;
    sig->waveform_head = NULL;
    sig->waveform_tail = NULL;
    sig->waveform_count = 0;
    return sig;
}

void hv_signal_destroy(hv_signal_t *sig) {
    if (!sig) return;
    /* Free waveform entries */
    hv_waveform_entry_t *we = sig->waveform_head;
    while (we) {
        hv_waveform_entry_t *next = we->next;
        free(we);
        we = next;
    }
    free(sig);
}

void hv_signal_set(hv_signal_t *sig, uint32_t value) {
    if (!sig || sig->is_forced) return;
    uint32_t mask = (sig->width == 32) ? 0xFFFFFFFF :
                    ((1ULL << sig->width) - 1);
    sig->current_value = value & mask;
}

uint32_t hv_signal_get(const hv_signal_t *sig) {
    if (!sig) return 0;
    return sig->is_forced ? sig->forced_value : sig->current_value;
}

bool hv_signal_is_x(const hv_signal_t *sig) {
    return sig ? sig->has_x : false;
}

bool hv_signal_is_z(const hv_signal_t *sig) {
    return sig ? sig->has_z : false;
}

void hv_signal_set_x(hv_signal_t *sig) {
    if (!sig) return;
    sig->has_x = true;
    sig->x_mask = (sig->width == 32) ? 0xFFFFFFFF : ((1ULL << sig->width) - 1);
    sig->current_value = 0;
}

void hv_signal_set_z(hv_signal_t *sig) {
    if (!sig) return;
    sig->has_z = true;
    sig->z_mask = (sig->width == 32) ? 0xFFFFFFFF : ((1ULL << sig->width) - 1);
    sig->current_value = 0;
}

const char *hv_signal_value_str(const hv_signal_t *sig, char *buf, size_t len) {
    if (!sig || !buf || len == 0) return "";
    if (sig->width == 1) {
        if (sig->has_x) snprintf(buf, len, "x");
        else if (sig->has_z) snprintf(buf, len, "z");
        else snprintf(buf, len, "%u", sig->current_value ? 1 : 0);
    } else {
        /* Multi-bit: print hex if large, binary if small */
        if (sig->width <= 16) {
            uint32_t mask = (1U << sig->width) - 1;
            snprintf(buf, len, "%u'b", sig->width);
            for (int i = (int)sig->width - 1; i >= 0; i--) {
                if (sig->has_x && (sig->x_mask & (1U << i))) {
                    strncat(buf, "x", len - strlen(buf) - 1);
                } else if (sig->has_z && (sig->z_mask & (1U << i))) {
                    strncat(buf, "z", len - strlen(buf) - 1);
                } else {
                    char bit[2] = { (sig->current_value & (1U << i)) ? '1' : '0', 0 };
                    strncat(buf, bit, len - strlen(buf) - 1);
                }
            }
        } else {
            snprintf(buf, len, "0x%x", sig->current_value);
        }
    }
    return buf;
}

/* ========================================================================
 * Event Creation & Scheduling (L2)
 * ======================================================================== */

static hv_sim_event_t *sim_event_create(event_type_t type, event_region_t region,
                                         sim_time_t time) {
    hv_sim_event_t *evt = (hv_sim_event_t*)calloc(1, sizeof(hv_sim_event_t));
    if (!evt) return NULL;
    evt->type = type;
    evt->region = region;
    evt->time = time;
    return evt;
}

static void sim_event_destroy(hv_sim_event_t *evt) {
    free(evt);
}

/* ========================================================================
 * Simulation Kernel (L2, L3)
 * ======================================================================== */

hv_sim_kernel_t *hv_sim_kernel_create(void) {
    hv_sim_kernel_t *sim = (hv_sim_kernel_t*)calloc(1, sizeof(hv_sim_kernel_t));
    if (!sim) return NULL;
    memset(sim->queue_heads, 0, sizeof(sim->queue_heads));
    memset(sim->queue_tails, 0, sizeof(sim->queue_tails));
    memset(sim->queue_sizes, 0, sizeof(sim->queue_sizes));
    sim->current_time = 0;
    sim->current_cycle = 0;
    sim->delta_count = 0;
    sim->is_running = false;
    sim->is_finished = false;
    sim->dut = NULL;
    sim->record_waveform = false;
    sim->waveform_fp = NULL;
    sim->total_events = 0;
    sim->total_delta_cycles = 0;
    sim->max_delta_in_timestep = 0;
    sim->sim_start = 0;
    sim->sim_end = 0;
    return sim;
}

void hv_sim_kernel_destroy(hv_sim_kernel_t *sim) {
    if (!sim) return;
    /* Free all queued events in all regions */
    for (int r = 0; r < 6; r++) {
        hv_sim_event_t *evt = sim->queue_heads[r];
        while (evt) {
            hv_sim_event_t *next = evt->next;
            sim_event_destroy(evt);
            evt = next;
        }
    }
    if (sim->waveform_fp) fclose(sim->waveform_fp);
    free(sim);
}

void hv_sim_kernel_set_dut(hv_sim_kernel_t *sim, hv_dut_t *dut) {
    if (sim) sim->dut = dut;
}

/* Low-level: enqueue event into specified region queue */
static void sim_enqueue_event(hv_sim_kernel_t *sim, hv_sim_event_t *evt) {
    event_region_t r = evt->region;
    evt->next = NULL;
    evt->prev = sim->queue_tails[r];
    if (sim->queue_tails[r]) {
        sim->queue_tails[r]->next = evt;
    } else {
        sim->queue_heads[r] = evt;
    }
    sim->queue_tails[r] = evt;
    sim->queue_sizes[r]++;
    sim->total_events++;
}

/* High-level: schedule a signal update */
void hv_sim_schedule(hv_sim_kernel_t *sim, hv_sim_event_t *evt) {
    if (!sim || !evt) return;
    sim_enqueue_event(sim, evt);
}

void hv_sim_schedule_signal_update(hv_sim_kernel_t *sim, hv_signal_t *sig,
                                    uint32_t value, sim_time_t delay,
                                    event_region_t region) {
    if (!sim || !sig) return;
    hv_sim_event_t *evt = sim_event_create(EVT_SIGNAL_UPDATE, region,
                                            sim->current_time + delay);
    if (!evt) return;
    evt->signal = sig;
    evt->new_value = value;
    sim_enqueue_event(sim, evt);
}

void hv_sim_schedule_callback(hv_sim_kernel_t *sim, void (*cb)(void*),
                               void *ctx, sim_time_t delay) {
    if (!sim || !cb) return;
    hv_sim_event_t *evt = sim_event_create(EVT_EVAL_CB, EVENT_ACTIVE,
                                            sim->current_time + delay);
    if (!evt) return;
    evt->callback = cb;
    evt->cb_ctx = ctx;
    sim_enqueue_event(sim, evt);
}

/* Process a single event */
static void sim_process_event(hv_sim_kernel_t *sim, hv_sim_event_t *evt) {
    if (!sim || !evt) return;

    switch (evt->type) {
        case EVT_SIGNAL_UPDATE:
            if (evt->signal) {
                hv_signal_set(evt->signal, evt->new_value);
                /* Record waveform if enabled */
                if (sim->record_waveform && sim->waveform_fp) {
                    hv_waveform_entry_t *we = (hv_waveform_entry_t*)calloc(
                        1, sizeof(hv_waveform_entry_t));
                    if (we) {
                        we->time = sim->current_time;
                        we->value = evt->new_value;
                        if (evt->signal->waveform_tail) {
                            evt->signal->waveform_tail->next = we;
                        } else {
                            evt->signal->waveform_head = we;
                        }
                        evt->signal->waveform_tail = we;
                        evt->signal->waveform_count++;
                    }
                }
            }
            break;
        case EVT_EVAL_CB:
            if (evt->callback) {
                evt->callback(evt->cb_ctx);
            }
            break;
        case EVT_ASSERTION:
        case EVT_TIMEOUT:
        case EVT_USER:
            if (evt->callback) {
                evt->callback(evt->cb_ctx);
            }
            break;
    }
}

/* Process all events in a specific region queue */
static size_t sim_process_region(hv_sim_kernel_t *sim, event_region_t region) {
    size_t processed = 0;
    hv_sim_event_t *evt = sim->queue_heads[region];
    while (evt) {
        hv_sim_event_t *next = evt->next;
        sim_process_event(sim, evt);
        sim_event_destroy(evt);
        processed++;
        evt = next;
    }
    sim->queue_heads[region] = NULL;
    sim->queue_tails[region] = NULL;
    sim->queue_sizes[region] = 0;
    return processed;
}

/* Delta-cycle processing (L5)
 *
 * Process all event regions in order at the current simulation time.
 * Active -> Inactive -> NBA -> Monitor -> Postponed
 * If new events are generated in any region, we loop (delta cycle).
 * Detects infinite delta-cycle loops (combinational feedback).
 *
 * Returns true if simulation should continue, false on delta-cycle overflow.
 */
bool hv_sim_process_delta(hv_sim_kernel_t *sim) {
    if (!sim) return false;

    bool events_remaining;
    sim->delta_count = 0;

    do {
        events_remaining = false;

        /* 1. Active region */
        if (sim->queue_sizes[EVENT_ACTIVE] > 0) {
            sim_process_region(sim, EVENT_ACTIVE);
            events_remaining = true;
        }

        /* 2. Inactive region */
        if (sim->queue_sizes[EVENT_INACTIVE] > 0) {
            sim_process_region(sim, EVENT_INACTIVE);
            events_remaining = true;
        }

        /* 3. NBA region */
        if (sim->queue_sizes[EVENT_NBA] > 0) {
            /* Move NBA events to Active for next delta */
            hv_sim_event_t *nba_head = sim->queue_heads[EVENT_NBA];
            hv_sim_event_t *nba_tail = sim->queue_tails[EVENT_NBA];
            sim->queue_heads[EVENT_NBA] = NULL;
            sim->queue_tails[EVENT_NBA] = NULL;
            sim->queue_sizes[EVENT_NBA] = 0;

            /* Append to Active queue */
            if (sim->queue_tails[EVENT_ACTIVE]) {
                sim->queue_tails[EVENT_ACTIVE]->next = nba_head;
                if (nba_head) nba_head->prev = sim->queue_tails[EVENT_ACTIVE];
            } else {
                sim->queue_heads[EVENT_ACTIVE] = nba_head;
            }
            if (nba_tail) sim->queue_tails[EVENT_ACTIVE] = nba_tail;
            events_remaining = true;
        }

        /* 4. Monitor region */
        if (sim->queue_sizes[EVENT_MONITOR] > 0) {
            sim_process_region(sim, EVENT_MONITOR);
        }

        /* 5. Postponed */
        if (sim->queue_sizes[EVENT_POSTPONED] > 0) {
            sim_process_region(sim, EVENT_POSTPONED);
        }

        /* If DUT has eval callback, trigger it */
        if (sim->dut && sim->dut->eval_cb) {
            sim->dut->eval_cb(sim->dut);
        }

        sim->delta_count++;
        sim->total_delta_cycles++;

        /* Safety check: delta-cycle overflow = combinational loop */
        if (sim->delta_count > MAX_DELTA_CYCLES) {
            fprintf(stderr, "ERROR: Delta cycle limit (%u) exceeded at time %lu. "
                    "Likely combinational feedback loop.\n",
                    MAX_DELTA_CYCLES, (unsigned long)sim->current_time);
            return false;
        }

    } while (events_remaining);

    /* Track max delta cycles per timestep */
    if (sim->delta_count > sim->max_delta_in_timestep) {
        sim->max_delta_in_timestep = sim->delta_count;
    }

    return true;
}

/* Main simulation run loop */
void hv_sim_run(hv_sim_kernel_t *sim, sim_time_t duration) {
    if (!sim) return;

    sim->is_running = true;
    sim->is_finished = false;
    sim->sim_start = sim->current_time;
    sim->sim_end = sim->current_time + duration;

    while (sim->current_time < sim->sim_end && sim->is_running) {
        /* Check if any events at current time or in future */
        bool has_events = false;
        for (int r = 0; r < 5; r++) { /* all except future */
            if (sim->queue_sizes[r] > 0) { has_events = true; break; }
        }

        if (!has_events && sim->queue_sizes[EVENT_FUTURE] == 0) {
            /* No events at all - simulation complete */
            break;
        }

        /* Find next event time */
        sim_time_t next_time = UINT64_MAX;
        for (int r = 0; r < 6; r++) {
            hv_sim_event_t *evt = sim->queue_heads[r];
            while (evt) {
                if (evt->time < next_time && evt->time >= sim->current_time) {
                    next_time = evt->time;
                }
                evt = evt->next;
            }
        }

        /* Move events from FUTURE to ACTIVE if their time has come */
        while (sim->queue_heads[EVENT_FUTURE] &&
               sim->queue_heads[EVENT_FUTURE]->time <= sim->current_time) {
            hv_sim_event_t *evt = sim->queue_heads[EVENT_FUTURE];
            sim->queue_heads[EVENT_FUTURE] = evt->next;
            if (sim->queue_tails[EVENT_FUTURE] == evt) {
                sim->queue_tails[EVENT_FUTURE] = NULL;
            }
            sim->queue_sizes[EVENT_FUTURE]--;
            evt->region = EVENT_ACTIVE;
            evt->next = NULL;
            evt->prev = NULL;
            sim_enqueue_event(sim, evt);
        }

        /* Process delta cycles */
        if (!hv_sim_process_delta(sim)) {
            /* Delta overflow - abort */
            sim->is_running = false;
            break;
        }

        /* Dump waveform for this timestep */
        if (sim->record_waveform) {
            hv_sim_dump_waveform_timestep(sim);
        }

        /* Advance time */
        sim->current_cycle++;
        if (sim->current_time < sim->sim_end) {
            sim->current_time++;
        } else {
            break;
        }
    }

    sim->is_finished = true;
    sim->is_running = false;
}

void hv_sim_run_cycles(hv_sim_kernel_t *sim, uint32_t num_cycles) {
    if (!sim) return;
    hv_sim_run(sim, num_cycles);
}

void hv_sim_reset(hv_sim_kernel_t *sim) {
    if (!sim) return;
    /* Clear all event queues */
    for (int r = 0; r < 6; r++) {
        hv_sim_event_t *evt = sim->queue_heads[r];
        while (evt) {
            hv_sim_event_t *next = evt->next;
            sim_event_destroy(evt);
            evt = next;
        }
        sim->queue_heads[r] = NULL;
        sim->queue_tails[r] = NULL;
        sim->queue_sizes[r] = 0;
    }
    sim->current_time = 0;
    sim->current_cycle = 0;
    sim->delta_count = 0;
    sim->is_running = false;
    sim->is_finished = false;
}

/* ========================================================================
 * Waveform Recording (L7) - VCD-style
 * ======================================================================== */

void hv_sim_enable_waveform(hv_sim_kernel_t *sim, const char *filename) {
    if (!sim) return;
    sim->record_waveform = true;
    if (filename) strncpy(sim->waveform_file, filename, sizeof(sim->waveform_file) - 1);
}

void hv_sim_disable_waveform(hv_sim_kernel_t *sim) {
    if (!sim) return;
    sim->record_waveform = false;
    if (sim->waveform_fp) {
        fclose(sim->waveform_fp);
        sim->waveform_fp = NULL;
    }
}

/* Write VCD header */
void hv_sim_dump_waveform_header(hv_sim_kernel_t *sim) {
    if (!sim || !sim->record_waveform) return;

    if (!sim->waveform_fp && sim->waveform_file[0]) {
        sim->waveform_fp = fopen(sim->waveform_file, "w");
    }
    if (!sim->waveform_fp) {
        sim->waveform_fp = stdout;
    }

    FILE *fp = sim->waveform_fp;
    fprintf(fp, "$date\n  Mini-Hardware-Verification Waveform\n$end\n");
    fprintf(fp, "$version\n  hv_sim_kernel v1.0\n$end\n");
    fprintf(fp, "$timescale 1ns $end\n");

    /* Dump DUT signals */
    if (sim->dut) {
        for (uint32_t i = 0; i < sim->dut->num_signals; i++) {
            hv_signal_t *sig = sim->dut->signals[i];
            fprintf(fp, "$var wire %u %c %s $end\n",
                    sig->width, (char)('a' + i), sig->name);
        }
    }
    fprintf(fp, "$enddefinitions $end\n");
}

/* Dump one timestep of values */
void hv_sim_dump_waveform_timestep(hv_sim_kernel_t *sim) {
    if (!sim || !sim->waveform_fp || !sim->dut) return;

    FILE *fp = sim->waveform_fp;
    fprintf(fp, "#%lu\n", (unsigned long)sim->current_time);

    for (uint32_t i = 0; i < sim->dut->num_signals; i++) {
        hv_signal_t *sig = sim->dut->signals[i];
        char val_str[128];
        hv_signal_value_str(sig, val_str, sizeof(val_str));
        fprintf(fp, "%s%c\n", val_str, (char)('a' + i));
    }
}

/* ========================================================================
 * Simulation Report (L2)
 * ======================================================================== */

void hv_sim_report(const hv_sim_kernel_t *sim, FILE *fp) {
    if (!sim || !fp) return;
    fprintf(fp, "=== Simulation Kernel Report ===\n");
    fprintf(fp, "  Status: %s\n", sim->is_finished ? "Finished" :
            sim->is_running ? "Running" : "Stopped");
    fprintf(fp, "  Time: %lu / %lu\n",
            (unsigned long)sim->current_time, (unsigned long)sim->sim_end);
    fprintf(fp, "  Cycle: %u\n", sim->current_cycle);
    fprintf(fp, "  Total events: %lu\n", (unsigned long)sim->total_events);
    fprintf(fp, "  Total delta cycles: %lu\n", (unsigned long)sim->total_delta_cycles);
    fprintf(fp, "  Max delta in timestep: %lu\n",
            (unsigned long)sim->max_delta_in_timestep);
    fprintf(fp, "  Event queue sizes:\n");
    const char *region_names[] = {"Active", "Inactive", "NBA", "Monitor", "Future", "Postponed"};
    for (int r = 0; r < 6; r++) {
        fprintf(fp, "    %s: %zu\n", region_names[r], sim->queue_sizes[r]);
    }
    if (sim->dut) {
        fprintf(fp, "  DUT: %s (%u signals)\n",
                sim->dut->name, sim->dut->num_signals);
    }
}

/* ========================================================================
 * Timing Wheel (L5)
 * ======================================================================== */

hv_timing_wheel_t *hv_timing_wheel_create(sim_time_t resolution) {
    hv_timing_wheel_t *tw = (hv_timing_wheel_t*)calloc(1, sizeof(hv_timing_wheel_t));
    if (!tw) return NULL;
    tw->resolution = (resolution > 0) ? resolution : 1;
    tw->current_slot = 0;
    tw->total_events_processed = 0;
    memset(tw->slots, 0, sizeof(tw->slots));
    return tw;
}

void hv_timing_wheel_destroy(hv_timing_wheel_t *tw) {
    if (!tw) return;
    for (uint32_t i = 0; i < TIMING_WHEEL_SLOTS; i++) {
        hv_sim_event_t *evt = tw->slots[i];
        while (evt) {
            hv_sim_event_t *next = evt->next;
            sim_event_destroy(evt);
            evt = next;
        }
    }
    free(tw);
}

void hv_timing_wheel_insert(hv_timing_wheel_t *tw, hv_sim_event_t *evt) {
    if (!tw || !evt) return;

    /* Calculate slot index from event time */
    uint32_t slot = (uint32_t)((evt->time / tw->resolution +
                                 tw->current_slot) % TIMING_WHEEL_SLOTS);

    /* Insert at head of slot's linked list */
    evt->next = tw->slots[slot];
    evt->prev = NULL;
    if (tw->slots[slot]) tw->slots[slot]->prev = evt;
    tw->slots[slot] = evt;
}

hv_sim_event_t *hv_timing_wheel_dequeue(hv_timing_wheel_t *tw) {
    if (!tw) return NULL;

    /* Get all events from current slot */
    hv_sim_event_t *head = tw->slots[tw->current_slot];
    tw->slots[tw->current_slot] = NULL;
    tw->total_events_processed++;

    return head;
}

void hv_timing_wheel_advance(hv_timing_wheel_t *tw) {
    if (!tw) return;
    tw->current_slot = (tw->current_slot + 1) % TIMING_WHEEL_SLOTS;
}
