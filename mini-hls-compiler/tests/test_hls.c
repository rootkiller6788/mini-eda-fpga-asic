#include "hls_dfg.h"
#include "hls_schedule.h"
#include "hls_allocate.h"
#include "hls_binding.h"
#include "hls_fsmd.h"
#include <stdio.h>
#include <assert.h>

static int test_dfg_create(void) {
    DataFlowGraph dfg; dfg_init(&dfg);
    int n = dfg_add_node(&dfg, HLS_ADD, "test", 1.0);
    assert(n == 0); assert(dfg.node_count == 1);
    assert(dfg.nodes[0].op == HLS_ADD); assert(dfg.nodes[0].latency == 1);
    printf("  PASS: DFG creation\n"); return 0;
}
static int test_dfg_edges(void) {
    DataFlowGraph dfg; dfg_init(&dfg);
    int a = dfg_add_node(&dfg, HLS_LOAD, "a", 0.5);
    int b = dfg_add_node(&dfg, HLS_LOAD, "b", 0.5);
    int add = dfg_add_node(&dfg, HLS_ADD, "add", 1.0);
    assert(dfg_add_edge(&dfg, a, add, 0) == 0);
    assert(dfg_add_edge(&dfg, b, add, 0) == 1);
    assert(dfg.edge_count == 2); assert(dfg.nodes[add].input_count == 2);
    printf("  PASS: DFG edges\n"); return 0;
}
static int test_asap_schedule(void) {
    DataFlowGraph dfg; dfg_init(&dfg);
    int a = dfg_add_node(&dfg, HLS_LOAD, "a", 0.5);
    int b = dfg_add_node(&dfg, HLS_LOAD, "b", 0.5);
    int add = dfg_add_node(&dfg, HLS_ADD, "res", 1.0);
    dfg_add_edge(&dfg, a, add, 0); dfg_add_edge(&dfg, b, add, 0);
    assert(sched_asap(&dfg));
    assert(dfg.nodes[a].schedule_cycle == 0);
    assert(dfg.nodes[add].schedule_cycle >= 1);
    printf("  PASS: ASAP scheduling\n"); return 0;
}
static int test_alloc(void) {
    DataFlowGraph dfg; dfg_init(&dfg);
    dfg_add_node(&dfg, HLS_LOAD, "a", 0.5); dfg_add_node(&dfg, HLS_MUL, "m", 5.0);
    sched_asap(&dfg); AllocResult r;
    assert(alloc_greedy(&dfg, &r)); assert(r.total_area > 0);
    printf("  PASS: Resource allocation\n"); return 0;
}
static int test_binding(void) {
    DataFlowGraph dfg; dfg_init(&dfg);
    int a = dfg_add_node(&dfg, HLS_LOAD, "a", 0.5);
    int b = dfg_add_node(&dfg, HLS_LOAD, "b", 0.5);
    int add = dfg_add_node(&dfg, HLS_ADD, "res", 1.0);
    dfg_add_edge(&dfg, a, add, 0); dfg_add_edge(&dfg, b, add, 0);
    sched_asap(&dfg); BindingResult r;
    assert(bind_register_lifetime(&dfg, &r));
    assert(r.reg_count > 0);
    printf("  PASS: Register binding\n"); return 0;
}
static int test_fsmd(void) {
    DataFlowGraph dfg; dfg_init(&dfg);
    int a = dfg_add_node(&dfg, HLS_LOAD, "x", 0.5);
    int add = dfg_add_node(&dfg, HLS_ADD, "y", 1.0);
    dfg_add_edge(&dfg, a, add, 0);
    sched_asap(&dfg); FsmdController c; c.dfg = &dfg;
    assert(fsmd_generate(&c)); assert(c.state_count > 0);
    printf("  PASS: FSMD generation\n"); return 0;
}
int main(void) {
    printf("=== HLS Compiler Tests ===\n");
    int passed = 0, failed = 0;
    #define RUN(t) do { if (t() == 0) passed++; else { printf("  FAIL: %s\n", #t); failed++; } } while(0)
    RUN(test_dfg_create); RUN(test_dfg_edges); RUN(test_asap_schedule);
    RUN(test_alloc); RUN(test_binding); RUN(test_fsmd);
    printf("\nResult: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
