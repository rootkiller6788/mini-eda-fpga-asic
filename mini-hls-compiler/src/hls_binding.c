#include "hls_binding.h"
#include "hls_dfg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Left-edge algorithm for register binding based on lifetime intervals */
bool bind_register_lifetime(DataFlowGraph *dfg, BindingResult *result) {
    if (!dfg || !result || dfg->node_count == 0) return false;
    result->reg_count = 0;
    /* Compute lifetimes */ int first_use[MAX_DFG_NODES], last_use[MAX_DFG_NODES];
    for (int i = 0; i < dfg->node_count; i++) { first_use[i] = dfg->nodes[i].schedule_cycle; last_use[i] = dfg->nodes[i].schedule_cycle + 1; }
    for (int i = 0; i < dfg->edge_count; i++) {
        int src = dfg->edges[i].from, dst = dfg->edges[i].to;
        int birth = dfg->nodes[src].schedule_cycle;
        int death = dfg->nodes[dst].schedule_cycle;
        if (death > last_use[src]) last_use[src] = death;
        if (birth < first_use[src]) first_use[src] = birth;
    }
    /* Left-edge: sort intervals by start time, then allocate register per node */
    int sorted[MAX_DFG_NODES];
    for (int i = 0; i < dfg->node_count; i++) sorted[i] = i;
    for (int i = 0; i < dfg->node_count - 1; i++)
        for (int j = i + 1; j < dfg->node_count; j++)
            if (first_use[sorted[i]] > first_use[sorted[j]]) { int t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t; }
    /* Greedy register allocation */ bool *reg_free_at = (bool*)calloc(MAX_REGISTERS * 256, sizeof(bool));
    for (int i = 0; i < dfg->node_count; i++) {
        int ni = sorted[i];
        /* Skip nodes that don't produce values (stores, phi, cmp) */
        if (dfg->nodes[ni].op == HLS_STORE || dfg->nodes[ni].op == HLS_PHI || dfg->nodes[ni].op == HLS_CMP) continue;
        /* Find a free register */ int reg = -1;
        for (int r = 0; r < MAX_REGISTERS; r++) {
            bool conflict = false;
            for (int c = first_use[ni]; c <= last_use[ni]; c++)
                if (c < 256 && reg_free_at[r * 256 + c]) { conflict = true; break; }
            if (!conflict && result->reg_count > r) { reg = r; break; }
        }
        /* If no free register, allocate new one */
        if (reg == -1 && result->reg_count < MAX_REGISTERS) reg = result->reg_count++;
        if (reg >= 0 && reg < MAX_REGISTERS) {
            for (int c = first_use[ni]; c <= last_use[ni] && c < 256; c++) reg_free_at[reg * 256 + c] = true;
            if (reg >= result->reg_count) result->reg_count = reg + 1;
            result->regs[reg].id = reg; result->regs[reg].bound_node = ni;
            result->regs[reg].from_cycle = first_use[ni]; result->regs[reg].to_cycle = last_use[ni];
            result->regs[reg].shared = (last_use[ni] - first_use[ni] > 1);
        }
    }
    free(reg_free_at);
    return true;
}

/* Bind functional units to operations */
bool bind_functional_unit(DataFlowGraph *dfg, BindingResult *result) {
    if (!dfg || !result) return false;
    result->fu_count = 0;
    for (int i = 0; i < dfg->node_count && result->fu_count < MAX_FU_INSTANCES; i++) {
        DfgNode *n = &dfg->nodes[i];
        if (n->op == HLS_LOAD || n->op == HLS_STORE) continue; /* skip memory ops for FU binding */
        FuBind *fb = &result->fus[result->fu_count];
        fb->id = result->fu_count; fb->bound_node = i; fb->op = (int)n->op; fb->cycle = n->schedule_cycle;
        result->fu_count++;
    }
    return true;
}

int bind_register_count(BindingResult *r) { return r ? r->reg_count : 0; }

void bind_print(BindingResult *r) {
    if (!r) return;
    printf("=== Binding Result ===\n");
    printf("  Registers: %d\n", r->reg_count);
    for (int i = 0; i < r->reg_count; i++) printf("    r%d -> n%d [%d..%d]%s\n", r->regs[i].id, r->regs[i].bound_node, r->regs[i].from_cycle, r->regs[i].to_cycle, r->regs[i].shared ? " (shared)" : "");
    printf("  Functional Units: %d\n", r->fu_count);
    for (int i = 0; i < r->fu_count; i++) printf("    fu%d -> n%d op=%d @cycle %d\n", r->fus[i].id, r->fus[i].bound_node, r->fus[i].op, r->fus[i].cycle);
}
