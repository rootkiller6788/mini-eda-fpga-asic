#include "accel_roofline.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void roofline_init(RooflineModel *rm, double peak_tflops, double peak_bw) {
    rm->peak_compute_tflops = peak_tflops;
    rm->peak_bandwidth_gbps = peak_bw;
    rm->operational_intensity = 0;
    rm->achieved_performance_tflops = 0;
    rm->compute_bound = false;
}

bool roofline_compute_bound(RooflineModel *rm, double op_intensity) {
    double ridge_point = rm->peak_compute_tflops / rm->peak_bandwidth_gbps * 1e3; /* TFLOPS / (GB/s) = FLOPS/Byte * 1e3 */
    return op_intensity >= ridge_point;
}

bool roofline_memory_bound(RooflineModel *rm, double op_intensity) {
    return !roofline_compute_bound(rm, op_intensity);
}

double roofline_achieved(RooflineModel *rm, double op_intensity) {
    double compute_roof = rm->peak_compute_tflops;
    double memory_roof = rm->peak_bandwidth_gbps * op_intensity * 1e-3;
    rm->operational_intensity = op_intensity;
    rm->achieved_performance_tflops = fmin(compute_roof, memory_roof);
    rm->compute_bound = roofline_compute_bound(rm, op_intensity);
    return rm->achieved_performance_tflops;
}

void roofline_optimization_suggest(RooflineModel *rm, double op_intensity) {
    double achieved = roofline_achieved(rm, op_intensity);
    printf("  Optimization suggestions:\n");
    if (rm->compute_bound) {
        printf("    Compute bound: increase parallelism, use more PEs or higher frequency\n");
        printf("    Consider quantization (FP16/INT8) for higher throughput\n");
    } else {
        printf("    Memory bound: reduce data movement, increase reuse\n");
        printf("    Consider larger buffers, tiling, or dataflow optimization\n");
        double gap = (rm->peak_compute_tflops - achieved) / rm->peak_compute_tflops * 100;
        printf("    %.1f%% of peak unused due to memory bottleneck\n", gap);
    }
    (void)achieved;
}

void roofline_report(RooflineModel *rm, double op_intensity) {
    double achieved = roofline_achieved(rm, op_intensity);
    printf("=== Roofline Analysis ===\n");
    printf("  Peak compute: %.3f TFLOPS\n", rm->peak_compute_tflops);
    printf("  Peak bandwidth: %.1f GB/s\n", rm->peak_bandwidth_gbps);
    printf("  Op intensity: %.3f FLOPS/byte\n", op_intensity);
    printf("  Achieved: %.3f TFLOPS (%.1f%% of peak)\n", achieved, rm->peak_compute_tflops > 0 ? achieved / rm->peak_compute_tflops * 100 : 0);
    printf("  Bottleneck: %s\n", rm->compute_bound ? "COMPUTE" : "MEMORY");
    roofline_optimization_suggest(rm, op_intensity);
}
