#include "hls_dfg.h"
#include "hls_schedule.h"
#include "hls_allocate.h"
#include "hls_binding.h"
#include "hls_fsmd.h"
#include <stdio.h>

int main(void) {
    printf("=== mini-hls-compiler Demo ===\n\n");
    DataFlowGraph dfg;
    dfg_init(&dfg);
    /* Build DFG: a * b + c  (mul -> add) */
    int ld_a = dfg_add_node(&dfg, HLS_LOAD, "a", 0.5);
    int ld_b = dfg_add_node(&dfg, HLS_LOAD, "b", 0.5);
    int ld_c = dfg_add_node(&dfg, HLS_LOAD, "c", 0.5);
    int mul  = dfg_add_node(&dfg, HLS_MUL,  "mul", 5.0);
    int add  = dfg_add_node(&dfg, HLS_ADD,  "add", 1.5);
    int cmp  = dfg_add_node(&dfg, HLS_CMP,  "cmp", 0.5);
    int st   = dfg_add_node(&dfg, HLS_STORE,"result", 0.5);
    dfg_add_edge(&dfg, ld_a, mul, 0);
    dfg_add_edge(&dfg, ld_b, mul, 0);
    dfg_add_edge(&dfg, mul, add, 0);
    dfg_add_edge(&dfg, ld_c, add, 0);
    dfg_add_edge(&dfg, add, cmp, 0);
    dfg_add_edge(&dfg, cmp, st, 0);
    printf("DFG: %d nodes, %d edges\n", dfg.node_count, dfg.edge_count);
    printf("Critical path: %d\n\n", dfg_critical_path(&dfg));
    /* ASAP scheduling */
    sched_asap(&dfg);
    sched_print(&dfg, "ASAP");
    /* Resource allocation */
    AllocResult alloc;
    alloc_greedy(&dfg, &alloc);
    alloc_print(&alloc);
    /* Register binding */
    BindingResult bind_res;
    bind_register_lifetime(&dfg, &bind_res);
    bind_functional_unit(&dfg, &bind_res);
    bind_print(&bind_res);
    printf("\nTotal registers: %d\n\n", bind_register_count(&bind_res));
    /* FSMD generation */
    FsmdController ctrl;
    fsmd_init(&ctrl, &dfg);
    fsmd_generate(&ctrl);
    fsmd_print(&ctrl);
    printf("\n=== Verilog Output ===\n");
    fsmd_emit_verilog(&ctrl);
    return 0;
}
