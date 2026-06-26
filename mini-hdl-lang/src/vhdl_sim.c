#include "vhdl_sim.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

VhdlStdLogic vhdl_resolve_std_logic(const VhdlStdLogic *drivers, int count) {
    if (count == 0) return VHDL_STD_U;

    bool has_0 = false, has_1 = false, has_z = false, has_x = false;

    for (int i = 0; i < count; i++) {
        switch (drivers[i]) {
            case VHDL_STD_0: case VHDL_STD_L: has_0 = true; break;
            case VHDL_STD_1: case VHDL_STD_H: has_1 = true; break;
            case VHDL_STD_Z: has_z = true; break;
            case VHDL_STD_X: case VHDL_STD_W: case VHDL_STD_DC: has_x = true; break;
            case VHDL_STD_U: break;
        }
    }

    if (has_0 && has_1) return VHDL_STD_X;
    if (has_0 && !has_1) return VHDL_STD_0;
    if (!has_0 && has_1) return VHDL_STD_1;
    if (has_z) return VHDL_STD_Z;
    if (has_x) return VHDL_STD_X;
    return VHDL_STD_U;
}

const char *vhdl_std_logic_to_char(VhdlStdLogic val) {
    static const char *chars[] = {"U", "X", "0", "1", "Z", "W", "L", "H", "-"};
    if (val <= VHDL_STD_DC) return chars[val];
    return "U";
}

VhdlStdLogic vhdl_char_to_std_logic(char c) {
    switch (c) {
        case 'U': case 'u': return VHDL_STD_U;
        case 'X': case 'x': return VHDL_STD_X;
        case '0': return VHDL_STD_0;
        case '1': return VHDL_STD_1;
        case 'Z': case 'z': return VHDL_STD_Z;
        case 'W': case 'w': return VHDL_STD_W;
        case 'L': case 'l': return VHDL_STD_L;
        case 'H': case 'h': return VHDL_STD_H;
        case '-': return VHDL_STD_DC;
        default: return VHDL_STD_U;
    }
}

void vhdl_init(VhdlSimulator *sim) {
    memset(sim, 0, sizeof(*sim));
}

int vhdl_add_entity(VhdlSimulator *sim, const char *name) {
    assert(sim->entity_count < VHDL_MAX_ENTITIES);
    int idx = sim->entity_count++;
    memset(&sim->entities[idx], 0, sizeof(VhdlEntity));
    strncpy(sim->entities[idx].name, name, VHDL_MAX_NAME_LEN - 1);
    return idx;
}

VhdlEntity *vhdl_get_entity(VhdlSimulator *sim, int idx) {
    if (idx >= 0 && idx < sim->entity_count) return &sim->entities[idx];
    return NULL;
}

int vhdl_add_port(VhdlEntity *ent, const char *name, VhdlPortMode mode, VhdlSignalType type, int width) {
    assert(ent->port_count < VHDL_MAX_PORTS);
    int idx = ent->port_count++;
    VhdlPort *port = &ent->ports[idx];
    memset(port, 0, sizeof(*port));
    strncpy(port->name, name, VHDL_MAX_NAME_LEN - 1);
    port->mode = mode;
    port->type = type;
    port->width = width;
    port->signal_index = -1;
    return idx;
}

int vhdl_add_architecture(VhdlSimulator *sim, int entity_idx, const char *name) {
    assert(sim->arch_count < VHDL_MAX_ENTITIES);
    int idx = sim->arch_count++;
    VhdlArchitecture *arch = &sim->architectures[idx];
    memset(arch, 0, sizeof(*arch));
    strncpy(arch->name, name, VHDL_MAX_NAME_LEN - 1);
    arch->entity_index = entity_idx;
    return idx;
}

VhdlArchitecture *vhdl_get_architecture(VhdlSimulator *sim, int idx) {
    if (idx >= 0 && idx < sim->arch_count) return &sim->architectures[idx];
    return NULL;
}

int vhdl_add_signal(VhdlArchitecture *arch, const char *name, VhdlSignalType type, int width, bool resolved) {
    assert(arch->signal_count < VHDL_MAX_SIGNALS);
    int idx = arch->signal_count++;
    VhdlSignal *sig = &arch->signals[idx];
    memset(sig, 0, sizeof(*sig));
    strncpy(sig->name, name, VHDL_MAX_NAME_LEN - 1);
    sig->type = type;
    sig->width = width;
    sig->is_resolved = resolved;
    sig->has_event = false;
    sig->resolved.value = VHDL_STD_U;
    return idx;
}

int vhdl_add_variable(VhdlProcess *proc, const char *name, VhdlSignalType type, int width) {
    assert(proc->var_count < VHDL_MAX_VARIABLES);
    int idx = proc->var_count++;
    VhdlVariable *var = &proc->variables[idx];
    memset(var, 0, sizeof(*var));
    strncpy(var->name, name, VHDL_MAX_NAME_LEN - 1);
    var->type = type;
    var->width = width;
    var->value = (VhdlStdLogic *)calloc((size_t)width, sizeof(VhdlStdLogic));
    return idx;
}

int vhdl_add_process(VhdlArchitecture *arch, const char *name) {
    assert(arch->process_count < VHDL_MAX_PROCESSES);
    int idx = arch->process_count++;
    VhdlProcess *proc = &arch->processes[idx];
    memset(proc, 0, sizeof(*proc));
    strncpy(proc->name, name, VHDL_MAX_NAME_LEN - 1);
    proc->entry_stmt = -1;
    proc->is_active = false;
    return idx;
}

void vhdl_add_process_sensitivity(VhdlProcess *proc, int signal_idx) {
    assert(proc->sensitivity_count < VHDL_MAX_PROCESSES * 2);
    proc->sensitivity[proc->sensitivity_count++] = signal_idx;
}

int vhdl_add_process_stmt(VhdlProcess *proc, VhdlStmtKind kind) {
    assert(proc->stmt_count < VHDL_MAX_STMTS);
    int idx = proc->stmt_count++;
    memset(&proc->stmts[idx], 0, sizeof(VhdlStmt));
    proc->stmts[idx].kind = kind;
    if (proc->entry_stmt < 0) proc->entry_stmt = idx;
    return idx;
}

void vhdl_add_signal_assign(VhdlProcess *proc, int stmt, int lhs, const VhdlStdLogic *rhs, int width, int delay) {
    assert(stmt >= 0 && stmt < proc->stmt_count);
    VhdlStmt *s = &proc->stmts[stmt];
    s->kind = VHDL_STMT_SIG_ASSIGN;
    s->lhs_signal = lhs;
    s->rhs_width = width;
    s->delay = delay;
    s->rhs_value = (VhdlStdLogic *)malloc((size_t)width * sizeof(VhdlStdLogic));
    if (rhs) memcpy(s->rhs_value, rhs, (size_t)width * sizeof(VhdlStdLogic));
}

int vhdl_add_concurrent(VhdlArchitecture *arch, VhdlConcurrentKind kind) {
    assert(arch->concurrent_count < VHDL_MAX_CONCURRENT);
    int idx = arch->concurrent_count++;
    VhdlConcurrentStmt *cs = &arch->concurrent_stmts[idx];
    memset(cs, 0, sizeof(*cs));
    cs->kind = kind;
    return idx;
}

void vhdl_set_concurrent_assign(int stmt, int target, const VhdlStdLogic *rhs, int width) {
    (void)stmt; (void)target; (void)rhs; (void)width;
}

void vhdl_run_delta_cycle(VhdlSimulator *sim, VhdlArchitecture *arch) {
    bool changed = true;
    while (changed) {
        changed = false;
        sim->delta_count++;

        for (int p = 0; p < arch->process_count; p++) {
            VhdlProcess *proc = &arch->processes[p];
            if (!proc->is_active && proc->sensitivity_count == 0) {
                proc->is_active = true;
            }
            if (proc->is_active) {
                vhdl_evaluate_process(sim, arch, proc);
                proc->is_active = false;
                changed = true;
            }
        }

        vhdl_evaluate_concurrent(arch);
        vhdl_resolve_signals(arch);

        for (int s = 0; s < arch->signal_count; s++) {
            if (arch->signals[s].has_event) {
                arch->signals[s].has_event = false;
                changed = true;
                for (int p = 0; p < arch->process_count; p++) {
                    for (int se = 0; se < arch->processes[p].sensitivity_count; se++) {
                        if (arch->processes[p].sensitivity[se] == s) {
                            arch->processes[p].is_active = true;
                        }
                    }
                }
            }
        }
    }
}

void vhdl_evaluate_process(VhdlSimulator *sim, VhdlArchitecture *arch, VhdlProcess *proc) {
    for (int s = proc->entry_stmt; s >= 0 && s < proc->stmt_count; s++) {
        VhdlStmt *stmt = &proc->stmts[s];
        switch (stmt->kind) {
            case VHDL_STMT_SIG_ASSIGN:
                if (stmt->lhs_signal >= 0 && stmt->rhs_value && stmt->lhs_signal < arch->signal_count) {
                    VhdlSignal *sig = &arch->signals[stmt->lhs_signal];
                    if (stmt->delay == 0) {
                        sig->resolved.drivers[0] = stmt->rhs_value[0];
                        sig->resolved.driver_count = 1;
                        sig->has_event = true;
                    } else {
                        int pidx = sig->projected_count++;
                        if (pidx < VHDL_MAX_PROJECTED) {
                            sig->projected[pidx] = stmt->rhs_value[0];
                        }
                    }
                }
                break;
            case VHDL_STMT_VAR_ASSIGN:
                if (stmt->lhs_signal >= 0 && stmt->rhs_value && stmt->lhs_signal < proc->var_count) {
                    VhdlVariable *var = &proc->variables[stmt->lhs_signal];
                    if (var->value) var->value[0] = stmt->rhs_value[0];
                }
                break;
            case VHDL_STMT_REPORT:
                if (stmt->report_msg[0]) {
                    printf("VHDL REPORT [t=%llu d=%llu]: %s\n",
                           (unsigned long long)sim->current_time,
                           (unsigned long long)sim->delta_count,
                           stmt->report_msg);
                }
                break;
            case VHDL_STMT_IF:
                if (stmt->cond_signal >= 0 && stmt->cond_signal < arch->signal_count) {
                    if (arch->signals[stmt->cond_signal].resolved.value == VHDL_STD_1) {
                        if (stmt->true_target >= 0) s = stmt->true_target - 1;
                    } else {
                        if (stmt->false_target >= 0) s = stmt->false_target - 1;
                    }
                }
                break;
            default:
                break;
        }
    }
}

void vhdl_evaluate_concurrent(VhdlArchitecture *arch) {
    for (int i = 0; i < arch->concurrent_count; i++) {
        VhdlConcurrentStmt *cs = &arch->concurrent_stmts[i];
        if (cs->target_signal >= 0 && cs->rhs_value && cs->target_signal < arch->signal_count) {
            arch->signals[cs->target_signal].resolved.drivers[0] = cs->rhs_value[0];
            arch->signals[cs->target_signal].resolved.driver_count = 1;
            arch->signals[cs->target_signal].has_event = true;
        }
    }
}

void vhdl_resolve_signals(VhdlArchitecture *arch) {
    for (int s = 0; s < arch->signal_count; s++) {
        VhdlSignal *sig = &arch->signals[s];
        if (sig->is_resolved && sig->resolved.driver_count > 0) {
            sig->resolved.value = vhdl_resolve_std_logic(sig->resolved.drivers, sig->resolved.driver_count);
        }
        if (sig->projected_count > 0 && sig->resolved.driver_count == 0) {
            sig->resolved.value = sig->projected[0];
            memmove(sig->projected, sig->projected + 1, (size_t)(sig->projected_count - 1) * sizeof(VhdlStdLogic));
            sig->projected_count--;
            sig->has_event = true;
        }
    }
}

void vhdl_run(VhdlSimulator *sim, uint64_t end_time) {
    sim->running = true;
    while (sim->current_time < end_time && sim->running) {
        for (int a = 0; a < sim->arch_count; a++) {
            vhdl_run_delta_cycle(sim, &sim->architectures[a]);
        }
        sim->current_time++;
    }
}

void vhdl_display_signals(const VhdlArchitecture *arch) {
    printf("Architecture: %s\n", arch->name);
    for (int s = 0; s < arch->signal_count; s++) {
        printf("  signal %s = %s\n", arch->signals[s].name,
               vhdl_std_logic_to_char(arch->signals[s].resolved.value));
    }
}

void vhdl_free(VhdlSimulator *sim) {
    for (int a = 0; a < sim->arch_count; a++) {
        VhdlArchitecture *arch = &sim->architectures[a];
        for (int p = 0; p < arch->process_count; p++) {
            VhdlProcess *proc = &arch->processes[p];
            for (int v = 0; v < proc->var_count; v++) {
                free(proc->variables[v].value);
            }
            for (int s = 0; s < proc->stmt_count; s++) {
                free(proc->stmts[s].rhs_value);
            }
        }
        for (int c = 0; c < arch->concurrent_count; c++) {
            free(arch->concurrent_stmts[c].rhs_value);
        }
    }
    memset(sim, 0, sizeof(*sim));
}
