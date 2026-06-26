/**
 * uvm_components.c - UVM Component Implementation
 *
 * Implements the UVM-alike verification component hierarchy:
 * TLM connections, Monitor/Driver/Sequencer/Scoreboard,
 * Agent/Env, sequence generation and arbitration.
 */

#include "uvm_components.h"
#include "simulation_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * TLM Port / Export / Imp (L2, L3)
 * ======================================================================== */

hv_tlm_port_t *hv_tlm_port_create(const char *name, void *owner) {
    hv_tlm_port_t *port = (hv_tlm_port_t*)calloc(1, sizeof(hv_tlm_port_t));
    if (!port) return NULL;
    strncpy(port->name, name, sizeof(port->name) - 1);
    port->owner = owner;
    port->connected_export = NULL;
    port->write = NULL;
    port->read = NULL;
    return port;
}

void hv_tlm_port_destroy(hv_tlm_port_t *port) {
    free(port);
}

hv_tlm_export_t *hv_tlm_export_create(const char *name, void *owner) {
    hv_tlm_export_t *exp = (hv_tlm_export_t*)calloc(1, sizeof(hv_tlm_export_t));
    if (!exp) return NULL;
    strncpy(exp->name, name, sizeof(exp->name) - 1);
    exp->owner = owner;
    exp->connected_imp = NULL;
    exp->bound_port = NULL;
    return exp;
}

void hv_tlm_export_destroy(hv_tlm_export_t *export_) {
    free(export_);
}

hv_tlm_imp_t *hv_tlm_imp_create(const char *name, void *owner,
                                 void (*write_cb)(hv_tlm_imp_t*, hv_transaction_t*)) {
    hv_tlm_imp_t *imp = (hv_tlm_imp_t*)calloc(1, sizeof(hv_tlm_imp_t));
    if (!imp) return NULL;
    strncpy(imp->name, name, sizeof(imp->name) - 1);
    imp->owner = owner;
    imp->write_impl = write_cb;
    imp->read_impl = NULL;
    return imp;
}

void hv_tlm_imp_destroy(hv_tlm_imp_t *imp) {
    free(imp);
}

void hv_tlm_connect(hv_tlm_port_t *port, hv_tlm_export_t *export_) {
    if (!port || !export_) return;
    port->connected_export = export_;
    export_->bound_port = port;
    /* Set trampoline functions */
    if (export_->connected_imp) {
        port->write = (void (*)(hv_tlm_port_t*, hv_transaction_t*))
            export_->connected_imp->write_impl;
    }
}

void hv_tlm_bind(hv_tlm_export_t *export_, hv_tlm_imp_t *imp) {
    if (!export_ || !imp) return;
    export_->connected_imp = imp;
    /* Update bound port if already connected */
    if (export_->bound_port && imp->write_impl) {
        export_->bound_port->write = (void (*)(hv_tlm_port_t*, hv_transaction_t*))
            imp->write_impl;
    }
}

/* ========================================================================
 * Monitor (L2)
 * ======================================================================== */

static void monitor_collect_default(hv_monitor_t *m) {
    /* Default collector: reads DUT signals to build a transaction */
    if (!m || !m->dut) return;
    /* This is a template - real implementations override collect_transaction */
}

hv_monitor_t *hv_monitor_create(const char *name, hv_dut_t *dut) {
    hv_monitor_t *m = (hv_monitor_t*)calloc(1, sizeof(hv_monitor_t));
    if (!m) return NULL;
    strncpy(m->base.name, name, sizeof(m->base.name) - 1);
    m->base.impl = m;
    m->base.build_phase = NULL;
    m->base.connect_phase = NULL;
    m->base.run_phase = hv_monitor_run_phase;
    m->base.report_phase = NULL;
    m->dut = dut;
    m->analysis_port = hv_tlm_port_create(name, m);
    m->pending_items = NULL;
    m->pending_count = 0;
    m->watch_read = true;
    m->watch_write = true;
    m->watch_idle = false;
    m->tx_seen = 0;
    m->tx_error = 0;
    m->collect_transaction = monitor_collect_default;
    return m;
}

void hv_monitor_destroy(hv_monitor_t *m) {
    if (!m) return;
    hv_tlm_port_destroy(m->analysis_port);
    /* Free pending items */
    hv_sequence_item_t *item = m->pending_items;
    while (item) {
        hv_sequence_item_t *next = item->next;
        hv_sequence_item_destroy(item);
        item = next;
    }
    free(m);
}

void hv_monitor_run_phase(hv_component_t *comp) {
    hv_monitor_t *m = (hv_monitor_t*)comp->impl;
    if (!m || !m->collect_transaction) return;
    m->collect_transaction(m);
}

/* ========================================================================
 * Driver (L2)
 * ======================================================================== */

static void driver_seq_item_write(hv_tlm_imp_t *imp, hv_transaction_t *tx) {
    hv_driver_t *drv = (hv_driver_t*)imp->owner;
    if (!drv || !tx) return;
    if (drv->drive_transaction) {
        drv->drive_transaction(drv, tx);
    }
    drv->items_driven++;
}

hv_driver_t *hv_driver_create(const char *name, hv_dut_t *dut) {
    hv_driver_t *drv = (hv_driver_t*)calloc(1, sizeof(hv_driver_t));
    if (!drv) return NULL;
    strncpy(drv->base.name, name, sizeof(drv->base.name) - 1);
    drv->base.impl = drv;
    drv->base.run_phase = hv_driver_run_phase;
    drv->dut = dut;
    drv->seq_item_port = hv_tlm_imp_create(name, drv, driver_seq_item_write);
    drv->clk = NULL;
    drv->rst_n = NULL;
    drv->current_item = NULL;
    drv->is_busy = false;
    drv->items_driven = 0;
    drv->cycles_driven = 0;
    drv->drive_transaction = NULL;
    return drv;
}

void hv_driver_destroy(hv_driver_t *drv) {
    if (!drv) return;
    hv_tlm_imp_destroy(drv->seq_item_port);
    free(drv);
}

void hv_driver_run_phase(hv_component_t *comp) {
    /* Driver run phase: check if idle and fetch next item */
    hv_driver_t *drv = (hv_driver_t*)comp->impl;
    if (!drv) return;
    drv->cycles_driven++;
}

/* ========================================================================
 * Sequencer (L2, L5: Round-Robin Arbitration)
 * ======================================================================== */

/* Default round-robin arbitration across active sequences.
 * Complexity: O(num_active), space O(1).
 * Ensures fairness: each active sequence gets a turn.
 */
hv_sequence_item_t *hv_sequencer_rr_arbitrate(hv_sequencer_t *sqr) {
    if (!sqr || sqr->num_active == 0) return NULL;

    /* Try each sequence starting from rr_index */
    for (size_t attempt = 0; attempt < sqr->num_active; attempt++) {
        size_t idx = (sqr->rr_index + attempt) % sqr->num_active;
        hv_sequence_t *seq = sqr->active_sequences[idx];
        if (seq && seq->get_next_item) {
            hv_sequence_item_t *item = seq->get_next_item(seq);
            if (item) {
                sqr->rr_index = (uint32_t)((idx + 1) % sqr->num_active);
                sqr->items_processed++;
                return item;
            }
        }
    }
    /* No sequence has items ready */
    return NULL;
}

hv_sequencer_t *hv_sequencer_create(const char *name) {
    hv_sequencer_t *sqr = (hv_sequencer_t*)calloc(1, sizeof(hv_sequencer_t));
    if (!sqr) return NULL;
    strncpy(sqr->base.name, name, sizeof(sqr->base.name) - 1);
    sqr->base.impl = sqr;
    sqr->base.run_phase = hv_sequencer_run_phase;
    memset(sqr->active_sequences, 0, sizeof(sqr->active_sequences));
    sqr->num_active = 0;
    sqr->pending_sequences = NULL;
    sqr->num_pending = 0;
    sqr->seq_item_port = hv_tlm_port_create(name, sqr);
    sqr->items_processed = 0;
    sqr->rr_index = 0;
    sqr->arbitrate = hv_sequencer_rr_arbitrate;
    return sqr;
}

void hv_sequencer_destroy(hv_sequencer_t *sqr) {
    if (!sqr) return;
    hv_tlm_port_destroy(sqr->seq_item_port);
    free(sqr);
}

void hv_sequencer_run_phase(hv_component_t *comp) {
    hv_sequencer_t *sqr = (hv_sequencer_t*)comp->impl;
    if (!sqr || !sqr->arbitrate || !sqr->seq_item_port) return;
    hv_sequence_item_t *item = sqr->arbitrate(sqr);
    if (item && sqr->seq_item_port->write) {
        sqr->seq_item_port->write(sqr->seq_item_port, &item->trans);
    }
}

/* ========================================================================
 * Scoreboard (L2) - Reference Model + Checker
 * ======================================================================== */

hv_scoreboard_t *hv_scoreboard_create(const char *name) {
    hv_scoreboard_t *sb = (hv_scoreboard_t*)calloc(1, sizeof(hv_scoreboard_t));
    if (!sb) return NULL;
    strncpy(sb->base.name, name, sizeof(sb->base.name) - 1);
    sb->base.impl = sb;
    sb->base.run_phase = hv_scoreboard_run_phase;
    sb->monitor_imp = NULL;
    sb->expected_list = NULL;
    sb->expected_count = 0;
    sb->tx_checked = 0;
    sb->tx_matched = 0;
    sb->tx_mismatch = 0;
    sb->tx_unexpected = 0;
    sb->predict = NULL;
    sb->add_expected = NULL;
    sb->check = NULL;
    return sb;
}

void hv_scoreboard_destroy(hv_scoreboard_t *sb) {
    if (!sb) return;
    hv_tlm_imp_destroy(sb->monitor_imp);
    struct hv_scoreboard_expected *e = sb->expected_list;
    while (e) {
        struct hv_scoreboard_expected *next = e->next;
        free(e);
        e = next;
    }
    free(sb);
}

/* Default check: compare addr+data+cmd */
static bool scoreboard_default_check(hv_scoreboard_t *sb, hv_transaction_t *actual) {
    if (!sb || !actual) return false;
    sb->tx_checked++;

    /* Search expected list for a match */
    struct hv_scoreboard_expected *prev = NULL;
    struct hv_scoreboard_expected *e = sb->expected_list;
    while (e) {
        if (!e->matched &&
            e->tx.addr == actual->addr &&
            e->tx.data == actual->data &&
            e->tx.cmd  == actual->cmd) {
            e->matched = true;
            sb->tx_matched++;
            return true;
        }
        prev = e;
        e = e->next;
    }
    sb->tx_unexpected++;
    return false;
}

void hv_scoreboard_run_phase(hv_component_t *comp) {
    hv_scoreboard_t *sb = (hv_scoreboard_t*)comp->impl;
    if (!sb) return;
    /* If check method is not set, use default */
    if (!sb->check) {
        sb->check = scoreboard_default_check;
    }
}

/* ========================================================================
 * Agent (L2)
 * ======================================================================== */

hv_agent_t *hv_agent_create(const char *name, hv_dut_t *dut, bool active) {
    hv_agent_t *agent = (hv_agent_t*)calloc(1, sizeof(hv_agent_t));
    if (!agent) return NULL;
    strncpy(agent->base.name, name, sizeof(agent->base.name) - 1);
    agent->base.impl = agent;
    agent->base.run_phase = hv_agent_run_phase;
    agent->agent_type = active ? COMP_AGENT : COMP_MONITOR;
    agent->dut = dut;
    agent->is_active = active;

    /* Always create monitor */
    char mon_name[72];
    snprintf(mon_name, sizeof(mon_name), "%s_mon", name);
    agent->monitor = hv_monitor_create(mon_name, dut);

    if (active) {
        char drv_name[72], sqr_name[72];
        snprintf(drv_name, sizeof(drv_name), "%s_drv", name);
        snprintf(sqr_name, sizeof(sqr_name), "%s_sqr", name);
        agent->driver = hv_driver_create(drv_name, dut);
        agent->sequencer = hv_sequencer_create(sqr_name);
    } else {
        agent->driver = NULL;
        agent->sequencer = NULL;
    }

    return agent;
}

void hv_agent_destroy(hv_agent_t *agent) {
    if (!agent) return;
    hv_monitor_destroy(agent->monitor);
    if (agent->driver) hv_driver_destroy(agent->driver);
    if (agent->sequencer) hv_sequencer_destroy(agent->sequencer);
    free(agent);
}

void hv_agent_run_phase(hv_component_t *comp) {
    hv_agent_t *agent = (hv_agent_t*)comp->impl;
    if (!agent) return;
    if (agent->monitor && agent->monitor->base.run_phase) {
        agent->monitor->base.run_phase(&agent->monitor->base);
    }
    if (agent->is_active && agent->driver && agent->driver->base.run_phase) {
        agent->driver->base.run_phase(&agent->driver->base);
    }
}

/* ========================================================================
 * Environment (L2)
 * ======================================================================== */

hv_env_t *hv_env_create(const char *name) {
    hv_env_t *env = (hv_env_t*)calloc(1, sizeof(hv_env_t));
    if (!env) return NULL;
    strncpy(env->base.name, name, sizeof(env->base.name) - 1);
    env->base.impl = env;
    env->base.run_phase = hv_env_run_phase;
    env->num_agents = 0;
    memset(env->agents, 0, sizeof(env->agents));
    env->scoreboard = NULL;
    env->config_db = hv_config_db_create();
    env->verify_plan = NULL;
    return env;
}

void hv_env_destroy(hv_env_t *env) {
    if (!env) return;
    for (size_t i = 0; i < env->num_agents; i++) {
        hv_agent_destroy(env->agents[i]);
    }
    hv_scoreboard_destroy(env->scoreboard);
    hv_config_db_destroy(env->config_db);
    hv_verify_plan_destroy(env->verify_plan);
    free(env);
}

void hv_env_run_phase(hv_component_t *comp) {
    hv_env_t *env = (hv_env_t*)comp->impl;
    if (!env) return;
    for (size_t i = 0; i < env->num_agents; i++) {
        if (env->agents[i] && env->agents[i]->base.run_phase) {
            env->agents[i]->base.run_phase(&env->agents[i]->base);
        }
    }
    if (env->scoreboard && env->scoreboard->base.run_phase) {
        env->scoreboard->base.run_phase(&env->scoreboard->base);
    }
}

/* ========================================================================
 * Sequence Generation (L5)
 * ======================================================================== */

hv_sequence_t *hv_sequence_create(const char *name,
                                   void (*body)(hv_sequence_t*)) {
    hv_sequence_t *seq = (hv_sequence_t*)calloc(1, sizeof(hv_sequence_t));
    if (!seq) return NULL;
    strncpy(seq->name, name, sizeof(seq->name) - 1);
    seq->head = NULL;
    seq->tail = NULL;
    seq->count = 0;
    seq->is_running = false;
    seq->body = body;
    seq->get_next_item = NULL;
    seq->user_data = NULL;
    return seq;
}

void hv_sequence_destroy(hv_sequence_t *seq) {
    if (!seq) return;
    hv_sequence_item_t *item = seq->head;
    while (item) {
        hv_sequence_item_t *next = item->next;
        hv_sequence_item_destroy(item);
        item = next;
    }
    free(seq);
}

void hv_sequence_add_item(hv_sequence_t *seq, hv_sequence_item_t *item) {
    if (!seq || !item) return;
    item->next = NULL;
    item->prev = seq->tail;
    if (seq->tail) {
        seq->tail->next = item;
    } else {
        seq->head = item;
    }
    seq->tail = item;
    seq->count++;
}

hv_sequence_item_t *hv_sequence_item_create(void) {
    hv_sequence_item_t *item = (hv_sequence_item_t*)calloc(1, sizeof(hv_sequence_item_t));
    if (!item) return NULL;
    item->is_done = false;
    item->trans.trans_id = 0;
    item->start_time = 0;
    item->end_time = 0;
    item->next = NULL;
    item->prev = NULL;
    return item;
}

void hv_sequence_item_destroy(hv_sequence_item_t *item) {
    free(item);
}
