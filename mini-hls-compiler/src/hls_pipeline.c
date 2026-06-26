#include "hls_pipeline.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define HLS_MAX_NODES   1024
#define HLS_MAX_BLOCKS  256

static uint32_t dfg_node_counter = 0;
static uint32_t dfg_block_counter = 0;

HlsDataFlowGraph* hls_dfg_create(void)
{
    HlsDataFlowGraph *dfg = calloc(1, sizeof(HlsDataFlowGraph));
    if (!dfg) return NULL;
    dfg->nodes = calloc(HLS_MAX_NODES, sizeof(HlsNode*));
    if (!dfg->nodes) { free(dfg); return NULL; }
    dfg->blocks = calloc(HLS_MAX_BLOCKS, sizeof(HlsBasicBlock*));
    if (!dfg->blocks) { free(dfg->nodes); free(dfg); return NULL; }
    return dfg;
}

void hls_dfg_destroy(HlsDataFlowGraph *dfg)
{
    if (!dfg) return;
    for (uint32_t i = 0; i < dfg->num_nodes; i++) {
        HlsNode *n = dfg->nodes[i];
        if (n) { free(n->inputs); free(n->outputs); free(n); }
    }
    for (uint32_t i = 0; i < dfg->num_blocks; i++) {
        HlsBasicBlock *b = dfg->blocks[i];
        if (b) { free(b->nodes); free(b->predecessors);
                 free(b->successors); free(b); }
    }
    free(dfg->nodes);
    free(dfg->blocks);
    free(dfg);
}

HlsNode* hls_dfg_add_node(HlsDataFlowGraph *dfg, HlsOpType op,
            const char *name)
{
    if (!dfg || dfg->num_nodes >= HLS_MAX_NODES) return NULL;
    HlsNode *n = calloc(1, sizeof(HlsNode));
    if (!n) return NULL;
    n->node_id = dfg_node_counter++;
    n->op = op;
    strncpy(n->name, name ? name : "", sizeof(n->name) - 1);
    n->latency = (op == HLS_OP_MUL || op == HLS_OP_DIV) ? 3 : 1;
    n->bound_unit = -1;
    n->inputs  = calloc(8, sizeof(HlsNode*));
    n->outputs = calloc(8, sizeof(HlsNode*));
    if (!n->inputs || !n->outputs) {
        free(n->inputs); free(n->outputs); free(n); return NULL;
    }
    dfg->nodes[dfg->num_nodes++] = n;
    return n;
}

void hls_dfg_add_edge(HlsNode *src, HlsNode *dst)
{
    if (!src || !dst) return;
    if (src->num_outputs < 8)
        src->outputs[src->num_outputs++] = dst;
    if (dst->num_inputs < 8)
        dst->inputs[dst->num_inputs++] = src;
}

HlsBasicBlock* hls_dfg_add_block(HlsDataFlowGraph *dfg)
{
    if (!dfg || dfg->num_blocks >= HLS_MAX_BLOCKS) return NULL;
    HlsBasicBlock *bb = calloc(1, sizeof(HlsBasicBlock));
    if (!bb) return NULL;
    bb->bb_id = dfg_block_counter++;
    bb->nodes = calloc(HLS_MAX_NODES, sizeof(HlsNode*));
    bb->predecessors = calloc(8, sizeof(HlsBasicBlock*));
    bb->successors = calloc(8, sizeof(HlsBasicBlock*));
    if (!bb->nodes || !bb->predecessors || !bb->successors) {
        free(bb->nodes); free(bb->predecessors);
        free(bb->successors); free(bb); return NULL;
    }
    dfg->blocks[dfg->num_blocks++] = bb;
    return bb;
}

void hls_dfg_add_edge_bb(HlsBasicBlock *src, HlsBasicBlock *dst)
{
    if (!src || !dst) return;
    if (src->num_succs < 8)
        src->successors[src->num_succs++] = dst;
    if (dst->num_preds < 8)
        dst->predecessors[dst->num_preds++] = src;
}

HlsScheduleResult hls_schedule_asap(HlsDataFlowGraph *dfg)
{
    HlsScheduleResult r = {0, 0, 0.0, false, ""};
    if (!dfg || dfg->num_nodes == 0) {
        strncpy(r.error_msg, "Empty or null DFG", sizeof(r.error_msg)-1);
        return r;
    }
    for (uint32_t i = 0; i < dfg->num_nodes; i++) {
        HlsNode *n = dfg->nodes[i];
        uint32_t max_pred_time = 0;
        for (uint32_t j = 0; j < n->num_inputs; j++) {
            HlsNode *pred = n->inputs[j];
            uint32_t pred_end = pred->asap_time + pred->latency;
            if (pred_end > max_pred_time) max_pred_time = pred_end;
        }
        n->asap_time = max_pred_time;
        n->scheduled_cycle = n->asap_time;
        n->is_scheduled = true;
        if (n->asap_time + n->latency > r.total_cycles)
            r.total_cycles = n->asap_time + n->latency;
    }
    r.critical_path = r.total_cycles;
    r.feasible = true;
    r.utilization = (dfg->num_nodes > 0)
        ? (double)dfg->num_nodes / (double)r.total_cycles : 0.0;
    return r;
}

HlsScheduleResult hls_schedule_alap(HlsDataFlowGraph *dfg,
        uint32_t max_cycles)
{
    HlsScheduleResult r = {max_cycles, 0, 0.0, false, ""};
    if (!dfg || dfg->num_nodes == 0) {
        strncpy(r.error_msg, "Empty or null DFG", sizeof(r.error_msg)-1);
        return r;
    }
    for (uint32_t i = 0; i < dfg->num_nodes; i++)
        dfg->nodes[i]->alap_time = max_cycles;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int32_t i = (int32_t)dfg->num_nodes - 1; i >= 0; i--) {
            HlsNode *n = dfg->nodes[i];
            uint32_t min_succ = max_cycles;
            for (uint32_t j = 0; j < n->num_outputs; j++) {
                HlsNode *s = n->outputs[j];
                if (s->alap_time < min_succ)
                    min_succ = s->alap_time;
            }
            uint32_t new_alap = (min_succ >= n->latency)
                ? min_succ - n->latency : 0;
            if (new_alap < n->alap_time) {
                n->alap_time = new_alap;
                changed = true;
            }
        }
    }
    for (uint32_t i = 0; i < dfg->num_nodes; i++) {
        HlsNode *n = dfg->nodes[i];
        uint32_t slack = n->alap_time - n->asap_time;
        if (slack > r.critical_path) r.critical_path = slack;
    }
    r.feasible = true;
    return r;
}

HlsScheduleResult hls_schedule_list(HlsDataFlowGraph *dfg,
        uint32_t *res_limits)
{
    HlsScheduleResult r = {0, 0, 0.0, false, ""};
    uint32_t res_used[8] = {0};
    if (!res_limits) {
        strncpy(r.error_msg, "Null resource limits",
               sizeof(r.error_msg)-1);
        return r;
    }
    for (uint32_t i = 0; i < dfg->num_nodes; i++) {
        HlsNode *n = dfg->nodes[i];
        uint32_t ready_cycle = 0;
        for (uint32_t j = 0; j < n->num_inputs; j++) {
            HlsNode *pred = n->inputs[j];
            uint32_t pred_end = pred->scheduled_cycle + pred->latency;
            if (pred_end > ready_cycle) ready_cycle = pred_end;
        }
        HlsResType rt = HLS_RES_ALU;
        switch (n->op) {
            case HLS_OP_MUL: rt = HLS_RES_MUL; break;
            case HLS_OP_DIV: rt = HLS_RES_DIV; break;
            case HLS_OP_LD:  rt = HLS_RES_MEM_RD; break;
            case HLS_OP_ST:  rt = HLS_RES_MEM_WR; break;
            default: break;
        }
        uint32_t cycle = ready_cycle;
        while (cycle < 10000) {
            if (res_limits[rt] == 0 || res_used[rt] < res_limits[rt]) {
                res_used[rt]++;
                break;
            }
            cycle++;
        }
        n->scheduled_cycle = cycle;
        n->is_scheduled = true;
        if (cycle + n->latency > r.total_cycles)
            r.total_cycles = cycle + n->latency;
    }
    r.feasible = true;
    r.critical_path = r.total_cycles;
    return r;
}

bool hls_bind_resources(HlsDataFlowGraph *dfg, HlsScheduleResult *sched)
{
    if (!dfg || !sched || !sched->feasible) return false;
    for (uint32_t i = 0; i < dfg->num_nodes; i++) {
        HlsNode *n = dfg->nodes[i];
        HlsResType rt = HLS_RES_ALU;
        switch (n->op) {
            case HLS_OP_MUL: rt = HLS_RES_MUL; break;
            case HLS_OP_DIV: rt = HLS_RES_DIV; break;
            case HLS_OP_LD: case HLS_OP_ST: rt = HLS_RES_MEM_RD; break;
            default: break;
        }
        n->bound_unit = (int32_t)rt;
    }
    return true;
}

HlsPipelineConfig hls_pipeline_create(uint32_t ii, uint32_t num_stages)
{
    HlsPipelineConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.ii = ii;
    cfg.num_stages = num_stages;
    cfg.stages = calloc(num_stages, sizeof(HlsPipelineStage));
    if (cfg.stages) {
        for (uint32_t i = 0; i < num_stages; i++) {
            cfg.stages[i].stage_id = i;
            cfg.stages[i].latency = 1;
            cfg.stages[i].stallable = true;
        }
    }
    cfg.depth = num_stages;
    cfg.enable_stall = true;
    return cfg;
}

void hls_pipeline_stall_insert(HlsPipelineConfig *cfg, uint32_t stage)
{
    if (!cfg || stage >= cfg->num_stages) return;
    cfg->stages[stage].stallable = true;
    cfg->enable_stall = true;
}

void hls_pipeline_flush_enable(HlsPipelineConfig *cfg, bool en)
{
    if (!cfg) return;
    cfg->enable_flush = en;
    if (cfg->stages) {
        for (uint32_t i = 0; i < cfg->num_stages; i++)
            cfg->stages[i].flushable = en;
    }
}

bool hls_pipeline_verify(HlsPipelineConfig *cfg, HlsScheduleResult *sched)
{
    if (!cfg || !sched) return false;
    if (cfg->ii == 0) return false;
    if (cfg->num_stages == 0) return false;
    return sched->feasible;
}

HlsRTLModule* hls_generate_rtl(HlsDataFlowGraph *dfg,
        HlsPipelineConfig *cfg)
{
    HlsRTLModule *mod = calloc(1, sizeof(HlsRTLModule));
    if (!mod) return NULL;
    strncpy(mod->name, "hls_top", sizeof(mod->name) - 1);
    mod->dfg = dfg;
    if (cfg) {
        mod->pipeline = *cfg;
        mod->pipeline.stages = calloc(cfg->num_stages,
            sizeof(HlsPipelineStage));
        if (mod->pipeline.stages)
            memcpy(mod->pipeline.stages, cfg->stages,
                   cfg->num_stages * sizeof(HlsPipelineStage));
        mod->total_latency = cfg->num_stages;
        mod->throughput = (cfg->ii > 0) ? 1000000U / cfg->ii : 0;
    }
    memset(mod->num_resources, 0, sizeof(mod->num_resources));
    return mod;
}

void hls_rtl_destroy(HlsRTLModule *mod)
{
    if (!mod) return;
    free(mod->pipeline.stages);
    free(mod);
}

void hls_rtl_print_verilog(HlsRTLModule *mod, FILE *out)
{
    if (!mod || !out) return;
    fprintf(out, "// mini-hls-compiler generated RTL\n");
    fprintf(out, "module %s (\n", mod->name);
    fprintf(out, "  input  wire clk,\n");
    fprintf(out, "  input  wire rst_n\n");
    fprintf(out, ");\n\n");
    fprintf(out, "  // Pipeline: %u stages, II=%u, depth=%u\n",
        mod->pipeline.num_stages, mod->pipeline.ii, mod->pipeline.depth);
    fprintf(out, "  // Total latency: %u cycles\n", mod->total_latency);
    for (uint32_t i = 0; i < mod->pipeline.num_stages; i++) {
        fprintf(out, "  reg [31:0] stage_%u_data;\n", i);
    }
    fprintf(out, "\n  always @(posedge clk or negedge rst_n) begin\n");
    fprintf(out, "    if (!rst_n) begin\n");
    for (uint32_t i = 0; i < mod->pipeline.num_stages; i++)
        fprintf(out, "      stage_%u_data <= 32'd0;\n", i);
    fprintf(out, "    end else begin\n");
    for (uint32_t i = 1; i < mod->pipeline.num_stages; i++)
        fprintf(out, "      stage_%u_data <= stage_%u_data;\n", i, i-1);
    fprintf(out, "    end\n");
    fprintf(out, "  end\n");
    fprintf(out, "endmodule\n");
}

static bool (*g_stall_conds[8])(void*);
static void *g_stall_ctxs[8];
static bool (*g_flush_cond)(void*);
static void *g_flush_ctx;
static bool g_pipeline_stalled = false;

void hls_stall_condition_set(HlsPipelineConfig *cfg, uint32_t stage,
       bool (*cond)(void*), void *ctx)
{
    (void)cfg;
    if (stage < 8) { g_stall_conds[stage] = cond; g_stall_ctxs[stage] = ctx; }
}

void hls_flush_trigger_set(HlsPipelineConfig *cfg,
       bool (*cond)(void*), void *ctx)
{
    (void)cfg;
    g_flush_cond = cond; g_flush_ctx = ctx;
}

bool hls_pipeline_is_stalled(HlsPipelineConfig *cfg)
{
    (void)cfg;
    for (int i = 0; i < 8; i++)
        if (g_stall_conds[i] && g_stall_conds[i](g_stall_ctxs[i]))
            return true;
    return false;
}

void hls_pipeline_advance(HlsPipelineConfig *cfg, uint32_t cycles)
{
    if (!cfg) return;
    g_pipeline_stalled = false;
    for (uint32_t i = 0; i < cycles; i++) {
        if (hls_pipeline_is_stalled(cfg))
            g_pipeline_stalled = true;
    }
}
