#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "hls_pipeline.h"
#include "loop_optimize.h"
#include "array_partition.h"
#include "interface_pragma.h"

#define IMG_HEIGHT  64
#define IMG_WIDTH   64
#define KERNEL_SIZE 3
#define OUT_HEIGHT  (IMG_HEIGHT - KERNEL_SIZE + 1)
#define OUT_WIDTH   (IMG_WIDTH  - KERNEL_SIZE + 1)

typedef struct {
    float data[OUT_HEIGHT][OUT_WIDTH];
} OutputBuffer;

typedef struct {
    HlsArray      *input_arr;
    HlsArray      *weight_arr;
    HlsArray      *output_arr;
    HlsLoopNest   *nest;
    HlsLoop       *h_loop;
    HlsLoop       *w_loop;
    HlsLoop       *kh_loop;
    HlsLoop       *kw_loop;
    HlsPragmaSet  *pragmas;
    HlsDataFlowGraph *dfg;
} Conv2DDesign;

static Conv2DDesign* conv2d_design_create(void)
{
    Conv2DDesign *d = calloc(1, sizeof(Conv2DDesign));
    if (!d) return NULL;

    d->input_arr = hls_array_create("input_feature", 32, 3);
    hls_array_set_dim(d->input_arr, 0, IMG_HEIGHT);
    hls_array_set_dim(d->input_arr, 1, IMG_WIDTH);
    hls_array_set_dim(d->input_arr, 2, 1);

    d->weight_arr = hls_array_create("kernel_weight", 32, 2);
    hls_array_set_dim(d->weight_arr, 0, KERNEL_SIZE);
    hls_array_set_dim(d->weight_arr, 1, KERNEL_SIZE);

    d->output_arr = hls_array_create("output_feature", 32, 2);
    hls_array_set_dim(d->output_arr, 0, OUT_HEIGHT);
    hls_array_set_dim(d->output_arr, 1, OUT_WIDTH);

    d->nest = hls_loop_nest_create();

    d->h_loop  = hls_loop_add(d->nest, NULL, LOOP_FOR, "conv_h");
    hls_loop_set_trip_count(d->h_loop, OUT_HEIGHT);

    d->w_loop  = hls_loop_add(d->nest, d->h_loop, LOOP_FOR, "conv_w");
    hls_loop_set_trip_count(d->w_loop, OUT_WIDTH);

    d->kh_loop = hls_loop_add(d->nest, d->w_loop, LOOP_FOR, "conv_kh");
    hls_loop_set_trip_count(d->kh_loop, KERNEL_SIZE);

    d->kw_loop = hls_loop_add(d->nest, d->kh_loop, LOOP_FOR, "conv_kw");
    hls_loop_set_trip_count(d->kw_loop, KERNEL_SIZE);

    d->pragmas = hls_pragma_set_create();
    hls_pragma_set_top(d->pragmas, "conv2d_top");
    hls_pragma_set_clock(d->pragmas, 10);

    d->dfg = hls_dataflow_create("conv2d_dataflow");

    return d;
}

static void conv2d_optimize_memory(Conv2DDesign *d)
{
    printf("\n--- Memory Optimization ---\n");

    hls_array_partition_complete(d->weight_arr, 0);
    hls_array_partition_complete(d->weight_arr, 1);

    hls_array_partition_cyclic(d->input_arr, 1, 4);

    hls_array_set_memory_type(d->input_arr, MEM_BRAM);
    hls_array_set_ports(d->input_arr, 2, true);

    HlsMemoryTradeoff mt_w = hls_memory_tradeoff_analyze(
        d->weight_arr->total_elements, d->weight_arr->elem_width);
    printf("Weights: %s\n", mt_w.rationale);

    HlsMemoryTradeoff mt_i = hls_memory_tradeoff_analyze(
        d->input_arr->total_elements, d->input_arr->elem_width);
    printf("Input: %s\n", mt_i.rationale);

    hls_array_set_memory_type(d->weight_arr,
        mt_w.recommended);
    hls_array_set_memory_type(d->output_arr,
        hls_array_recommend_memory(d->output_arr));

    hls_array_print_layout(d->input_arr, stdout);
    hls_array_print_layout(d->weight_arr, stdout);
    hls_array_print_layout(d->output_arr, stdout);
}

static void conv2d_optimize_loops(Conv2DDesign *d)
{
    printf("\n--- Loop Optimization ---\n");

    hls_loop_pipeline_set_ii(d->h_loop, 1);
    hls_loop_pipeline_set_ii(d->w_loop, 1);

    HlsUnrollConfig ucfg = { 0, true, true, "" };
    hls_loop_unroll_configured(d->kh_loop, &ucfg);
    hls_loop_unroll_configured(d->kw_loop, &ucfg);

    HlsFlattenConfig fcfg = { true, 0, false };
    hls_loop_flatten(d->nest, &fcfg);

    printf("Pipeline II: h=%u w=%u kh=%u kw=%u\n",
        d->h_loop->pipeline_ii, d->w_loop->pipeline_ii,
        d->kh_loop->pipeline_ii, d->kw_loop->pipeline_ii);
    printf("Unroll: kh_kind=%d kw_kind=%d\n",
        d->kh_loop->unroll_kind, d->kw_loop->unroll_kind);

    hls_loop_pipeline_print(d->nest, stdout);

    HlsTripCount tc = hls_analyze_trip_count(d->h_loop);
    printf("Trip count h: %s\n", tc.bound_desc);
    int64_t total_ops = (int64_t)OUT_HEIGHT * OUT_WIDTH *
        KERNEL_SIZE * KERNEL_SIZE;
    printf("Total MAC ops: %lld\n", (long long)total_ops);
}

static void conv2d_setup_interfaces(Conv2DDesign *d)
{
    printf("\n--- Interface Setup ---\n");

    hls_interface_axi_master(d->pragmas, "gmem_in", NULL);
    hls_interface_axi_master(d->pragmas, "gmem_out", NULL);

    HlsAxiMasterConfig axi_cfg;
    hls_axi_master_config_default(&axi_cfg);
    axi_cfg.max_burst_len = 16;
    axi_cfg.burst_size = 4;
    axi_cfg.num_read_channels = 2;

    const char *bundle_ports[] = { "gmem_in", "gmem_w" };
    hls_interface_bundle(d->pragmas, "gmem",
        bundle_ports, 2, IF_M_AXI);

    hls_interface_s_axilite(d->pragmas, "ctrl");
    hls_interface_axi_stream(d->pragmas, "axis_out", 16);

    hls_pragma_pipeline(d->pragmas, 1, false, true);
    hls_pragma_pipeline_set_target(d->pragmas, "conv2d_top");

    hls_pragma_unroll(d->pragmas, 0, "conv_kh_region");
    hls_pragma_unroll(d->pragmas, 0, "conv_kw_region");

    hls_pragma_array_partition(d->pragmas, "weight_arr",
        ARRAY_PART_COMPLETE, 1, 0);
    hls_pragma_array_partition(d->pragmas, "weight_arr",
        ARRAY_PART_COMPLETE, 2, 0);
    hls_pragma_array_partition(d->pragmas, "input_feature",
        ARRAY_PART_CYCLIC, 2, 4);

    hls_pragma_dataflow(d->pragmas, "conv2d_dataflow");

    hls_pragma_resource(d->pragmas, "mul", "DSP48E2", 4);
    hls_pragma_resource(d->pragmas, "add", "DSP48E2", 4);

    if (hls_pragma_validate(d->pragmas))
        printf("Pragma set validated OK\n");
    else
        printf("Pragma set validation FAILED\n");
}

static void conv2d_setup_dataflow(Conv2DDesign *d)
{
    printf("\n--- Dataflow Setup ---\n");

    HlsTask *t_ld = hls_dataflow_add_task(d->dfg, "load_data");
    HlsTask *t_conv = hls_dataflow_add_task(d->dfg, "conv2d_compute");
    HlsTask *t_st = hls_dataflow_add_task(d->dfg, "store_data");

    t_ld->latency = 128;
    t_conv->latency = 256;
    t_st->latency = 128;
    t_conv->is_pipelined = true;

    HlsChannel *ch1 = hls_dataflow_add_channel(d->dfg,
        CHAN_FIFO, "fifo_ld_conv");
    hls_channel_fifo_configure(ch1, 32, 128);

    HlsChannel *ch2 = hls_dataflow_add_channel(d->dfg,
        CHAN_PINGPONG, "pingpong_conv_st");
    hls_channel_pingpong_configure(ch2, 1024);

    hls_dataflow_connect(t_ld, ch1, t_conv);
    hls_dataflow_connect(t_conv, ch2, t_st);

    int32_t radius[3] = {1, 1, 0};
    hls_stencil_define(t_conv, STENCIL_2D, radius);
    hls_stencil_line_buffer(t_conv, KERNEL_SIZE);
    if (hls_stencil_validate(t_conv))
        printf("Stencil pattern validated for conv2d\n");

    hls_dataflow_schedule(d->dfg);
    hls_dataflow_balance(d->dfg);

    if (hls_dataflow_verify(d->dfg))
        printf("Dataflow verification passed\n");

    hls_dataflow_report(d->dfg, stdout);
}

static void conv2d_run_scheduling(Conv2DDesign *d)
{
    printf("\n--- Scheduling ---\n");

    HlsDataFlowGraph *dfg = hls_dfg_create();
    HlsNode *ld_in = hls_dfg_add_node(dfg, HLS_OP_LD, "ld_input");
    HlsNode *ld_w  = hls_dfg_add_node(dfg, HLS_OP_LD, "ld_weight");
    HlsNode *mul   = hls_dfg_add_node(dfg, HLS_OP_MUL, "mul");
    HlsNode *acc   = hls_dfg_add_node(dfg, HLS_OP_ADD, "accumulate");
    HlsNode *st    = hls_dfg_add_node(dfg, HLS_OP_ST, "st_output");

    hls_dfg_add_edge(ld_in, mul);
    hls_dfg_add_edge(ld_w, mul);
    hls_dfg_add_edge(mul, acc);
    hls_dfg_add_edge(acc, st);

    HlsScheduleResult asap = hls_schedule_asap(dfg);
    printf("ASAP: %u cycles (crit=%u)\n",
        asap.total_cycles, asap.critical_path);

    uint32_t limits[8] = {2, 4, 1, 2, 2, 4, 2, 2};
    HlsScheduleResult list = hls_schedule_list(dfg, limits);
    printf("List (res-constrained): %u cycles\n",
        list.total_cycles);

    hls_bind_resources(dfg, &asap);

    HlsPipelineConfig pipe = hls_pipeline_create(1, 5);
    HlsRTLModule *rtl = hls_generate_rtl(dfg, &pipe);
    if (rtl) {
        printf("RTL: %s, latency=%u, throughput=%u\n",
            rtl->name, rtl->total_latency, rtl->throughput);
        hls_rtl_destroy(rtl);
    }

    free(pipe.stages);
    hls_dfg_destroy(dfg);
}

static void conv2d_design_destroy(Conv2DDesign *d)
{
    if (!d) return;
    hls_array_destroy(d->input_arr);
    hls_array_destroy(d->weight_arr);
    hls_array_destroy(d->output_arr);
    hls_loop_nest_destroy(d->nest);
    hls_pragma_set_destroy(d->pragmas);
    hls_dataflow_destroy(d->dfg);
    free(d);
}

static void conv2d_print_summary(Conv2DDesign *d)
{
    printf("\n=== Convolution 2D Design Summary ===\n");
    printf("Input:  %ux%ux1\n", IMG_HEIGHT, IMG_WIDTH);
    printf("Kernel: %ux%u\n", KERNEL_SIZE, KERNEL_SIZE);
    printf("Output: %ux%u\n", OUT_HEIGHT, OUT_WIDTH);
    printf("Loops:  %u total, max depth=%u\n",
        d->nest->num_loops, d->nest->max_depth);
    printf("Arrays: 3 (input/weight/output)\n");

    int64_t total_mac = (int64_t)OUT_HEIGHT * OUT_WIDTH *
        KERNEL_SIZE * KERNEL_SIZE;
    printf("Total MAC: %lld\n", (long long)total_mac);

    double throughput = (double)total_mac /
        (double)(OUT_HEIGHT * d->h_loop->pipeline_ii);
    printf("Estimated throughput: %.1f MAC/cycle\n", throughput);
}

int main(void)
{
    printf("=== mini-hls-compiler: Convolution 2D Demo ===\n");

    Conv2DDesign *d = conv2d_design_create();
    if (!d) { printf("Failed to create design\n"); return 1; }

    conv2d_optimize_memory(d);
    conv2d_optimize_loops(d);
    conv2d_setup_interfaces(d);
    conv2d_setup_dataflow(d);
    conv2d_run_scheduling(d);
    conv2d_print_summary(d);

    printf("\n--- Generated Tcl Directives ---\n");
    hls_pragma_print_tcl(d->pragmas, stdout);

    conv2d_design_destroy(d);
    printf("\nDone.\n");
    return 0;
}
