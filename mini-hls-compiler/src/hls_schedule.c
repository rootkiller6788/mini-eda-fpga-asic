#include "hls_schedule.h"
#include "hls_dfg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Compute the latency weight of a node operation */
static int node_latency(DfgNode *n) { return n->latency; }

/* ASAP scheduling: forward pass, assign earliest possible cycles */
bool sched_asap(DataFlowGraph *dfg) {
    if (!dfg || dfg->node_count == 0) return false;
    /* Initialize all nodes to cycle 0 */
    for (int i = 0; i < dfg->node_count; i++) {
        dfg->nodes[i].asap = 0;
        dfg->nodes[i].schedule_cycle = 0;
    }
    /* Iterate until stable (longest-path propagation through edges) */
    bool changed = true;
    int iter = 0;
    while (changed && iter < dfg->node_count * 2) {
        changed = false; iter++;
        for (int j = 0; j < dfg->edge_count; j++) {
            DfgEdge *e = &dfg->edges[j];
            DfgNode *src = &dfg->nodes[e->from];
            DfgNode *dst = &dfg->nodes[e->to];
            int new_cycle = src->asap + node_latency(src) + e->delay;
            if (new_cycle > dst->asap) { dst->asap = new_cycle; dst->schedule_cycle = new_cycle; changed = true; }
        }
    }
    for (int i = 0; i < dfg->node_count; i++) dfg->nodes[i].scheduled = true;
    return true;
}

/* ALAP scheduling: backward pass from max_latency constraint */
bool sched_alap(DataFlowGraph *dfg, int max_latency) {
    if (!dfg || dfg->node_count == 0) return false;
    /* First run ASAP to know minimum latency */
    sched_asap(dfg);
    /* Initialize alap to max_latency */
    for (int i = 0; i < dfg->node_count; i++) dfg->nodes[i].alap = max_latency;
    /* Backward propagation */
    bool changed = true;
    int iter = 0;
    while (changed && iter < dfg->node_count * 2) {
        changed = false; iter++;
        for (int j = 0; j < dfg->edge_count; j++) {
            DfgEdge *e = &dfg->edges[j];
            DfgNode *dst = &dfg->nodes[e->to];
            DfgNode *src = &dfg->nodes[e->from];
            int new_cycle = dst->alap - node_latency(src) - e->delay;
            if (new_cycle < src->alap) { src->alap = new_cycle; changed = true; }
        }
    }
    /* Move nodes toward ALAP if feasible */
    for (int i = 0; i < dfg->node_count; i++) {
        if (dfg->nodes[i].alap > dfg->nodes[i].asap)
            dfg->nodes[i].schedule_cycle = dfg->nodes[i].alap;
        else
            dfg->nodes[i].schedule_cycle = dfg->nodes[i].asap;
    }
    return true;
}

/* List scheduling with resource constraint */
bool sched_list(DataFlowGraph *dfg, int max_resources) {
    if (!dfg || dfg->node_count == 0) return false;
    /* Run ASAP first to get priorities */
    sched_asap(dfg);
    int *ready = (int*)malloc((size_t)dfg->node_count * sizeof(int));
    int *pred_count = (int*)malloc((size_t)dfg->node_count * sizeof(int));
    if (!ready || !pred_count) { free(ready); free(pred_count); return false; }
    for (int i = 0; i < dfg->node_count; i++) { ready[i] = 0; pred_count[i] = 0; }
    /* Count predecessors for each node */
    for (int j = 0; j < dfg->edge_count; j++) pred_count[dfg->edges[j].to]++;
    int cycle = 0, done = 0, used;
    while (done < dfg->node_count && cycle < 256) {
        /* Find ready nodes (all predecessors scheduled) */
        int rc = 0;
        for (int i = 0; i < dfg->node_count; i++) {
            if (!dfg->nodes[i].scheduled && pred_count[i] == 0) ready[rc++] = i;
        }
        if (rc == 0) { cycle++; continue; }
        /* Sort ready nodes by ASAP priority (earliest first) */
        for (int i = 0; i < rc - 1; i++)
            for (int k = i + 1; k < rc; k++)
                if (dfg->nodes[ready[i]].asap > dfg->nodes[ready[k]].asap) {
                    int tmp = ready[i]; ready[i] = ready[k]; ready[k] = tmp;
                }
        used = 0;
        for (int i = 0; i < rc && used < max_resources; i++) {
            int ni = ready[i];
            if (dfg->nodes[ni].scheduled) continue;
            dfg->nodes[ni].schedule_cycle = cycle;
            dfg->nodes[ni].scheduled = true;
            used++; done++;
            /* Free successors */
            for (int j = 0; j < dfg->edge_count; j++)
                if (dfg->edges[j].from == ni) pred_count[dfg->edges[j].to]--;
        }
        cycle++;
    }
    free(ready); free(pred_count);
    return true;
}

/* Force-directed scheduling: balance operations across cycles */
bool sched_force_directed(DataFlowGraph *dfg, int max_latency) {
    if (!dfg || dfg->node_count == 0) return false;
    /* Initialize with ASAP and ALAP */
    sched_asap(dfg);
    /* For force-directed, we simply distribute evenly */
    int total_ops = dfg->node_count;
    int ops_per_cycle = (total_ops + max_latency - 1) / max_latency;
    if (ops_per_cycle < 1) ops_per_cycle = 1;
    int *ops_in_cycle = (int*)calloc((size_t)(max_latency + 16), sizeof(int));
    if (!ops_in_cycle) return false;
    for (int i = 0; i < dfg->node_count; i++) {
        int target = dfg->nodes[i].asap;
        /* Try to balance: find the least-loaded feasible cycle */
        int best_c = target, best_load = 9999;
        for (int c = dfg->nodes[i].asap; c <= max_latency && c < dfg->nodes[i].asap + 4; c++) {
            if (ops_in_cycle[c] < best_load) { best_load = ops_in_cycle[c]; best_c = c; }
        }
        dfg->nodes[i].schedule_cycle = best_c;
        dfg->nodes[i].scheduled = true;
        ops_in_cycle[best_c]++;
    }
    free(ops_in_cycle);
    return true;
}

int sched_total_latency(DataFlowGraph *dfg) {
    int max_c = 0;
    for (int i = 0; i < dfg->node_count; i++) {
        int end = dfg->nodes[i].schedule_cycle + node_latency(&dfg->nodes[i]);
        if (end > max_c) max_c = end;
    }
    return max_c;
}

void sched_print(DataFlowGraph *dfg, const char *title) {
    printf("=== Schedule: %s ===\n", title);
    sched_total_latency(dfg); /* ensure consistency */
    for (int c = 0; c < 16; c++) {
        int count = 0;
        for (int i = 0; i < dfg->node_count; i++)
            if (dfg->nodes[i].schedule_cycle == c) count++;
        if (count > 0) {
            printf("  Cycle %2d: ", c);
            for (int i = 0; i < dfg->node_count; i++)
                if (dfg->nodes[i].schedule_cycle == c) printf("%s ", dfg->nodes[i].name);
            printf("\n");
        }
    }
    printf("  Total latency: %d cycles\n", sched_total_latency(dfg));
}
