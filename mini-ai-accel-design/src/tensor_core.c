#include "tensor_core.h"
#include <stdio.h>
#include <string.h>

void tensor_core_init(TensorCore *tc, TensorPrecision prec) {
    memset(tc, 0, sizeof(*tc));
    tc->precision = prec; tc->mixed_precision = (prec == TC_FP16);
    tc->cycles_per_mma = (prec == TC_FP32) ? 4 : 2;
    tc->a.m = TC_MATRIX_M; tc->a.n = TC_MATRIX_K;
    tc->b.m = TC_MATRIX_K; tc->b.n = TC_MATRIX_N;
    tc->c.m = TC_MATRIX_M; tc->c.n = TC_MATRIX_N;
}

void tensor_core_mma(TensorCore *tc) {
    /* Simulated MMA: C += A * B (MxK * KxN = MxN) */
    int M = tc->a.m, N = tc->b.n, K = tc->a.n;
    double total_ops = 2.0 * M * N * K;
    double ops_per_cycle = (double)(M * N * K) / tc->cycles_per_mma;
    tc->throughput_tflops = ops_per_cycle * 1e9 * 1e-12; /* assuming 1GHz */
    (void)total_ops;
}

void tensor_core_warp_schedule(TensorCore *tc, int num_warps) {
    /* Warp-level parallelism: warp scheduler issues MMA instructions */
    int issue_rate = num_warps / 4; /* 4 warps per scheduler */
    if (issue_rate < 1) issue_rate = 1;
    tc->cycles_per_mma = (tc->cycles_per_mma * TC_WARP_SIZE) / (num_warps * issue_rate);
    if (tc->cycles_per_mma < 1) tc->cycles_per_mma = 1;
}

double tensor_core_throughput(TensorCore *tc) {
    tensor_core_mma(tc);
    return tc->throughput_tflops;
}

double tensor_core_energy_per_mma(TensorCore *tc) {
    switch (tc->precision) {
        case TC_INT8:  return 0.02;
        case TC_FP16: case TC_BF16: return 0.05;
        case TC_FP32: return 0.1;
        default: return 0.05;
    }
}

void tensor_core_print(TensorCore *tc) {
    const char *prec[] = {"FP16","FP32","INT8","BF16"};
    printf("=== Tensor Core ===\n");
    printf("  Precision: %s, MMA: %dx%dx%d\n", prec[tc->precision], TC_MATRIX_M, TC_MATRIX_N, TC_MATRIX_K);
    printf("  Throughput: %.3f TFLOPS, Cycles/MMA: %d\n", tensor_core_throughput(tc), tc->cycles_per_mma);
    printf("  Energy/MMA: %.3f nJ\n", tensor_core_energy_per_mma(tc));
}
