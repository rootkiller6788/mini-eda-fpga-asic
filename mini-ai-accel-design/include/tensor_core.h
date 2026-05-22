#ifndef TENSOR_CORE_H
#define TENSOR_CORE_H
#include <stdint.h>
#include <stdbool.h>

#define TC_MATRIX_M 4
#define TC_MATRIX_N 4
#define TC_MATRIX_K 4
#define TC_WARP_SIZE 32

typedef enum { TC_FP16, TC_FP32, TC_INT8, TC_BF16 } TensorPrecision;

typedef struct { double (*data)[TC_MATRIX_N]; int m, n; } TensorTile;

typedef struct {
    TensorTile a, b, c;
    int cycles_per_mma;
    double throughput_tflops;
    TensorPrecision precision;
    bool mixed_precision;
} TensorCore;

void tensor_core_init(TensorCore *tc, TensorPrecision prec);
void tensor_core_mma(TensorCore *tc);  /* matrix multiply-accumulate */
void tensor_core_warp_schedule(TensorCore *tc, int num_warps);
double tensor_core_throughput(TensorCore *tc); /* TFLOPS */
double tensor_core_energy_per_mma(TensorCore *tc); /* nJ */
void tensor_core_print(TensorCore *tc);
#endif
