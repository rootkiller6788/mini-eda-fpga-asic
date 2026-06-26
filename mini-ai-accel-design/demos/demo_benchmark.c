/* demo_benchmark.c - Full accelerator benchmark suite
 * L7: Application ¡ª comprehensive performance evaluation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "dnn_isa.h"
#include "systolic_array.h"
#include "pe_microarch.h"
#include "buffer_hierarchy.h"
#include "accel_roofline.h"

static void bench_matmul(systolic_array_t *sa, uint32_t size) {
    double *A = calloc(size*size, sizeof(double));
    double *B = calloc(size*size, sizeof(double));
    double *C = calloc(size*size, sizeof(double));
    for (uint32_t i = 0; i < size; i++) { A[i*size+i] = 1.0; B[i*size+i] = 2.0; }
    sa_matmul(sa, A, size, size, B, size, size, C);
    sa_stats_t s;
    sa_collect_stats(sa, &s);
    printf("  MatMul[%ux%u]: %llu MACs, %.2f TOPS, %.1f%% util
",
           size, size, (unsigned long long)s.mac_operations,
           s.throughput_tops, s.utilization*100);
    free(A); free(B); free(C);
}

static void bench_dataflow(systolic_array_t *sa) {
    double W[64], X[64], Y[64];
    memset(W, 0, sizeof(W)); memset(X, 0, sizeof(X));
    for (int i = 0; i < 8; i++) { W[i*8+i] = 1.0; X[i] = 2.0; }
    const char *df_names[] = {"WS","OS","RS","IS"};
    sa_dataflow_t dfs[] = {SA_DATAFLOW_WEIGHT_STATIONARY,
                            SA_DATAFLOW_OUTPUT_STATIONARY,
                            SA_DATAFLOW_ROW_STATIONARY,
                            SA_DATAFLOW_INPUT_STATIONARY};
    for (int d = 0; d < 4; d++) {
        sa_set_dataflow(sa, dfs[d]);
        sa_reset(sa);
        sa_load_weights(sa, W, 8, 8);
        sa_load_inputs(sa, X, 8, 8);
        sa_run_to_completion(sa);
        sa_stats_t s;
        sa_collect_stats(sa, &s);
        printf("  %s: %u cycles, w_reuse=%.1f, i_reuse=%.1f
",
               df_names[d], (uint32_t)s.cycles, s.weight_reuse_factor,
               s.input_reuse_factor);
    }
}

int main(void) {
    printf("========================================
");
    printf("  AI Accelerator Benchmark Suite
");
    printf("========================================

");

    /* Systolic array benchmarks */
    printf("[Systolic Array MatMul Benchmarks]
");
    systolic_array_t sa;
    sa_config_t cfg = sa_default_config();
    cfg.rows = 16; cfg.cols = 16; cfg.clock_ghz = 1.0;
    sa_init(&sa, &cfg);
    bench_matmul(&sa, 4);
    bench_matmul(&sa, 8);
    sa_free(&sa);

    printf("
[Dataflow Comparison]
");
    cfg.rows = 8; cfg.cols = 8;
    sa_init(&sa, &cfg);
    bench_dataflow(&sa);
    sa_free(&sa);

    /* PE benchmarks */
    printf("
[PE Pipeline Benchmarks]
");
    pe_state_t pe;
    pe_config_t pcfg = pe_default_config();
    pe_init(&pe, 0, 0, 0, &pcfg);
    pe_load_weight(&pe, 2.0);
    pe_load_input(&pe, 3.0);
    int i;
    clock_t start = clock();
    for (i = 0; i < 1000; i++) pe_cycle(&pe);
    clock_t end = clock();
    printf("  1000 PE cycles: %.2f ms (%.1f Mcycles/s)
",
           (double)(end-start)*1000/CLOCKS_PER_SEC,
           1000.0/((double)(end-start)/CLOCKS_PER_SEC)/1e6);
    printf("  MACs: %llu  Output: %.4f
",
           (unsigned long long)pe.mac_count, pe_read_output(&pe));

    /* Buffer hierarchy benchmarks */
    printf("
[Buffer Hierarchy Analysis]
");
    buffer_hierarchy_t bh;
    buf_hierarchy_init(&bh);
    buf_hierarchy_add_level(&bh, BUF_LEVEL_L1_LOCAL, "L1", 128*1024, 2, 400.0);
    buf_hierarchy_add_level(&bh, BUF_LEVEL_L2_GLOBAL, "L2", 1024*1024, 10, 200.0);
    buf_hierarchy_add_level(&bh, BUF_LEVEL_DRAM, "DRAM", 8*1024*1024, 100, 50.0);
    buf_hierarchy_print_layout(&bh);

    dma_load_weights(&bh, NULL, 64*1024);
    dma_wait_all(&bh.dma, &bh);
    buf_perf_t bperf;
    buf_hierarchy_collect_perf(&bh, &bperf);
    printf("  DMA: %llu bytes, %llu cycles, %.1f GB/s effective
",
           (unsigned long long)bh.dma.total_bytes_transferred,
           (unsigned long long)bh.dma.total_transfer_cycles,
           bh.dma.total_transfer_cycles > 0 ?
           (double)bh.dma.total_bytes_transferred / bh.dma.total_transfer_cycles : 0);

    /* Roofline comparison */
    printf("
[Roofline Model Comparison]
");
    roof_accel_spec_t specs[] = {
        {4096, 500.0, 100.0, 128, 2048, 1.0, 4096, 32, 1, 7.0, 150.0, 0.9},
        {16384, 800.0, 200.0, 256, 8192, 1.0, 16384, 32, 1, 5.0, 300.0, 0.7},
    };
    const char *names[] = {"Small Accel (TPUv1-like)", "Large Accel (TPUv3-like)"};
    for (int s = 0; s < 2; s++) {
        roofline_model_t rm;
        roofline_init(&rm, &specs[s]);
        roofline_compute_ceilings(&rm);
        printf("
  %s:
", names[s]);
        printf("    Peak: %.0f GFLOP/s, Ridge: %.1f FLOP/byte
",
               roofline_compute_peak_gflops(&specs[s]),
               roofline_compute_ridge_point(&specs[s]));
    }

    /* ISA program analysis */
    printf("
[ISA Program Analysis]
");
    dnn_program_t prog;
    dnn_program_init(&prog, "bench");
    int32_t r0 = dnn_alloc_register(&prog, "r0", DNN_DTYPE_FP16);
    int32_t r1 = dnn_alloc_register(&prog, "r1", DNN_DTYPE_FP16);
    int32_t r2 = dnn_alloc_register(&prog, "r2", DNN_DTYPE_FP16);
    dnn_emit_matmul(&prog, DNN_DTYPE_FP16, r2, r0, r1, 128, 128, 256);
    dnn_emit_scalar_op(&prog, DNN_OP_RELU, DNN_DTYPE_FP16, r2, r2, -1);
    dnn_emit_halt(&prog);
    dnn_isa_print_program(&prog);
    dnn_isa_stats_t istats;
    dnn_isa_collect_stats(&prog, &istats);
    dnn_isa_print_stats(&istats);

    printf("
========================================
");
    printf("  Benchmark Complete
");
    printf("========================================
");
    return 0;
}
