#include <stdio.h>
#include <stdlib.h>
#include "hls_pipeline.h"
#include "loop_optimize.h"

int main(void)
{
    printf("=== mini-hls-compiler: Pipeline Example ===\n\n");

    HlsDataFlowGraph *dfg = hls_dfg_create();

    HlsNode *a  = hls_dfg_add_node(dfg, HLS_OP_LD, "load_a");
    HlsNode *b  = hls_dfg_add_node(dfg, HLS_OP_LD, "load_b");
    HlsNode *c  = hls_dfg_add_node(dfg, HLS_OP_LD, "load_c");
    HlsNode *m1 = hls_dfg_add_node(dfg, HLS_OP_MUL, "mul_ab");
    HlsNode *m2 = hls_dfg_add_node(dfg, HLS_OP_MUL, "mul_bc");
    HlsNode *add = hls_dfg_add_node(dfg, HLS_OP_ADD, "sum");
    HlsNode *st = hls_dfg_add_node(dfg, HLS_OP_ST, "store");

    hls_dfg_add_edge(a, m1);
    hls_dfg_add_edge(b, m1);
    hls_dfg_add_edge(b, m2);
    hls_dfg_add_edge(c, m2);
    hls_dfg_add_edge(m1, add);
    hls_dfg_add_edge(m2, add);
    hls_dfg_add_edge(add, st);

    printf("DFG created: %u nodes\n", dfg->num_nodes);

    HlsScheduleResult asap = hls_schedule_asap(dfg);
    printf("ASAP schedule: %u cycles, critical path=%u\n",
        asap.total_cycles, asap.critical_path);

    HlsScheduleResult alap = hls_schedule_alap(dfg, asap.total_cycles);
    printf("ALAP schedule: %u cycles, slack max=%u\n",
        alap.total_cycles, alap.critical_path);

    for (uint32_t i = 0; i < dfg->num_nodes; i++) {
        HlsNode *n = dfg->nodes[i];
        printf("  Node[%u] \"%s\": asap=%u alap=%u sched=%u slack=%u\n",
            n->node_id, n->name,
            n->asap_time, n->alap_time,
            n->scheduled_cycle,
            n->alap_time - n->asap_time);
    }

    uint32_t res_limits[8] = {2, 1, 1, 1, 1, 1, 1, 1};
    HlsScheduleResult list = hls_schedule_list(dfg, res_limits);
    printf("\nList schedule: %u cycles (res-constrained)\n",
        list.total_cycles);

    hls_bind_resources(dfg, &asap);

    HlsPipelineConfig pipe = hls_pipeline_create(1, 3);
    hls_pipeline_stall_insert(&pipe, 1);
    hls_pipeline_flush_enable(&pipe, true);

    if (hls_pipeline_verify(&pipe, &asap))
        printf("\nPipeline verified: II=%u, %u stages\n",
            pipe.ii, pipe.num_stages);

    HlsRTLModule *rtl = hls_generate_rtl(dfg, &pipe);
    if (rtl) {
        printf("RTL generated: \"%s\", latency=%u, throughput=%u\n",
            rtl->name, rtl->total_latency, rtl->throughput);
        hls_rtl_print_verilog(rtl, stdout);
        hls_rtl_destroy(rtl);
    }

    free(pipe.stages);
    hls_dfg_destroy(dfg);

    printf("\nDone.\n");
    return 0;
}
