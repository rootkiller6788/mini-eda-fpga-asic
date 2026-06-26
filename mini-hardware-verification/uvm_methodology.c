#include "uvm_methodology.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ================================================================
   UVM Methodology Implementation
   uvm_methodology.c
   ================================================================ */

/* ----- Sequence Item ----- */
uvm_sequence_item_t* uvm_seq_item_create(const char* type_name, size_t data_size) {
    uvm_sequence_item_t* item = calloc(1, sizeof(uvm_sequence_item_t));
    if (!item) return NULL;
    strncpy(item->type_name, type_name, sizeof(item->type_name) - 1);
    item->data_size = data_size;
    item->item_id   = 1;
    item->timestamp = 0;
    if (data_size > 0) {
        item->data = calloc(1, data_size);
    }
    item->print   = uvm_default_print;
    item->copy    = uvm_default_copy;
    item->compare = uvm_default_compare;
    return item;
}

void uvm_seq_item_destroy(uvm_sequence_item_t* item) {
    if (!item) return;
    free(item->data);
    free(item);
}

void uvm_seq_item_set_data(uvm_sequence_item_t* item, const void* src, size_t sz) {
    if (!item || !src) return;
    if (sz != item->data_size) {
        free(item->data);
        item->data = malloc(sz);
        item->data_size = sz;
    }
    memcpy(item->data, src, sz);
}

int uvm_default_compare(const uvm_sequence_item_t* a, const uvm_sequence_item_t* b) {
    if (!a || !b) return -1;
    if (a->data_size != b->data_size) return (int)(a->data_size - b->data_size);
    if (a->data && b->data)
        return memcmp(a->data, b->data, a->data_size);
    return (a->data == b->data) ? 0 : -1;
}

void uvm_default_print(const uvm_sequence_item_t* item) {
    if (!item) return;
    printf("[SeqItem] type=%s id=%u ts=%llu size=%zu\n",
        item->type_name, item->item_id,
        (unsigned long long)item->timestamp, item->data_size);
}

void uvm_default_copy(uvm_sequence_item_t* dst, const uvm_sequence_item_t* src) {
    if (!dst || !src) return;
    uvm_seq_item_set_data(dst, src->data, src->data_size);
    dst->item_id   = src->item_id;
    dst->timestamp = src->timestamp;
    strncpy(dst->type_name, src->type_name, sizeof(dst->type_name) - 1);
}

/* ----- Sequencer ----- */
uvm_sequencer_t* uvm_sequencer_create(const char* name) {
    uvm_sequencer_t* sqr = calloc(1, sizeof(uvm_sequencer_t));
    if (!sqr) return NULL;
    strncpy(sqr->name, name, sizeof(sqr->name) - 1);
    sqr->seed = 0xDEADBEEF;
    sqr->fifo_cnt = 0;
    sqr->fifo_wr  = 0;
    sqr->fifo_rd  = 0;
    return sqr;
}

void uvm_sequencer_destroy(uvm_sequencer_t* sqr) {
    if (!sqr) return;
    for (int i = 0; i < sqr->seq_count; i++)
        uvm_sequencer_stop_sequence(sqr, sqr->active_sequences[i]);
    free(sqr);
}

bool uvm_sequencer_start_sequence(uvm_sequencer_t* sqr, uvm_sequence_t* seq) {
    if (!sqr || !seq || sqr->seq_count >= UVM_SEQUENCER_MAX_SEQ) return false;
    seq->is_running = true;
    sqr->active_sequences[sqr->seq_count++] = seq;
    return true;
}

void uvm_sequencer_stop_sequence(uvm_sequencer_t* sqr, uvm_sequence_t* seq) {
    if (!sqr || !seq) return;
    seq->is_running = false;
    for (int i = 0; i < sqr->seq_count; i++) {
        if (sqr->active_sequences[i] == seq) {
            sqr->active_sequences[i] = sqr->active_sequences[--sqr->seq_count];
            break;
        }
    }
}

bool uvm_sequencer_push_item(uvm_sequencer_t* sqr, uvm_sequence_item_t* item) {
    if (!sqr || sqr->fifo_cnt >= UVM_SEQUENCER_FIFO_DEPTH) return false;
    sqr->fifo[sqr->fifo_wr] = item;
    sqr->fifo_wr = (sqr->fifo_wr + 1) % UVM_SEQUENCER_FIFO_DEPTH;
    sqr->fifo_cnt++;
    return true;
}

uvm_sequence_item_t* uvm_sequencer_pop_item(uvm_sequencer_t* sqr) {
    if (!sqr || sqr->fifo_cnt <= 0) return NULL;
    uvm_sequence_item_t* item = sqr->fifo[sqr->fifo_rd];
    sqr->fifo_rd = (sqr->fifo_rd + 1) % UVM_SEQUENCER_FIFO_DEPTH;
    sqr->fifo_cnt--;
    return item;
}

bool uvm_sequencer_has_item(uvm_sequencer_t* sqr) {
    return sqr && sqr->fifo_cnt > 0;
}

/* ----- Driver ----- */
uvm_driver_t* uvm_driver_create(const char* name, uvm_sequencer_t* sqr) {
    uvm_driver_t* drv = calloc(1, sizeof(uvm_driver_t));
    if (!drv) return NULL;
    strncpy(drv->name, name, sizeof(drv->name) - 1);
    drv->sequencer = sqr;
    drv->is_active = true;
    return drv;
}

void uvm_driver_destroy(uvm_driver_t* drv) { free(drv); }
void uvm_driver_set_run_handler(uvm_driver_t* drv, uvm_driver_run_fn fn) {
    if (drv) drv->run_phase = fn;
}
void uvm_driver_run(uvm_driver_t* drv) {
    if (drv && drv->run_phase) {
        drv->run_phase(drv);
    }
}
void uvm_driver_reset(uvm_driver_t* drv) {
    if (drv && drv->reset) drv->reset(drv);
}

/* ----- Monitor ----- */
uvm_monitor_t* uvm_monitor_create(const char* name) {
    uvm_monitor_t* mon = calloc(1, sizeof(uvm_monitor_t));
    if (!mon) return NULL;
    strncpy(mon->name, name, sizeof(mon->name) - 1);
    mon->is_active = true;
    return mon;
}

void uvm_monitor_destroy(uvm_monitor_t* mon) { free(mon); }
void uvm_monitor_set_sample_handler(uvm_monitor_t* mon, uvm_monitor_sample_fn fn) {
    if (mon) mon->sample = fn;
}
void uvm_monitor_sample(uvm_monitor_t* mon, const uvm_sequence_item_t* item) {
    if (!mon || !item) return;
    mon->sample_count++;
    if (mon->sample) mon->sample(mon, item);
}

/* ----- Scoreboard ----- */
#define UVM_SB_FIFO_CAP 256

typedef struct {
    uvm_sequence_item_t** items;
    int head, tail, count, cap;
} uvm_sb_fifo_t;

uvm_scoreboard_t* uvm_scoreboard_create(const char* name, uvm_sb_compare_fn cmp) {
    uvm_scoreboard_t* sb = calloc(1, sizeof(uvm_scoreboard_t));
    if (!sb) return NULL;
    strncpy(sb->name, name, sizeof(sb->name) - 1);
    sb->compare = cmp;
    uvm_sb_fifo_t* fifo = calloc(1, sizeof(uvm_sb_fifo_t));
    fifo->cap = UVM_SB_FIFO_CAP;
    fifo->items = calloc((size_t)fifo->cap, sizeof(uvm_sequence_item_t*));
    sb->expected_fifo = fifo;
    return sb;
}

void uvm_scoreboard_destroy(uvm_scoreboard_t* sb) {
    if (!sb) return;
    uvm_sb_fifo_t* fifo = (uvm_sb_fifo_t*)sb->expected_fifo;
    if (fifo) {
        free(fifo->items);
        free(fifo);
    }
    free(sb);
}

void uvm_scoreboard_add_expected(uvm_scoreboard_t* sb, const uvm_sequence_item_t* item) {
    if (!sb || !item) return;
    uvm_sb_fifo_t* f = (uvm_sb_fifo_t*)sb->expected_fifo;
    if (!f || f->count >= f->cap) return;
    f->items[f->tail] = (uvm_sequence_item_t*)item;
    f->tail = (f->tail + 1) % f->cap;
    f->count++;
}

bool uvm_scoreboard_check_actual(uvm_scoreboard_t* sb, const uvm_sequence_item_t* item) {
    if (!sb || !item) return false;
    uvm_sb_fifo_t* f = (uvm_sb_fifo_t*)sb->expected_fifo;
    if (!f || f->count <= 0) { sb->drop_count++; return false; }
    uvm_sequence_item_t* exp = f->items[f->head];
    f->head = (f->head + 1) % f->cap;
    f->count--;
    bool match = sb->compare ? sb->compare(sb, exp, item) : (uvm_default_compare(exp, item) == 0);
    if (match) sb->match_count++; else sb->mismatch_count++;
    return match;
}

void uvm_scoreboard_report(const uvm_scoreboard_t* sb) {
    if (!sb) return;
    printf("[Scoreboard %s] match=%llu mismatch=%llu drop=%llu\n",
        sb->name,
        (unsigned long long)sb->match_count,
        (unsigned long long)sb->mismatch_count,
        (unsigned long long)sb->drop_count);
}

/* ----- TLM Port ----- */
tlm_port_t* tlm_port_create(const char* name, tlm_port_type_t type) {
    tlm_port_t* port = calloc(1, sizeof(tlm_port_t));
    if (!port) return NULL;
    strncpy(port->name, name, sizeof(port->name) - 1);
    port->type = type;
    return port;
}

void tlm_port_destroy(tlm_port_t* port) { free(port); }

bool tlm_port_connect(tlm_port_t* port, tlm_port_t* peer) {
    if (!port || !peer) return false;
    port->peer = peer;
    port->is_connected = true;
    peer->peer = port;
    peer->is_connected = true;
    return true;
}

bool tlm_port_put(tlm_port_t* port, uvm_sequence_item_t* item) {
    if (!port || !item) return false;
    if (port->transport) return port->transport(port, item);
    printf("[TLM %s] put item id=%u\n", port->name, item->item_id);
    return true;
}

bool tlm_port_get(tlm_port_t* port, uvm_sequence_item_t** item) {
    if (!port || !item) return false;
    printf("[TLM %s] get request\n", port->name);
    return false;
}

bool tlm_port_write_analysis(tlm_port_t* port, uvm_sequence_item_t* item) {
    if (!port) return false;
    tlm_port_t* peer = (tlm_port_t*)port->peer;
    if (peer && peer->transport) {
        return peer->transport(peer, item);
    }
    return tlm_port_put(port, item);
}

/* ----- Agent ----- */
uvm_agent_t* uvm_agent_create(const char* name, uvm_agent_type_t type) {
    uvm_agent_t* ag = calloc(1, sizeof(uvm_agent_t));
    if (!ag) return NULL;
    strncpy(ag->name, name, sizeof(ag->name) - 1);
    ag->agent_type = type;
    return ag;
}

void uvm_agent_destroy(uvm_agent_t* agent) {
    if (!agent) return;
    uvm_driver_destroy(agent->driver);
    uvm_monitor_destroy(agent->monitor);
    uvm_sequencer_destroy(agent->sequencer);
    tlm_port_destroy(agent->monitor_ap);
    free(agent);
}

void uvm_agent_connect(uvm_agent_t* agent) {
    if (!agent) return;
    if (agent->agent_type == UVM_AGENT_ACTIVE && agent->driver && agent->sequencer) {
        agent->driver->sequencer = agent->sequencer;
    }
}

/* ----- Environment ----- */
uvm_env_t* uvm_env_create(const char* name) {
    uvm_env_t* env = calloc(1, sizeof(uvm_env_t));
    if (!env) return NULL;
    strncpy(env->name, name, sizeof(env->name) - 1);
    env->agent_capacity = 8;
    env->agents = calloc((size_t)env->agent_capacity, sizeof(uvm_agent_t*));
    return env;
}

void uvm_env_destroy(uvm_env_t* env) {
    if (!env) return;
    for (int i = 0; i < env->agent_count; i++)
        uvm_agent_destroy(env->agents[i]);
    free(env->agents);
    uvm_scoreboard_destroy(env->scoreboard);
    free(env);
}

void uvm_env_add_agent(uvm_env_t* env, uvm_agent_t* agent) {
    if (!env || !agent) return;
    if (env->agent_count >= env->agent_capacity) {
        env->agent_capacity *= 2;
        env->agents = realloc(env->agents,
            (size_t)env->agent_capacity * sizeof(uvm_agent_t*));
    }
    env->agents[env->agent_count++] = agent;
}

void uvm_env_set_scoreboard(uvm_env_t* env, uvm_scoreboard_t* sb) {
    if (env) env->scoreboard = sb;
}

/* ----- Test ----- */
uvm_test_t* uvm_test_create(const char* name) {
    uvm_test_t* t = calloc(1, sizeof(uvm_test_t));
    if (!t) return NULL;
    strncpy(t->name, name, sizeof(t->name) - 1);
    t->verbosity = UVM_MEDIUM;
    t->seed = 42;
    t->timeout_ns = 1000000000; /* 1 second */
    t->pass = true;
    return t;
}

void uvm_test_destroy(uvm_test_t* test) {
    if (!test) return;
    uvm_env_destroy(test->env);
    free(test);
}

void uvm_test_set_env(uvm_test_t* test, uvm_env_t* env) {
    if (test) test->env = env;
}

void uvm_test_set_run_handler(uvm_test_t* test, uvm_test_run_fn fn) {
    if (test) test->run = fn;
}

bool uvm_test_run(uvm_test_t* test) {
    if (!test) return false;
    printf("[Test %s] starting (seed=%u, timeout=%llu ns)\n",
        test->name, test->seed, (unsigned long long)test->timeout_ns);
    if (test->run) test->run(test);
    return test->pass;
}

bool uvm_test_report(const uvm_test_t* test) {
    if (!test) return false;
    printf("\n[Test %s] %s (fails=%d)\n",
        test->name,
        test->pass ? "PASSED" : "FAILED",
        test->fail_count);
    return test->pass;
}

/* ----- Factory ----- */
static uvm_factory_t g_uvm_factory = {NULL, 0};

uvm_factory_t* uvm_factory_get(void) { return &g_uvm_factory; }

void uvm_factory_register(uvm_factory_t* f, const char* type_name, uvm_factory_create_fn create_fn) {
    if (!f || !type_name || !create_fn) return;
    uvm_component_reg_t* reg = calloc(1, sizeof(uvm_component_reg_t));
    strncpy(reg->type_name, type_name, sizeof(reg->type_name) - 1);
    reg->create_fn = create_fn;
    reg->next = f->registry;
    f->registry = reg;
    f->registry_count++;
}

void uvm_factory_set_override(uvm_factory_t* f, const char* original_type,
    const char* override_type, uvm_factory_create_fn create_fn)
{
    if (!f || !original_type) return;
    for (uvm_component_reg_t* r = f->registry; r; r = r->next) {
        if (strcmp(r->type_name, original_type) == 0) {
            if (override_type)
                strncpy(r->override_type, override_type, sizeof(r->override_type) - 1);
            r->override_fn = create_fn;
            return;
        }
    }
}

void* uvm_factory_create_component(uvm_factory_t* f, const char* type_name, const char* instance_name) {
    if (!f || !type_name) return NULL;
    for (uvm_component_reg_t* r = f->registry; r; r = r->next) {
        if (strcmp(r->type_name, type_name) == 0) {
            if (r->override_fn) return r->override_fn(instance_name);
            if (r->create_fn) return r->create_fn(instance_name);
        }
    }
    return NULL;
}

/* ----- Messages ----- */
void uvm_msg(uvm_severity_t severity, const char* id, const char* msg,
    uvm_verbosity_t verbosity, uvm_verbosity_t threshold)
{
    if (verbosity > threshold) return;
    static const char* sev_str[] = {"INFO", "WARNING", "ERROR", "FATAL"};
    printf("[%s] %s: %s\n", sev_str[severity], id, msg);
    if (severity == UVM_FATAL) exit(1);
}

void uvm_msg_info(const char* id, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    printf("[INFO] %s: ", id);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

void uvm_msg_warning(const char* id, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    printf("[WARNING] %s: ", id);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

void uvm_msg_error(const char* id, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    printf("[ERROR] %s: ", id);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

void uvm_msg_fatal(const char* id, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    printf("[FATAL] %s: ", id);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    exit(1);
}

/* ----- Phase Controller ----- */
static uvm_phase_ctrl_t g_phase_ctrl = {UVM_PHASE_BUILD, {0}, 0, 0};

uvm_phase_ctrl_t* uvm_phase_ctrl_get(void) { return &g_phase_ctrl; }

void uvm_phase_ctrl_transition(uvm_phase_ctrl_t* ctrl, uvm_phase_t next) {
    if (!ctrl) return;
    ctrl->phase_done[ctrl->current_phase] = true;
    ctrl->current_phase = next;
    printf("[PHASE] -> %s\n", uvm_phase_name(next));
}

void uvm_phase_raise_objection(uvm_phase_ctrl_t* ctrl) {
    if (ctrl) ctrl->raised_count++;
}

void uvm_phase_drop_objection(uvm_phase_ctrl_t* ctrl) {
    if (ctrl) ctrl->dropped_count++;
}

bool uvm_phase_is_done(uvm_phase_ctrl_t* ctrl) {
    if (!ctrl) return true;
    return ctrl->dropped_count >= ctrl->raised_count;
}
