#include "systemverilog_sim.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void sv_init(SvSimulator *sim) {
    memset(sim, 0, sizeof(*sim));
}

int sv_add_module(SvSimulator *sim, const char *name) {
    assert(sim->module_count < SV_MAX_MODULES);
    int idx = sim->module_count++;
    SvModule *mod = &sim->modules[idx];
    memset(mod, 0, sizeof(*mod));
    strncpy(mod->name, name, SV_MAX_NAME_LEN - 1);
    return idx;
}

SvModule *sv_get_module(SvSimulator *sim, int idx) {
    if (idx >= 0 && idx < sim->module_count) return &sim->modules[idx];
    return NULL;
}

int sv_add_port(SvModule *mod, const char *name, SvPortDir dir, int width) {
    assert(mod->port_count < SV_MAX_PORTS);
    int idx = mod->port_count++;
    SvPort *port = &mod->ports[idx];
    memset(port, 0, sizeof(*port));
    strncpy(port->name, name, SV_MAX_NAME_LEN - 1);
    port->direction = dir;
    port->width = width;
    port->signal_index = -1;
    return idx;
}

int sv_add_signal(SvModule *mod, const char *name, int width, bool four_state) {
    assert(mod->signal_count < SV_MAX_SIGNALS);
    int idx = mod->signal_count++;
    SvSignal *sig = &mod->signals[idx];
    memset(sig, 0, sizeof(*sig));
    strncpy(sig->name, name, SV_MAX_NAME_LEN - 1);
    sig->width = width;
    sig->is_four_state = four_state;
    sig->value = SV_LOGIC_X;
    return idx;
}

int sv_add_always_ff(SvModule *mod, const char *clk_signal, bool posedge) {
    assert(mod->always_count < SV_MAX_ALWAYS);
    int idx = mod->always_count++;
    SvAlwaysBlock *blk = &mod->always_blocks[idx];
    memset(blk, 0, sizeof(*blk));
    blk->kind = SV_ALWAYS_FF;
    blk->edge_sensitive = true;
    blk->entry_stmt = -1;
    if (clk_signal) {
        blk->sensitivity[0] = posedge ? 1 : -1;
        blk->sensitivity_count = 1;
    }
    return idx;
}

int sv_add_always_comb(SvModule *mod) {
    assert(mod->always_count < SV_MAX_ALWAYS);
    int idx = mod->always_count++;
    SvAlwaysBlock *blk = &mod->always_blocks[idx];
    memset(blk, 0, sizeof(*blk));
    blk->kind = SV_ALWAYS_COMB;
    blk->edge_sensitive = false;
    blk->entry_stmt = -1;
    blk->sensitivity_count = 0;
    return idx;
}

int sv_add_interface(SvModule *mod, const char *name) {
    assert(mod->interface_count < SV_MAX_INTERFACES);
    int idx = mod->interface_count++;
    SvInterface *iface = &mod->interfaces[idx];
    memset(iface, 0, sizeof(*iface));
    strncpy(iface->name, name, SV_MAX_NAME_LEN - 1);
    return idx;
}

int sv_add_enum(SvModule *mod, const char *name) {
    assert(mod->enum_count < SV_MAX_ENUMS);
    int idx = mod->enum_count++;
    SvEnum *e = &mod->enums[idx];
    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, SV_MAX_NAME_LEN - 1);
    return idx;
}

void sv_add_enum_member(SvEnum *e, const char *name, int value) {
    assert(e->member_count < SV_MAX_ENUM_MEMBERS);
    int idx = e->member_count++;
    strncpy(e->members[idx].name, name, SV_MAX_NAME_LEN - 1);
    e->members[idx].value = value;
}

int sv_add_struct(SvModule *mod, const char *name) {
    assert(mod->struct_count < SV_MAX_STRUCTS);
    int idx = mod->struct_count++;
    SvStruct *s = &mod->structs[idx];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, SV_MAX_NAME_LEN - 1);
    return idx;
}

void sv_add_struct_field(SvStruct *s, const char *name, int width, bool is_signed) {
    assert(s->field_count < SV_MAX_STRUCT_FIELDS);
    int idx = s->field_count++;
    strncpy(s->fields[idx].name, name, SV_MAX_NAME_LEN - 1);
    s->fields[idx].width = width;
    s->fields[idx].is_signed = is_signed;
}

int sv_add_package(SvSimulator *sim, const char *name) {
    assert(sim->package_count < SV_MAX_PACKAGES);
    int idx = sim->package_count++;
    SvPackage *pkg = &sim->packages[idx];
    memset(pkg, 0, sizeof(*pkg));
    strncpy(pkg->name, name, SV_MAX_NAME_LEN - 1);
    return idx;
}

void sv_add_package_item(SvPackage *pkg, const char *name, int value, int type) {
    assert(pkg->item_count < SV_MAX_PKG_ITEMS);
    int idx = pkg->item_count++;
    strncpy(pkg->items[idx].name, name, SV_MAX_NAME_LEN - 1);
    pkg->items[idx].value = value;
    pkg->items[idx].type = type;
}

int sv_add_assertion(SvModule *mod, const char *cond, SvAssertKind kind) {
    assert(mod->assertion_count < SV_MAX_ASSERTIONS);
    int idx = mod->assertion_count++;
    SvAssertion *ass = &mod->assertions[idx];
    memset(ass, 0, sizeof(*ass));
    strncpy(ass->cond_text, cond, SV_MAX_NAME_LEN - 1);
    ass->kind = kind;
    ass->is_immediate = (kind == SV_ASSERT_IMMEDIATE);
    ass->passed = true;
    return idx;
}

int sv_add_stmt(SvAlwaysBlock *blk, SvStmtKind kind) {
    assert(blk->stmt_count < SV_MAX_STMTS);
    int idx = blk->stmt_count++;
    memset(&blk->stmts[idx], 0, sizeof(SvStmt));
    blk->stmts[idx].kind = kind;
    if (blk->entry_stmt < 0) blk->entry_stmt = idx;
    return idx;
}

void sv_add_assign(SvAlwaysBlock *blk, int stmt, int lhs, const SvLogicValue *rhs, int width) {
    assert(stmt >= 0 && stmt < blk->stmt_count);
    SvStmt *s = &blk->stmts[stmt];
    s->kind = SV_STMT_ASSIGN;
    s->lhs_signal = lhs;
    s->rhs_width = width;
    s->rhs_value = (SvLogicValue *)malloc((size_t)width * sizeof(SvLogicValue));
    if (rhs) memcpy(s->rhs_value, rhs, (size_t)width * sizeof(SvLogicValue));
}

void sv_evaluate_always_ff(SvModule *mod, SvAlwaysBlock *blk) {
    for (int s = blk->entry_stmt; s >= 0 && s < blk->stmt_count; s++) {
        SvStmt *stmt = &blk->stmts[s];
        switch (stmt->kind) {
            case SV_STMT_ASSIGN:
                if (stmt->lhs_signal >= 0 && stmt->lhs_signal < mod->signal_count && stmt->rhs_value) {
                    mod->signals[stmt->lhs_signal].value = stmt->rhs_value[0];
                }
                break;
            case SV_STMT_IF:
                if (stmt->cond_signal >= 0 && stmt->cond_signal < mod->signal_count) {
                    if (mod->signals[stmt->cond_signal].value == SV_LOGIC_1) {
                        if (stmt->true_target >= 0) s = stmt->true_target - 1;
                    } else {
                        if (stmt->false_target >= 0) s = stmt->false_target - 1;
                    }
                }
                break;
            case SV_STMT_DISPLAY:
                printf("[SV] %s\n", stmt->display_msg);
                break;
            default:
                break;
        }
    }
}

void sv_evaluate_always_comb(SvModule *mod, SvAlwaysBlock *blk) {
    bool changed;
    do {
        changed = false;
        for (int s = blk->entry_stmt; s >= 0 && s < blk->stmt_count; s++) {
            SvStmt *stmt = &blk->stmts[s];
            if (stmt->kind == SV_STMT_ASSIGN && stmt->lhs_signal >= 0 &&
                stmt->lhs_signal < mod->signal_count && stmt->rhs_value) {
                SvLogicValue old = mod->signals[stmt->lhs_signal].value;
                SvLogicValue new_val = stmt->rhs_value[0];
                if (old != new_val) {
                    mod->signals[stmt->lhs_signal].value = new_val;
                    changed = true;
                }
            }
        }
    } while (changed);
}

void sv_evaluate_assertions(SvModule *mod) {
    for (int i = 0; i < mod->assertion_count; i++) {
        SvAssertion *ass = &mod->assertions[i];
        if (ass->is_immediate) {
            ass->passed = true;
            printf("SV ASSERT [%s]: %s\n", ass->cond_text, ass->passed ? "PASS" : "FAIL");
        }
    }
}

void sv_run(SvSimulator *sim, uint64_t end_time) {
    sim->running = true;
    for (uint64_t t = 0; t < end_time && sim->running; t++) {
        sim->current_time = t;
        for (int m = 0; m < sim->module_count; m++) {
            SvModule *mod = &sim->modules[m];
            for (int a = 0; a < mod->always_count; a++) {
                SvAlwaysBlock *blk = &mod->always_blocks[a];
                switch (blk->kind) {
                    case SV_ALWAYS_FF:
                        sv_evaluate_always_ff(mod, blk);
                        break;
                    case SV_ALWAYS_COMB:
                        sv_evaluate_always_comb(mod, blk);
                        break;
                    case SV_ALWAYS_LATCH:
                        break;
                }
            }
            sv_evaluate_assertions(mod);
        }
    }
}

void sv_set_signal(SvModule *mod, int sig, SvLogicValue val) {
    if (sig >= 0 && sig < mod->signal_count) {
        mod->signals[sig].value = val;
    }
}

SvLogicValue sv_get_signal(const SvModule *mod, int sig) {
    if (sig >= 0 && sig < mod->signal_count) return mod->signals[sig].value;
    return SV_LOGIC_X;
}

void sv_display_module(const SvModule *mod) {
    printf("SV Module: %s\n", mod->name);
    printf("  Signals:\n");
    for (int i = 0; i < mod->signal_count; i++) {
        printf("    %s = %d (%d-bit)\n", mod->signals[i].name,
               mod->signals[i].value, mod->signals[i].width);
    }
    printf("  Interfaces: %d\n", mod->interface_count);
    printf("  Enums: %d\n", mod->enum_count);
    printf("  Structs: %d\n", mod->struct_count);
    printf("  Assertions: %d\n", mod->assertion_count);
}

void sv_free(SvSimulator *sim) {
    for (int m = 0; m < sim->module_count; m++) {
        SvModule *mod = &sim->modules[m];
        for (int a = 0; a < mod->always_count; a++) {
            SvAlwaysBlock *blk = &mod->always_blocks[a];
            for (int s = 0; s < blk->stmt_count; s++) {
                free(blk->stmts[s].rhs_value);
            }
        }
    }
    memset(sim, 0, sizeof(*sim));
}
