#include <stdio.h>
#include <stdlib.h>
#include "dataflow_opt.h"
#include "array_partition.h"

int main(void)
{
    printf("=== mini-hls-compiler: Dataflow + Array Example ===\n\n");

    HlsDataflowGraph *df = hls_dataflow_create("example_dataflow");

    HlsTask *t_read = hls_dataflow_add_task(df, "read_data");
    HlsTask *t_proc = hls_dataflow_add_task(df, "process_data");
    HlsTask *t_write = hls_dataflow_add_task(df, "write_data");

    t_read->latency = 10;
    t_proc->latency = 20;
    t_write->latency = 10;
    t_proc->is_pipelined = true;

    HlsChannel *ch_fifo = hls_dataflow_add_channel(df, CHAN_FIFO,
        "fifo_rp");
    hls_channel_fifo_configure(ch_fifo, 16, 32);

    HlsChannel *ch_pp = hls_dataflow_add_channel(df, CHAN_PINGPONG,
        "pingpong_pw");
    hls_channel_pingpong_configure(ch_pp, 1024);

    HlsChannel *ch_stream = hls_dataflow_add_channel(df, CHAN_STREAM,
        "stream_aux");
    hls_stream_create(ch_stream, 32, 64);

    hls_dataflow_connect(t_read,  ch_fifo,   t_proc);
    hls_dataflow_connect(t_proc,  ch_pp,     t_write);
    hls_dataflow_connect(t_proc,  ch_stream, t_write);

    printf("Dataflow graph: %u tasks, %u channels\n",
        df->num_tasks, df->num_channels);

    int32_t radius[3] = {1, 1, 0};
    hls_stencil_define(t_proc, STENCIL_2D, radius);
    hls_stencil_line_buffer(t_proc, 4);
    if (hls_stencil_validate(t_proc))
        printf("Stencil 2D (3x3) validated on process_data\n");

    hls_dataflow_schedule(df);
    hls_dataflow_balance(df);
    if (hls_dataflow_verify(df))
        printf("Dataflow verification passed\n");

    printf("\n--- Dataflow Report ---\n");
    hls_dataflow_report(df, stdout);

    hls_task_start(t_read);
    hls_task_stall(t_proc);
    printf("Read started, Proc stalled: %d\n",
        hls_task_is_stalled(t_proc));
    hls_task_resume(t_proc);
    hls_task_finish(t_read);

    printf("\n--- Array Partition ---\n");
    HlsArray *arr = hls_array_create("weights", 16, 2);
    hls_array_set_dim(arr, 0, 64);
    hls_array_set_dim(arr, 1, 32);
    printf("Array \"%s\": %u x %u = %u elements\n",
        arr->name, arr->dims[0].size, arr->dims[1].size,
        arr->total_elements);

    hls_array_partition_cyclic(arr, 0, 4);
    printf("Partitioned dim[0] cyclic x4: %u banks\n", arr->num_banks);

    hls_array_partition_block(arr, 1, 2);
    printf("Partitioned dim[1] block x2: %u banks\n", arr->num_banks);

    HlsMemoryTradeoff mt = hls_memory_tradeoff_analyze(
        arr->total_elements, arr->elem_width);
    printf("Memory tradeoff: %s (%u BRAM, %u LUT)\n",
        mt.rationale, mt.bram_count, mt.lut_usage);

    hls_array_set_ports(arr, 2, true);
    printf("Dual port BRAM, %u parallel accesses max\n",
        arr->num_ports * arr->num_banks);

    uint32_t indices[4] = {0, 4, 8, 12};
    bool conflict = hls_array_bank_conflict_detect(arr, indices, 4);
    printf("Bank conflict detection (4 accesses): %s\n",
        conflict ? "CONFLICT" : "OK");

    printf("\n--- Array Layout ---\n");
    hls_array_print_layout(arr, stdout);
    printf("\n--- Banking Report ---\n");
    hls_array_print_banking(arr, stdout);

    hls_array_set_memory_type(arr, MEM_BRAM);

    hls_array_destroy(arr);
    hls_dataflow_destroy(df);
    printf("Done.\n");
    return 0;
}
