#include "hls_allocate.h"
#include "hls_dfg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FuType map_op_to_fu(HlsOp op) {
    switch (op) {
        case HLS_ADD: case HLS_SUB: return FU_ADDER;
        case HLS_MUL:                return FU_MULTIPLIER;
        case HLS_DIV:                return FU_DIVIDER;
        case HLS_LOAD:               return FU_LOAD_UNIT;
        case HLS_STORE:              return FU_STORE_UNIT;
        default:                     return FU_ALU;
    }
}

static double fu_area(FuType t) {
    switch (t) { case FU_ADDER: return 1.0; case FU_MULTIPLIER: return 5.0; case FU_DIVIDER: return 8.0;
                 case FU_LOAD_UNIT: return 0.5; case FU_STORE_UNIT: return 0.5; case FU_ALU: return 1.5; default: return 1.0; }
}

static double fu_delay_val(FuType t) {
    switch (t) { case FU_ADDER: return 1.0; case FU_MULTIPLIER: return 3.0; case FU_DIVIDER: return 5.0;
                 case FU_LOAD_UNIT: return 1.0; case FU_STORE_UNIT: return 1.0; case FU_ALU: return 1.0; default: return 1.0; }
}

/* Greedy resource allocation: count max concurrent operations per FU type */
bool alloc_greedy(DataFlowGraph *dfg, AllocResult *result) {
    if (!dfg || !result || dfg->node_count == 0) return false;
    result->type_count = 6;
    for (int t = 0; t < 6; t++) { result->units[t].type = (FuType)t; result->units[t].count = 0; result->units[t].used = 0; result->units[t].area = fu_area((FuType)t); result->units[t].delay = fu_delay_val((FuType)t); }
    result->total_area = 0.0;
    /* Count max concurrent operations per cycle per type */
    int max_cycle = 0;
    for (int i = 0; i < dfg->node_count; i++) if (dfg->nodes[i].schedule_cycle > max_cycle) max_cycle = dfg->nodes[i].schedule_cycle;
    for (int c = 0; c <= max_cycle; c++) {
        int usage[6] = {0};
        for (int i = 0; i < dfg->node_count; i++) {
            if (dfg->nodes[i].schedule_cycle == c) {
                FuType ft = map_op_to_fu(dfg->nodes[i].op);
                if ((int)ft < 6) usage[(int)ft]++;
            }
        }
        for (int t = 0; t < 6; t++) if (usage[t] > result->units[t].count) result->units[t].count = usage[t];
    }
    for (int t = 0; t < 6; t++) result->total_area += result->units[t].count * result->units[t].area;
    return true;
}

/* Simple clique partition: group compatible operations by cycle non-overlap */
int alloc_clique_partition(DataFlowGraph *dfg, AllocResult *result) {
    if (!dfg || !result) return -1;
    alloc_greedy(dfg, result);
    /* Reduce count by checking if operations can share (non-overlapping cycles) */
    int *op_cycles = (int*)malloc((size_t)dfg->node_count * sizeof(int));
    FuType *op_fu = (FuType*)malloc((size_t)dfg->node_count * sizeof(FuType));
    if (!op_cycles || !op_fu) { free(op_cycles); free(op_fu); return -1; }
    for (int i = 0; i < dfg->node_count; i++) { op_cycles[i] = dfg->nodes[i].schedule_cycle; op_fu[i] = map_op_to_fu(dfg->nodes[i].op); }
    /* For each FU type, try clique-style sharing: if two ops don't overlap in time, they can share */
    int total_fus = 0;
    for (int t = 0; t < 6; t++) {
        int needed = 0;
        for (int cycle = 0; cycle < 64; cycle++) {
            int count = 0;
            for (int i = 0; i < dfg->node_count; i++) if (op_fu[i] == (FuType)t && op_cycles[i] == cycle) count++;
            if (count > needed) needed = count;
        }
        result->units[t].count = needed;
        total_fus += needed;
    }
    free(op_cycles); free(op_fu);
    return total_fus;
}

/* Lifetime analysis: compute first and last use of each node's output */
void alloc_lifetime_analysis(DataFlowGraph *dfg, int *first_use, int *last_use) {
    if (!dfg || !first_use || !last_use) return;
    for (int i = 0; i < dfg->node_count; i++) { first_use[i] = dfg->nodes[i].schedule_cycle; last_use[i] = dfg->nodes[i].schedule_cycle; }
    /* The result of a node is "born" when scheduled and "dies" after last consumer */
    for (int i = 0; i < dfg->node_count; i++) {
        int birth = dfg->nodes[i].schedule_cycle + dfg->nodes[i].latency;
        int death = birth;
        /* Find all consumers of node i */
        for (int j = 0; j < dfg->edge_count; j++) {
            if (dfg->edges[j].from == i) {
                int consumer_cycle = dfg->nodes[dfg->edges[j].to].schedule_cycle;
                if (consumer_cycle > death) death = consumer_cycle;
            }
        }
        first_use[i] = birth; last_use[i] = death;
    }
}

void alloc_print(AllocResult *r) {
    if (!r) return;
    const char *names[] = {"Adder","Multiplier","Divider","LoadUnit","StoreUnit","ALU"};
    printf("=== Resource Allocation ===\n");
    for (int i = 0; i < r->type_count; i++) { printf("  %-12s: %d units (area=%.1f, delay=%.1f)\n", names[(int)r->units[i].type], r->units[i].count, r->units[i].area, r->units[i].delay); }
    printf("  Total area: %.1f\n", r->total_area);
}
