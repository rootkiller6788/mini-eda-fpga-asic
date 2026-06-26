/**
 * hw_verify.c - Core Verification Framework Implementation
 *
 * Implements DUT management, verification plan, config database,
 * and utility functions for the hardware verification framework.
 */

#include "hw_verify.h"
#include "simulation_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ========================================================================
 * Verification Result & Severity String Conversion (L1)
 * ======================================================================== */

const char *verify_result_str(verify_result_t r) {
    switch (r) {
        case VERIFY_PASS:    return "PASS";
        case VERIFY_FAIL:    return "FAIL";
        case VERIFY_TIMEOUT: return "TIMEOUT";
        case VERIFY_ERROR:   return "ERROR";
        case VERIFY_NOT_RUN: return "NOT_RUN";
        default:             return "UNKNOWN";
    }
}

const char *severity_str(severity_t s) {
    switch (s) {
        case SEV_INFO:    return "INFO";
        case SEV_WARNING: return "WARNING";
        case SEV_ERROR:   return "ERROR";
        case SEV_FATAL:   return "FATAL";
        default:          return "UNKNOWN";
    }
}

/* ========================================================================
 * DUT Management (L1, L2)
 * ======================================================================== */

hv_dut_t *hv_dut_create(const char *name) {
    hv_dut_t *dut = (hv_dut_t*)calloc(1, sizeof(hv_dut_t));
    if (!dut) return NULL;
    strncpy(dut->name, name, sizeof(dut->name) - 1);
    dut->num_inputs = 0;
    dut->num_outputs = 0;
    dut->num_internals = 0;
    dut->num_signals = 0;
    dut->signals = NULL;
    dut->eval_cb = NULL;
    dut->user_data = NULL;
    return dut;
}

void hv_dut_destroy(hv_dut_t *dut) {
    if (!dut) return;
    for (uint32_t i = 0; i < dut->num_signals; i++) {
        free(dut->signals[i]);
    }
    free(dut->signals);
    free(dut);
}

hv_signal_t *hv_dut_add_port(hv_dut_t *dut, const char *sig_name,
                              port_dir_t dir, uint32_t width) {
    if (!dut || !sig_name) return NULL;
    /* increase signals array */
    hv_signal_t **new_signals = (hv_signal_t**)realloc(
        dut->signals, (dut->num_signals + 1) * sizeof(hv_signal_t*));
    if (!new_signals) return NULL;
    dut->signals = new_signals;

    hv_signal_t *sig = (hv_signal_t*)calloc(1, sizeof(hv_signal_t));
    if (!sig) return NULL;
    strncpy(sig->name, sig_name, sizeof(sig->name) - 1);
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
    sig->owner = dut;

    dut->signals[dut->num_signals] = sig;
    dut->num_signals++;

    switch (dir) {
        case PORT_INPUT:    dut->num_inputs++;    break;
        case PORT_OUTPUT:   dut->num_outputs++;   break;
        case PORT_INTERNAL: dut->num_internals++; break;
        case PORT_INOUT:    dut->num_inputs++; dut->num_outputs++; break;
    }
    return sig;
}

void hv_dut_set_eval_cb(hv_dut_t *dut, void (*cb)(hv_dut_t*)) {
    if (dut) dut->eval_cb = cb;
}

/* ========================================================================
 * Signal Operations (L1, L2)
 * ======================================================================== */

void hv_signal_drive(hv_signal_t *sig, uint32_t value) {
    if (!sig) return;
    /* masking: only bits within width are valid */
    uint32_t mask = (sig->width == 32) ? 0xFFFFFFFF :
                    ((1ULL << sig->width) - 1);
    sig->current_value = value & mask;
    sig->has_x = false;
    sig->has_z = false;
    sig->x_mask = 0;
    sig->z_mask = 0;
}

uint32_t hv_signal_read(const hv_signal_t *sig) {
    if (!sig) return 0;
    return sig->current_value;
}

uint32_t hv_signal_get_width(const hv_signal_t *sig) {
    return sig ? sig->width : 0;
}

const char *hv_signal_get_name(const hv_signal_t *sig) {
    return sig ? sig->name : NULL;
}

void hv_signal_force(hv_signal_t *sig, uint32_t value, sim_time_t duration) {
    if (!sig) return;
    sig->forced_value = value;
    sig->is_forced = true;
    sig->current_value = value;
    (void)duration; /* For future timed-force support */
}

void hv_signal_release(hv_signal_t *sig) {
    if (!sig) return;
    sig->is_forced = false;
}

/* ========================================================================
 * Verification Plan Management (L2)
 * ======================================================================== */

hv_verify_plan_t *hv_verify_plan_create(const char *name) {
    hv_verify_plan_t *plan = (hv_verify_plan_t*)calloc(1, sizeof(hv_verify_plan_t));
    if (!plan) return NULL;
    strncpy(plan->name, name, sizeof(plan->name) - 1);
    plan->items = NULL;
    plan->num_items = 0;
    plan->capacity = 0;
    plan->coverage_pct = 0.0f;
    return plan;
}

void hv_verify_plan_destroy(hv_verify_plan_t *plan) {
    if (!plan) return;
    free(plan->items);
    free(plan);
}

void hv_verify_plan_add_item(hv_verify_plan_t *plan,
                              const char *feature, const char *desc,
                              uint32_t priority) {
    if (!plan || !feature || !desc) return;
    if (plan->num_items >= plan->capacity) {
        size_t new_cap = (plan->capacity == 0) ? 16 : plan->capacity * 2;
        hv_verify_plan_item_t *new_items = (hv_verify_plan_item_t*)realloc(
            plan->items, new_cap * sizeof(hv_verify_plan_item_t));
        if (!new_items) return;
        plan->items = new_items;
        plan->capacity = new_cap;
    }
    hv_verify_plan_item_t *item = &plan->items[plan->num_items++];
    memset(item, 0, sizeof(*item));
    strncpy(item->feature, feature, sizeof(item->feature) - 1);
    strncpy(item->description, desc, sizeof(item->description) - 1);
    item->priority = priority;
    item->is_covered = false;
    item->is_tested = false;
    item->last_result = VERIFY_NOT_RUN;
}

void hv_verify_plan_mark_tested(hv_verify_plan_t *plan, size_t idx,
                                 verify_result_t result) {
    if (!plan || idx >= plan->num_items) return;
    plan->items[idx].is_tested = true;
    plan->items[idx].last_result = result;
    if (result == VERIFY_PASS) {
        plan->items[idx].is_covered = true;
    }
}

float hv_verify_plan_calc_coverage(const hv_verify_plan_t *plan) {
    if (!plan || plan->num_items == 0) return 0.0f;
    size_t covered = 0;
    for (size_t i = 0; i < plan->num_items; i++) {
        if (plan->items[i].is_covered) covered++;
    }
    return ((float)covered / (float)plan->num_items) * 100.0f;
}

void hv_verify_plan_report(const hv_verify_plan_t *plan, FILE *fp) {
    if (!plan || !fp) return;
    fprintf(fp, "=== Verification Plan: %s ===\n", plan->name);
    fprintf(fp, "Total items: %zu\n", plan->num_items);
    fprintf(fp, "Coverage: %.1f%%\n", hv_verify_plan_calc_coverage(plan));
    fprintf(fp, "%-40s %-10s %-10s %s\n",
            "Feature", "Priority", "Result", "Covered");
    fprintf(fp, "------------------------------------------------------------\n");
    for (size_t i = 0; i < plan->num_items; i++) {
        hv_verify_plan_item_t *it = &plan->items[i];
        fprintf(fp, "%-40.40s %-10u %-10s %s\n",
                it->feature, it->priority,
                verify_result_str(it->last_result),
                it->is_covered ? "YES" : "NO");
    }
}

/* ========================================================================
 * Configuration Database (L2)
 * ======================================================================== */

hv_config_db_t *hv_config_db_create(void) {
    hv_config_db_t *db = (hv_config_db_t*)calloc(1, sizeof(hv_config_db_t));
    return db;
}

void hv_config_db_destroy(hv_config_db_t *db) {
    if (!db) return;
    hv_config_kv_t *kv = db->head;
    while (kv) {
        hv_config_kv_t *next = kv->next;
        free(kv);
        kv = next;
    }
    free(db);
}

void hv_config_db_set(hv_config_db_t *db, const char *k, const char *v) {
    if (!db || !k || !v) return;
    /* search existing */
    hv_config_kv_t *kv = db->head;
    while (kv) {
        if (strcmp(kv->key, k) == 0) {
            strncpy(kv->value, v, sizeof(kv->value) - 1);
            return;
        }
        kv = kv->next;
    }
    /* create new */
    kv = (hv_config_kv_t*)calloc(1, sizeof(hv_config_kv_t));
    if (!kv) return;
    strncpy(kv->key, k, sizeof(kv->key) - 1);
    strncpy(kv->value, v, sizeof(kv->value) - 1);
    kv->next = db->head;
    db->head = kv;
    db->count++;
}

const char *hv_config_db_get(const hv_config_db_t *db, const char *k,
                              const char *default_val) {
    if (!db || !k) return default_val;
    hv_config_kv_t *kv = db->head;
    while (kv) {
        if (strcmp(kv->key, k) == 0) return kv->value;
        kv = kv->next;
    }
    return default_val;
}
