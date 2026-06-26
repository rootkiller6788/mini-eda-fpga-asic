#include "verilog_sim.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void vs_init(VerilogSimulator *sim) {
    memset(sim, 0, sizeof(*sim));
    sim->running = false;
    sim->current_time = 0;
    sim->vcd_file = NULL;
}

int vs_add_module(VerilogSimulator *sim, const char *name) {
    assert(sim->module_count < VS_MAX_MODULES);
    int idx = sim->module_count++;
    VerilogModule *mod = &sim->modules[idx];
    memset(mod, 0, sizeof(*mod));
    strncpy(mod->name, name, VS_MAX_NAME_LEN - 1);
    return idx;
}

VerilogModule *vs_get_module(VerilogSimulator *sim, int idx) {
    if (idx >= 0 && idx < sim->module_count) return &sim->modules[idx];
    return NULL;
}

int vs_add_port(VerilogModule *mod, const char *name, VerilogPortDir dir, int width) {
    assert(mod->port_count < VS_MAX_PORTS);
    int idx = mod->port_count++;
    VerilogPort *port = &mod->ports[idx];
    memset(port, 0, sizeof(*port));
    strncpy(port->name, name, VS_MAX_NAME_LEN - 1);
    port->direction = dir;
    port->width = width;
    port->net_index = -1;
    return idx;
}

int vs_add_net(VerilogModule *mod, const char *name, VerilogNetKind kind, int width) {
    assert(mod->net_count < VS_MAX_NETS);
    int idx = mod->net_count++;
    VerilogNet *net = &mod->nets[idx];
    memset(net, 0, sizeof(*net));
    strncpy(net->name, name, VS_MAX_NAME_LEN - 1);
    net->kind = kind;
    net->value = VS_VAL_X;
    net->width = width;
    net->driven = false;
    net->vcd_identifier = (char)('a' + idx);
    return idx;
}

void vs_add_assign(VerilogModule *mod, int lhs_net, const VerilogValue *rhs, int width) {
    assert(mod->assign_count < VS_MAX_ASSIGNS);
    int idx = mod->assign_count++;
    VerilogAssign *assign = &mod->assigns[idx];
    assign->lhs_net = lhs_net;
    assign->rhs_width = width;
    assign->rhs_values = (VerilogValue *)malloc((size_t)width * sizeof(VerilogValue));
    if (rhs) memcpy(assign->rhs_values, rhs, (size_t)width * sizeof(VerilogValue));
}

int vs_add_always(VerilogModule *mod) {
    assert(mod->always_count < VS_MAX_ALWAYS);
    int idx = mod->always_count++;
    VerilogAlwaysBlock *blk = &mod->always_blocks[idx];
    memset(blk, 0, sizeof(*blk));
    blk->entry_stmt = -1;
    return idx;
}

void vs_add_sensitivity(VerilogAlwaysBlock *blk, VerilogSensitivityKind kind, int signal) {
    assert(blk->sensitivity_count < VS_MAX_SENSITIVITY);
    int i = blk->sensitivity_count++;
    blk->sensitivity[i].kind = kind;
    blk->sensitivity[i].signal_index = signal;
}

int vs_add_stmt(VerilogAlwaysBlock *blk, VerilogStmtKind kind) {
    assert(blk->stmt_count < VS_MAX_STMTS);
    int idx = blk->stmt_count++;
    memset(&blk->stmts[idx], 0, sizeof(VerilogStmt));
    blk->stmts[idx].kind = kind;
    if (blk->entry_stmt < 0) blk->entry_stmt = idx;
    return idx;
}

void vs_add_blocking_assign(VerilogAlwaysBlock *blk, int stmt, int lhs, const VerilogValue *rhs, int width) {
    assert(stmt >= 0 && stmt < blk->stmt_count);
    VerilogStmt *s = &blk->stmts[stmt];
    s->kind = VS_STMT_BLOCKING_ASSIGN;
    s->lhs_net = lhs;
    s->rhs_width = width;
    s->rhs_values = (VerilogValue *)malloc((size_t)width * sizeof(VerilogValue));
    if (rhs) memcpy(s->rhs_values, rhs, (size_t)width * sizeof(VerilogValue));
}

void vs_add_nonblocking_assign(VerilogAlwaysBlock *blk, int stmt, int lhs, const VerilogValue *rhs, int width, int delay) {
    assert(stmt >= 0 && stmt < blk->stmt_count);
    VerilogStmt *s = &blk->stmts[stmt];
    s->kind = VS_STMT_NONBLOCKING_ASSIGN;
    s->lhs_net = lhs;
    s->rhs_width = width;
    s->delay = delay;
    s->rhs_values = (VerilogValue *)malloc((size_t)width * sizeof(VerilogValue));
    if (rhs) memcpy(s->rhs_values, rhs, (size_t)width * sizeof(VerilogValue));
}

void vs_schedule_event(VerilogSimulator *sim, VerilogTime t, VerilogEventType type, int signal, int module) {
    assert(sim->event_count < VS_MAX_EVENTS);
    int i = sim->event_count++;
    sim->events[i].time = t;
    sim->events[i].type = type;
    sim->events[i].signal_index = signal;
    sim->events[i].module_index = module;
    sim->events[i].callback_data = NULL;
}

static int vs_compare_events(const void *a, const void *b) {
    const VerilogEvent *ea = (const VerilogEvent *)a;
    const VerilogEvent *eb = (const VerilogEvent *)b;
    if (ea->time < eb->time) return -1;
    if (ea->time > eb->time) return 1;
    return 0;
}

static bool vs_is_sensitive(const VerilogAlwaysBlock *blk, int signal_idx, VerilogEventType evt) {
    for (int i = 0; i < blk->sensitivity_count; i++) {
        if (blk->sensitivity[i].signal_index != signal_idx) continue;
        switch (blk->sensitivity[i].kind) {
            case VS_SENS_POSEDGE: if (evt == VS_EVT_POSEDGE) return true; break;
            case VS_SENS_NEGEDGE: if (evt == VS_EVT_NEGEDGE) return true; break;
            case VS_SENS_LEVEL:   return true;
        }
    }
    return false;
}

void vs_evaluate_continuous_assigns(VerilogModule *mod) {
    for (int i = 0; i < mod->assign_count; i++) {
        VerilogAssign *a = &mod->assigns[i];
        if (a->lhs_net >= 0 && a->lhs_net < mod->net_count && a->rhs_width > 0) {
            mod->nets[a->lhs_net].value = a->rhs_values[0];
            mod->nets[a->lhs_net].driven = true;
        }
    }
}

void vs_evaluate_always_block(VerilogSimulator *sim, VerilogModule *mod, VerilogAlwaysBlock *blk) {
    (void)sim;
    for (int s = blk->entry_stmt; s >= 0 && s < blk->stmt_count; s++) {
        VerilogStmt *stmt = &blk->stmts[s];
        switch (stmt->kind) {
            case VS_STMT_BLOCKING_ASSIGN:
                if (stmt->lhs_net >= 0 && stmt->lhs_net < mod->net_count && stmt->rhs_values) {
                    mod->nets[stmt->lhs_net].value = stmt->rhs_values[0];
                    mod->nets[stmt->lhs_net].driven = true;
                }
                break;
            case VS_STMT_NONBLOCKING_ASSIGN:
                if (stmt->delay > 0 && stmt->lhs_net >= 0 && stmt->lhs_net < mod->net_count && stmt->rhs_values) {
                    vs_schedule_event(sim, sim->current_time + (VerilogTime)stmt->delay,
                                      VS_EVT_LEVEL, stmt->lhs_net, -1);
                }
                break;
            case VS_STMT_DISPLAY:
                printf("[%llu] %s\n", (unsigned long long)sim->current_time, stmt->display_msg);
                break;
            default:
                break;
        }
    }
}

void vs_run(VerilogSimulator *sim, VerilogTime end_time) {
    sim->running = true;
    while (sim->running && sim->event_count > 0) {
        qsort(sim->events, (size_t)sim->event_count, sizeof(VerilogEvent), vs_compare_events);
        VerilogEvent evt = sim->events[0];
        memmove(&sim->events[0], &sim->events[1], (size_t)(sim->event_count - 1) * sizeof(VerilogEvent));
        sim->event_count--;

        if (evt.time > end_time) {
            sim->current_time = end_time;
            break;
        }
        sim->current_time = evt.time;

        for (int m = 0; m < sim->module_count; m++) {
            VerilogModule *mod = &sim->modules[m];
            vs_evaluate_continuous_assigns(mod);
            for (int a = 0; a < mod->always_count; a++) {
                if (vs_is_sensitive(&mod->always_blocks[a], evt.signal_index, evt.type)) {
                    vs_evaluate_always_block(sim, mod, &mod->always_blocks[a]);
                }
            }
        }

        if (sim->vcd_file) {
            vs_vcd_print_time(sim, evt.time);
            vs_vcd_dump_signals(sim);
        }
    }
}

void vs_set_net_value(VerilogModule *mod, int net, VerilogValue val) {
    if (net < 0 || net >= mod->net_count) return;
    VerilogValue old = mod->nets[net].value;
    mod->nets[net].value = val;
    mod->nets[net].driven = true;
}

VerilogValue vs_get_net_value(const VerilogModule *mod, int net) {
    if (net >= 0 && net < mod->net_count) return mod->nets[net].value;
    return VS_VAL_X;
}

void vs_display_signals(const VerilogModule *mod, VerilogTime t) {
    printf("[t=%llu] Module: %s\n", (unsigned long long)t, mod->name);
    for (int i = 0; i < mod->net_count; i++) {
        printf("  %s = ", mod->nets[i].name);
        switch (mod->nets[i].value) {
            case VS_VAL_0: printf("0"); break;
            case VS_VAL_1: printf("1"); break;
            case VS_VAL_X: printf("x"); break;
            case VS_VAL_Z: printf("z"); break;
        }
        printf(" (%s)\n", mod->nets[i].kind == VS_NET_WIRE ? "wire" :
                          mod->nets[i].kind == VS_NET_REG ? "reg" : "other");
    }
}

static const char *vs_val_to_vcd(VerilogValue v) {
    switch (v) {
        case VS_VAL_0: return "0";
        case VS_VAL_1: return "1";
        case VS_VAL_X: return "x";
        case VS_VAL_Z: return "z";
        default: return "x";
    }
}

void vs_vcd_open(VerilogSimulator *sim, const char *filename) {
    sim->vcd_file = fopen(filename, "w");
    if (!sim->vcd_file) return;
    fprintf(sim->vcd_file, "$date\n  Today\n$end\n");
    fprintf(sim->vcd_file, "$version\n  mini-hdl-lang v1.0\n$end\n");
    fprintf(sim->vcd_file, "$timescale 1ns $end\n");

    for (int m = 0; m < sim->module_count; m++) {
        VerilogModule *mod = &sim->modules[m];
        fprintf(sim->vcd_file, "$scope module %s $end\n", mod->name);
        sim->vcd_signal_count = 0;
        for (int n = 0; n < mod->net_count; n++) {
            fprintf(sim->vcd_file, "$var wire %d %c %s $end\n",
                    mod->nets[n].width, mod->nets[n].vcd_identifier, mod->nets[n].name);
            if (sim->vcd_signal_count < VS_MAX_VCD_SIGNALS) {
                sim->vcd_signal_ids[sim->vcd_signal_count++] = n;
            }
        }
        fprintf(sim->vcd_file, "$upscope $end\n");
    }
    fprintf(sim->vcd_file, "$enddefinitions $end\n");
    sim->vcd_last_time = 0;
}

void vs_vcd_dump_signals(VerilogSimulator *sim) {
    if (!sim->vcd_file) return;
    for (int m = 0; m < sim->module_count; m++) {
        VerilogModule *mod = &sim->modules[m];
        for (int n = 0; n < mod->net_count; n++) {
            fprintf(sim->vcd_file, "%s%c\n",
                    vs_val_to_vcd(mod->nets[n].value),
                    mod->nets[n].vcd_identifier);
        }
    }
}

void vs_vcd_print_time(VerilogSimulator *sim, VerilogTime t) {
    if (!sim->vcd_file) return;
    if (t != sim->vcd_last_time) {
        fprintf(sim->vcd_file, "#%llu\n", (unsigned long long)t);
        sim->vcd_last_time = t;
    }
}

void vs_vcd_close(VerilogSimulator *sim) {
    if (sim->vcd_file) {
        fprintf(sim->vcd_file, "#%llu\n", (unsigned long long)sim->current_time);
        fclose(sim->vcd_file);
        sim->vcd_file = NULL;
    }
}

void vs_free(VerilogSimulator *sim) {
    vs_vcd_close(sim);
    for (int m = 0; m < sim->module_count; m++) {
        VerilogModule *mod = &sim->modules[m];
        for (int a = 0; a < mod->assign_count; a++) {
            free(mod->assigns[a].rhs_values);
        }
        for (int b = 0; b < mod->always_count; b++) {
            for (int s = 0; s < mod->always_blocks[b].stmt_count; s++) {
                free(mod->always_blocks[b].stmts[s].rhs_values);
            }
        }
    }
    memset(sim, 0, sizeof(*sim));
}
