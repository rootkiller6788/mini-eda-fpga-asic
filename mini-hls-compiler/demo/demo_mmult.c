#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "hls_pipeline.h"
#include "loop_optimize.h"
#include "array_partition.h"
#include "dataflow_opt.h"
#include "interface_pragma.h"

#define MATRIX_SIZE  128
#define BLOCK_SIZE   16
#define BLOCKS       (MATRIX_SIZE / BLOCK_SIZE)

typedef struct {
    HlsArray      *arr_a;
    HlsArray      *arr_b;
    HlsArray      *arr_c;
    HlsLoopNest   *nest;
    HlsLoop       *block_i_loop;
    HlsLoop       *block_j_loop;
    HlsLoop       *block_k_loop;
    HlsLoop       *inner_i_loop;
    HlsLoop       *inner_j_loop;
    HlsLoop       *inner_k_loop;
    HlsPragmaSet  *pragmas;
    HlsDataFlowGraph *dfg;
    uint32_t       dsp_target;
} MatrixMultDesign;

static MatrixMultDesign* mmult_design_create(void)
{
    MatrixMultDesign *d = calloc(1, sizeof(MatrixMultDesign));
    if (!d) return NULL;

    d->arr_a = hls_array_create("A", 32, 2);
    hls_array_set_dim(d->arr_a, 0, MATRIX_SIZE);
    hls_array_set_dim(d->arr_a, 1, MATRIX_SIZE);

    d->arr_b = hls_array_create("B", 32, 2);
    hls_array_set_dim(d->arr_b, 0, MATRIX_SIZE);
    hls_array_set_dim(d->arr_b, 1, MATRIX_SIZE);

    d->arr_c = hls_array_create("C", 32, 2);
    hls_array_set_dim(d->arr_c, 0, MATRIX_SIZE);
    hls_array_set_dim(d->arr_c, 1, MATRIX_SIZE);

    d->nest = hls_loop_nest_create();

    d->block_i_loop = hls_loop_add(d->nest, NULL, LOOP_FOR,
        "block_i");
    hls_loop_set_trip_count(d->block_i_loop, BLOCKS);

    d->block_j_loop = hls_loop_add(d->nest, d->block_i_loop, LOOP_FOR,
        "block_j");
    hls_loop_set_trip_count(d->block_j_loop, BLOCKS);

    d->block_k_loop = hls_loop_add(d->nest, d->block_j_loop, LOOP_FOR,
        "block_k");
    hls_loop_set_trip_count(d->block_k_loop, BLOCKS);

    d->inner_i_loop = hls_loop_add(d->nest, d->block_k_loop, LOOP_FOR,
        "inner_i");
    hls_loop_set_trip_count(d->inner_i_loop, BLOCK_SIZE);

    d->inner_j_loop = hls_loop_add(d->nest, d->inner_i_loop, LOOP_FOR,
        "inner_j");
    hls_loop_set_trip_count(d->inner_j_loop, BLOCK_SIZE);

    d->inner_k_loop = hls_loop_add(d->nest, d->inner_j_loop, LOOP_FOR,
        "inner_k");
    hls_loop_set_trip_count(d->inner_k_loop, BLOCK_SIZE);

    d->pragmas = hls_pragma_set_create();
    hls_pragma_set_top(d->pragmas, "matrix_multiply");
    hls_pragma_set_clock(d->pragmas, 5);

    d->dfg = hls_dataflow_create("mmult_dataflow");
    d->dsp_target = 256;

    return d;
}

static void mmult_partition_arrays(MatrixMultDesign *d)
{
    printf("\n--- Array Partition ---\n");

    hls_array_partition_cyclic(d->arr_a, 1, BLOCK_SIZE);
    printf("A: cyclic partition dim[1] x%u -> %u banks\n",
        BLOCK_SIZE, d->arr_a->num_banks);

    hls_array_partition_block(d->arr_b, 0, BLOCK_SIZE);
    printf("B: block partition dim[0] x%u -> %u banks\n",
        BLOCK_SIZE, d->arr_b->num_banks);

    hls_array_set_ports(d->arr_a, 2, true);
    hls_array_set_ports(d->arr_b, 2, true);
    hls_array_set_ports(d->arr_c, 2, true);

    uint32_t a_accesses = BLOCK_SIZE;
    bool a_parallel = hls_array_can_access_parallel(
        d->arr_a, a_accesses);
    printf("A: %u parallel accesses -> %s\n",
        a_accesses, a_parallel ? "OK" : "LIMIT");

    HlsMemoryTradeoff mt_a = hls_memory_tradeoff_analyze(
        d->arr_a->total_elements, d->arr_a->elem_width);
    printf("A memory: %s\n", mt_a.rationale);

    HlsMemoryTradeoff mt_b = hls_memory_tradeoff_analyze(
        d->arr_b->total_elements, d->arr_b->elem_width);
    printf("B memory: %s\n", mt_b.rationale);

    HlsMemType rec_a = hls_array_recommend_memory(d->arr_a);
    HlsMemType rec_b = hls_array_recommend_memory(d->arr_b);
    HlsMemType rec_c = hls_array_recommend_memory(d->arr_c);

    hls_array_set_memory_type(d->arr_a, rec_a);
    hls_array_set_memory_type(d->arr_b, rec_b);
    hls_array_set_memory_type(d->arr_c, rec_c);

    printf("Memory types: A=%d B=%d C=%d\n", rec_a, rec_b, rec_c);

    uint32_t test_indices[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    bool conflict = hls_array_bank_conflict_detect(
        d->arr_a, test_indices, 8);
    printf("A bank conflict (8 accesses): %s\n",
        conflict ? "CONFLICT" : "OK");

    hls_array_print_layout(d->arr_a, stdout);
    printf("\n");
    hls_array_print_banking(d->arr_a, stdout);
}

static void mmult_optimize_loops(MatrixMultDesign *d)
{
    printf("\n--- Loop Optimization ---\n");

    hls_loop_pipeline_set_ii(d->inner_k_loop, 1);
    hls_loop_pipeline_set_ii(d->inner_j_loop, 1);
    hls_loop_pipeline_set_ii(d->inner_i_loop, 1);

    HlsUnrollConfig ucfg = { 0, true, true, "" };
    hls_loop_unroll_configured(d->inner_k_loop, &ucfg);
    hls_loop_unroll_configured(d->inner_j_loop, &ucfg);

    HlsUnrollConfig pcfg = { 4, true, true, "" };
    hls_loop_unroll_configured(d->inner_i_loop, &pcfg);

    HlsFlattenConfig fcfg = { true, 0, true };
    hls_loop_flatten(d->nest, &fcfg);

    hls_loop_pipeline_set_ii(d->block_j_loop, 1);
    hls_loop_pipeline_set_ii(d->block_i_loop, 1);

    printf("Loop pipeline II values set\n");
    hls_loop_pipeline_print(d->nest, stdout);

    HlsTripCount tc = hls_analyze_trip_count(d->inner_k_loop);
    printf("Inner loop trip count: %s\n", tc.bound_desc);

    bool legal = true;
    legal = legal && hls_loop_unroll_is_legal(d->inner_k_loop, 0);
    legal = legal && hls_loop_unroll_is_legal(d->inner_j_loop, 0);
    printf("Unroll legality (inner_k, inner_j): %s\n",
        legal ? "OK" : "FAIL");

    int64_t total_ops = (int64_t)MATRIX_SIZE * MATRIX_SIZE * MATRIX_SIZE;
    printf("Total operations: %lld multiply-add\n",
        (long long)total_ops);
    int64_t gops = total_ops / 1000000000LL;
    printf("Approx: %lld GMAC\n", (long long)gops);

    HlsRewindConfig rwc = { true, 2, true };
    hls_loop_rewind_configure(d->inner_i_loop, &rwc);
    printf("Rewind enabled on inner_i_loop\n");
}

static void mmult_setup_interfaces(MatrixMultDesign *d)
{
    printf("\n--- Interface & Pragma Setup ---\n");

    const char *mem_ports[] = { "A", "B", "C" };
    hls_interface_bundle(d->pragmas, "gmem_bundle",
        mem_ports, 3, IF_M_AXI);

    hls_interface_s_axilite(d->pragmas, "ctrl");
    hls_interface_axi_stream(d->pragmas, "stream_out", 64);

    hls_interface_ap_ctrl_none(d->pragmas);

    hls_pragma_pipeline(d->pragmas, 1, true, false);
    hls_pragma_pipeline_set_target(d->pragmas,
        "matrix_multiply");

    hls_pragma_unroll(d->pragmas, 0, "inner_k");
    hls_pragma_unroll(d->pragmas, 0, "inner_j");
    hls_pragma_unroll(d->pragmas, 4, "inner_i");

    hls_pragma_array_partition(d->pragmas, "A",
        ARRAY_PART_CYCLIC, 2, BLOCK_SIZE);
    hls_pragma_array_partition(d->pragmas, "B",
        ARRAY_PART_BLOCK, 1, BLOCK_SIZE);
    hls_pragma_array_partition(d->pragmas, "C",
        ARRAY_PART_CYCLIC, 2, BLOCK_SIZE);

    hls_pragma_dataflow(d->pragmas, "mmult_compute");

    hls_pragma_resource(d->pragmas, "mul", "DSP48E2",
        (int32_t)d->dsp_target);
    hls_pragma_resource(d->pragmas, "add", "DSP48E2",
        (int32_t)d->dsp_target);
    hls_pragma_resource(d->pragmas, "mul", "Fabric", -1);

    if (hls_pragma_validate(d->pragmas))
        printf("Pragma validation: PASSED\n");
    else
        printf("Pragma validation: FAILED\n");

    printf("Interfaces: %u\n", d->pragmas->num_interfaces);
    printf("Unroll directives: %u\n", d->pragmas->num_unrolls);
    printf("Array partition directives: %u\n",
        d->pragmas->num_array_parts);
    printf("Resource directives: %u\n",
        d->pragmas->num_resources);
}

static void mmult_setup_dataflow(MatrixMultDesign *d)
{
    printf("\n--- Dataflow Setup ---\n");

    HlsTask *t_load_a = hls_dataflow_add_task(d->dfg,
        "load_matrix_A");
    HlsTask *t_load_b = hls_dataflow_add_task(d->dfg,
        "load_matrix_B");
    HlsTask *t_compute = hls_dataflow_add_task(d->dfg,
        "compute_mmult");
    HlsTask *t_store = hls_dataflow_add_task(d->dfg,
        "store_matrix_C");

    t_load_a->latency = 512;
    t_load_b->latency = 512;
    t_compute->latency = 4096;
    t_store->latency = 512;
    t_compute->is_pipelined = true;
    t_load_a->is_pipelined = true;
    t_load_b->is_pipelined = true;

    HlsChannel *ch_a = hls_dataflow_add_channel(d->dfg,
        CHAN_FIFO, "fifo_A");
    hls_channel_fifo_configure(ch_a, 256, 32 * BLOCK_SIZE);

    HlsChannel *ch_b = hls_dataflow_add_channel(d->dfg,
        CHAN_FIFO, "fifo_B");
    hls_channel_fifo_configure(ch_b, 256, 32 * BLOCK_SIZE);

    HlsChannel *ch_res = hls_dataflow_add_channel(d->dfg,
        CHAN_PINGPONG, "pingpong_result");
    hls_channel_pingpong_configure(ch_res, 1024);

    HlsChannel *ch_str = hls_dataflow_add_channel(d->dfg,
        CHAN_STREAM, "stream_status");
    hls_stream_create(ch_str, 16, 8);

    hls_dataflow_connect(t_load_a, ch_a, t_compute);
    hls_dataflow_connect(t_load_b, ch_b, t_compute);
    hls_dataflow_connect(t_compute, ch_res, t_store);
    hls_dataflow_connect(t_compute, ch_str, t_store);

    hls_dataflow_schedule(d->dfg);
    hls_dataflow_balance(d->dfg);

    if (hls_dataflow_verify(d->dfg))
        printf("Dataflow verification: PASSED\n");

    hls_dataflow_report(d->dfg, stdout);

    hls_task_start(t_load_a);
    hls_task_start(t_load_b);
    hls_task_start(t_compute);

    printf("Tasks started: load_A, load_B, compute\n");
    printf("Compute stalled: %s\n",
        hls_task_is_stalled(t_compute) ? "YES" : "NO");
}

static void mmult_run_performance_model(MatrixMultDesign *d)
{
    printf("\n--- Performance Model ---\n");

    HlsDataFlowGraph *dfg = hls_dfg_create();

    HlsNode *ld_a = hls_dfg_add_node(dfg, HLS_OP_LD, "ld_A");
    HlsNode *ld_b = hls_dfg_add_node(dfg, HLS_OP_LD, "ld_B");
    HlsNode *mul  = hls_dfg_add_node(dfg, HLS_OP_MUL, "multiply");
    HlsNode *acc  = hls_dfg_add_node(dfg, HLS_OP_ADD, "accumulate");
    HlsNode *st_c = hls_dfg_add_node(dfg, HLS_OP_ST, "st_C");

    hls_dfg_add_edge(ld_a, mul);
    hls_dfg_add_edge(ld_b, mul);
    hls_dfg_add_edge(mul, acc);
    hls_dfg_add_edge(acc, st_c);

    HlsScheduleResult asap = hls_schedule_asap(dfg);
    printf("Single MAC chain: %u cycles\n", asap.total_cycles);

    uint32_t dsp_limits[8] = {8, 256, 4, 4, 4, 256, 8, 8};
    HlsScheduleResult list = hls_schedule_list(dfg, dsp_limits);
    printf("Resource-constrained (DSP=256): %u cycles\n",
        list.total_cycles);

    hls_bind_resources(dfg, &asap);

    HlsPipelineConfig pipe = hls_pipeline_create(
        1, (uint32_t)(asap.total_cycles + 1));
    HlsRTLModule *rtl = hls_generate_rtl(dfg, &pipe);
    if (rtl) {
        printf("RTL pipeline: %u stages, "
            "II=%u, latency=%u\n",
            rtl->pipeline.num_stages,
            rtl->pipeline.ii,
            rtl->total_latency);
        printf("Throughput: %u ops/Mcycle, "
            "resources: DSP=%u LUT=%u FF=%u\n",
            rtl->throughput,
            rtl->num_resources[HLS_RES_DSP],
            rtl->num_resources[HLS_RES_LUT],
            rtl->num_resources[HLS_RES_FF]);
        hls_rtl_destroy(rtl);
    }

    free(pipe.stages);
    hls_dfg_destroy(dfg);

    uint32_t total_mac = MATRIX_SIZE * MATRIX_SIZE * MATRIX_SIZE;
    uint32_t block_mac = BLOCK_SIZE * BLOCK_SIZE * BLOCK_SIZE;
    uint32_t num_blocks = BLOCKS * BLOCKS * BLOCKS;

    printf("\nTotal MACs: %u\n", total_mac);
    printf("Per block: %u MACs\n", block_mac);
    printf("Blocks: %u\n", num_blocks);

    uint32_t est_cycles_init = (uint32_t)(
        (double)block_mac / (double)BLOCK_SIZE + BLOCK_SIZE);
    uint32_t est_cycles_total = num_blocks * est_cycles_init;
    printf("Estimated cycles (blocked): %u\n", est_cycles_total);
    printf("Estimated cycles (naive): %u\n",
        total_mac + MATRIX_SIZE * MATRIX_SIZE);

    (void)d;
}

static void mmult_design_destroy(MatrixMultDesign *d)
{
    if (!d) return;
    hls_array_destroy(d->arr_a);
    hls_array_destroy(d->arr_b);
    hls_array_destroy(d->arr_c);
    hls_loop_nest_destroy(d->nest);
    hls_pragma_set_destroy(d->pragmas);
    hls_dataflow_destroy(d->dfg);
    free(d);
}

static void mmult_print_config_summary(MatrixMultDesign *d)
{
    printf("\n=== Matrix Multiply Configuration ===\n");
    printf("Size:     %dx%d\n", MATRIX_SIZE, MATRIX_SIZE);
    printf("Block:    %dx%d\n", BLOCK_SIZE, BLOCK_SIZE);
    printf("Blocks:   %d\n", BLOCKS);
    printf("Precision: 32-bit float\n");
    printf("DSP target: %u\n", d->dsp_target);
    printf("Loops:    %u (6-level nest)\n", d->nest->num_loops);
    printf("Memory:   %u elements x 3 matrices\n",
        MATRIX_SIZE * MATRIX_SIZE);
}

int main(void)
{
    printf("=== mini-hls-compiler: Matrix Multiply Demo ===\n");

    MatrixMultDesign *d = mmult_design_create();
    if (!d) { printf("Failed to create design\n"); return 1; }

    mmult_print_config_summary(d);
    mmult_partition_arrays(d);
    mmult_optimize_loops(d);
    mmult_setup_interfaces(d);
    mmult_setup_dataflow(d);
    mmult_run_performance_model(d);

    printf("\n--- Generated Tcl Directives ---\n");
    hls_pragma_print_tcl(d->pragmas, stdout);

    printf("\n--- Directive Summary ---\n");
    hls_pragma_print_directives(d->pragmas, stdout);

    mmult_design_destroy(d);
    printf("\nDone.\n");
    return 0;
}
