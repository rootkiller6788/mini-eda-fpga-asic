#include "hls_fsmd.h"
#include "hls_dfg.h"
#include "hls_binding.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

void fsmd_init(FsmdController *c, DataFlowGraph *dfg) {
    c->state_count = 0; c->start_state = 0; c->dfg = dfg;
    memset(c->states, 0, sizeof(c->states));
}

int fsmd_create_state(FsmdController *c, const char *name) {
    if (c->state_count >= MAX_STATES) return -1;
    FsmdState *s = &c->states[c->state_count];
    s->id = c->state_count; s->op_count = 0; s->next_state = -1; s->alt_state = -1; s->branch = false;
    strncpy(s->name, name[0] ? name : "state", 31); s->name[31] = '\0';
    return c->state_count++;
}

/* Generate FSMD from scheduled DFG: one state per cycle */
bool fsmd_generate(FsmdController *c) {
    if (!c || !c->dfg || c->dfg->node_count == 0) return false;
    fsmd_init(c, c->dfg);
    /* Find max cycle */ int max_cycle = 0;
    for (int i = 0; i < c->dfg->node_count; i++) if (c->dfg->nodes[i].schedule_cycle > max_cycle) max_cycle = c->dfg->nodes[i].schedule_cycle;
    /* Create one state per cycle */
    for (int cycle = 0; cycle <= max_cycle; cycle++) {
        char sname[32]; sprintf(sname, "S%d", cycle);
        int sid = fsmd_create_state(c, sname);
        if (sid < 0) return false;
        FsmdState *st = &c->states[sid];
        st->next_state = sid + 1;
        /* Assign operations scheduled in this cycle to this state */
        for (int i = 0; i < c->dfg->node_count && st->op_count < MAX_DFG_NODES; i++) {
            if (c->dfg->nodes[i].schedule_cycle == cycle) st->operations[st->op_count++] = i;
        }
    }
    /* Last state transitions to itself (idle) */
    if (c->state_count > 0) c->states[c->state_count - 1].next_state = c->state_count - 1;
    /* Insert branch states for CMP/PHI nodes */
    for (int i = 0; i < c->dfg->node_count; i++) {
        if (c->dfg->nodes[i].op == HLS_CMP) {
            int cmp_cycle = c->dfg->nodes[i].schedule_cycle;
            if (cmp_cycle >= 0 && cmp_cycle < c->state_count) {
                /* Split: create a branch at this state */
                FsmdState *st = &c->states[cmp_cycle];
                /* Create alternate target (next cycle + 1 for skip) */
                if (c->state_count < MAX_STATES - 1) {
                    st->branch = true;
                    st->alt_state = cmp_cycle + 2; /* skip next state on branch taken */
                    if (st->alt_state >= c->state_count) st->alt_state = c->state_count - 1;
                }
            }
        }
    }
    return true;
}

static const char *hls_op_name(HlsOp op) {
    switch (op) { case HLS_ADD: return "add"; case HLS_SUB: return "sub"; case HLS_MUL: return "mul"; case HLS_DIV: return "div"; case HLS_LOAD: return "load"; case HLS_STORE: return "store"; case HLS_PHI: return "mux"; case HLS_CMP: return "cmp"; default: return "nop"; }
}

void fsmd_emit_verilog(FsmdController *c) {
    if (!c || c->state_count == 0) return;
    printf("// Auto-generated FSMD Verilog\n");
    printf("module fsmd_top(\n  input clk, input rst_n,\n");
    /* Gather inputs/outputs from DFG */ int in_count = 0, out_count = 0, input_nodes[8], output_nodes[8];
    for (int i = 0; i < c->dfg->node_count; i++) {
        if (c->dfg->nodes[i].op == HLS_LOAD && in_count < 8) { input_nodes[in_count++] = i; }
        if (c->dfg->nodes[i].op == HLS_STORE && out_count < 8) { output_nodes[out_count++] = i; }
    }
    for (int i = 0; i < in_count; i++) printf("  input [31:0] %s,\n", c->dfg->nodes[input_nodes[i]].name);
    for (int i = 0; i < out_count; i++) printf("  output reg [31:0] %s%s\n", c->dfg->nodes[output_nodes[i]].name, i < out_count - 1 ? "," : "");
    printf(");\n\n");
    printf("  reg [7:0] state, next_state;\n");
    printf("  localparam"); for (int i = 0; i < c->state_count; i++) printf(" %s = 8'd%d%s", c->states[i].name, i, i < c->state_count - 1 ? "," : ";\n");
    /* Internal registers */
    printf("  reg [31:0] reg_file[0:%d];\n", MAX_REGISTERS - 1);
    printf("  wire [31:0] alu_out, mul_out;\n\n");
    printf("  always @(posedge clk or negedge rst_n) begin\n");
    printf("    if (!rst_n) state <= %s;\n", c->states[c->start_state].name);
    printf("    else state <= next_state;\n  end\n\n");
    printf("  always @(*) begin\n    next_state = state;\n    case (state)\n");
    for (int i = 0; i < c->state_count; i++) {
        FsmdState *s = &c->states[i];
        printf("      %s: begin\n", s->name);
        /* Emit operations for this state */
        for (int j = 0; j < s->op_count; j++) {
            int ni = s->operations[j]; DfgNode *n = &c->dfg->nodes[ni];
            if (n->op == HLS_LOAD) { printf("        // %s <= input port\n", n->name); }
            else if (n->op == HLS_STORE) { printf("        // %s <= output port\n", n->name); }
            else { printf("        // %s <- %s\n", n->name, hls_op_name(n->op)); }
        }
        if (s->branch) printf("        if (cond) next_state = %s; else next_state = %s;\n", c->states[s->alt_state].name, c->states[s->next_state].name);
        else printf("        next_state = %s;\n", c->states[s->next_state].name);
        printf("      end\n");
    }
    printf("      default: next_state = %s;\n    endcase\n  end\n", c->states[c->start_state].name);
    printf("endmodule\n");
}

void fsmd_print(FsmdController *c) {
    if (!c) return;
    printf("=== FSMD Controller ===\n");
    printf("  States: %d, Start: %d\n", c->state_count, c->start_state);
    for (int i = 0; i < c->state_count; i++) {
        FsmdState *s = &c->states[i];
        printf("  [%s] ops=%d next=%d", s->name, s->op_count, s->next_state);
        if (s->branch) printf(" alt=%d", s->alt_state);
        printf(" ops=[");
        for (int j = 0; j < s->op_count; j++) printf("%s%d", j ? "," : "", s->operations[j]);
        printf("]\n");
    }
}
