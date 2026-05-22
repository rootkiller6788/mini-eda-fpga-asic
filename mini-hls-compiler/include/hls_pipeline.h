#ifndef HLS_PIPELINE_H
#define HLS_PIPELINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum {
    HLS_OP_ADD, HLS_OP_SUB, HLS_OP_MUL, HLS_OP_DIV,
    HLS_OP_AND, HLS_OP_OR, HLS_OP_XOR, HLS_OP_NOT,
    HLS_OP_SHL, HLS_OP_SHR, HLS_OP_SEL, HLS_OP_CMP,
    HLS_OP_LD, HLS_OP_ST, HLS_OP_PHI, HLS_OP_CALL,
    HLS_OP_RET, HLS_OP_BR, HLS_OP_CONST, HLS_OP_NOP
} HlsOpType;

typedef enum {
    HLS_SCHED_ASAP,
    HLS_SCHED_ALAP,
    HLS_SCHED_LIST,
    HLS_SCHED_FORCE_DIRECTED
} HlsSchedAlgo;

typedef enum {
    HLS_RES_ALU, HLS_RES_MUL, HLS_RES_DIV,
    HLS_RES_MEM_RD, HLS_RES_MEM_WR, HLS_RES_BRAM,
    HLS_RES_DSP, HLS_RES_LUT, HLS_RES_FF
} HlsResType;

typedef struct {
    uint32_t stage_id;
    uint32_t latency;
    uint32_t init_interval;
    bool     stallable;
    bool     flushable;
} HlsPipelineStage;

typedef struct hls_node {
    uint32_t          node_id;
    HlsOpType         op;
    char              name[64];
    uint32_t          asap_time;
    uint32_t          alap_time;
    uint32_t          scheduled_cycle;
    uint32_t          latency;
    struct hls_node **inputs;
    uint32_t          num_inputs;
    struct hls_node **outputs;
    uint32_t          num_outputs;
    bool              is_scheduled;
    int32_t           bound_unit;
} HlsNode;

typedef struct hls_bb {
    uint32_t         bb_id;
    HlsNode        **nodes;
    uint32_t         num_nodes;
    struct hls_bb  **predecessors;
    uint32_t         num_preds;
    struct hls_bb  **successors;
    uint32_t         num_succs;
    uint32_t         schedule_start;
    uint32_t         schedule_end;
} HlsBasicBlock;

typedef struct {
    HlsNode       **nodes;
    uint32_t        num_nodes;
    HlsBasicBlock **blocks;
    uint32_t        num_blocks;
} HlsDataFlowGraph;

typedef struct {
    uint32_t          ii;
    uint32_t          num_stages;
    HlsPipelineStage *stages;
    uint32_t          depth;
    bool              enable_flush;
    bool              enable_stall;
    uint32_t          stall_latency;
} HlsPipelineConfig;

typedef struct {
    char             name[128];
    HlsPipelineConfig pipeline;
    HlsDataFlowGraph *dfg;
    uint32_t         total_latency;
    uint32_t         throughput;
    uint32_t         num_resources[8];
} HlsRTLModule;

typedef struct {
    uint32_t total_cycles;
    uint32_t critical_path;
    double   utilization;
    bool     feasible;
    char     error_msg[256];
} HlsScheduleResult;

HlsDataFlowGraph* hls_dfg_create(void);
void              hls_dfg_destroy(HlsDataFlowGraph *dfg);
HlsNode*          hls_dfg_add_node(HlsDataFlowGraph *dfg, HlsOpType op,
                    const char *name);
void              hls_dfg_add_edge(HlsNode *src, HlsNode *dst);
HlsBasicBlock*    hls_dfg_add_block(HlsDataFlowGraph *dfg);
void              hls_dfg_add_edge_bb(HlsBasicBlock *src, HlsBasicBlock *dst);

HlsScheduleResult hls_schedule_asap(HlsDataFlowGraph *dfg);
HlsScheduleResult hls_schedule_alap(HlsDataFlowGraph *dfg,
                    uint32_t max_cycles);
HlsScheduleResult hls_schedule_list(HlsDataFlowGraph *dfg,
                    uint32_t *res_limits);

bool hls_bind_resources(HlsDataFlowGraph *dfg, HlsScheduleResult *sched);

HlsPipelineConfig hls_pipeline_create(uint32_t ii, uint32_t num_stages);
void hls_pipeline_stall_insert(HlsPipelineConfig *cfg, uint32_t stage);
void hls_pipeline_flush_enable(HlsPipelineConfig *cfg, bool en);
bool hls_pipeline_verify(HlsPipelineConfig *cfg, HlsScheduleResult *sched);

HlsRTLModule* hls_generate_rtl(HlsDataFlowGraph *dfg,
                HlsPipelineConfig *cfg);
void          hls_rtl_destroy(HlsRTLModule *mod);
void          hls_rtl_print_verilog(HlsRTLModule *mod, FILE *out);

void hls_stall_condition_set(HlsPipelineConfig *cfg, uint32_t stage,
       bool (*cond)(void*), void *ctx);
void hls_flush_trigger_set(HlsPipelineConfig *cfg,
       bool (*cond)(void*), void *ctx);
bool hls_pipeline_is_stalled(HlsPipelineConfig *cfg);
void hls_pipeline_advance(HlsPipelineConfig *cfg, uint32_t cycles);

#endif
