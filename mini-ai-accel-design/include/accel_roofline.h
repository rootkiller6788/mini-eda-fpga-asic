#ifndef ACCEL_ROOFLINE_H
#define ACCEL_ROOFLINE_H
#include <stdbool.h>

typedef struct {
    double peak_compute_tflops;
    double peak_bandwidth_gbps;
    double operational_intensity; /* FLOPs/byte */
    double achieved_performance_tflops;
    bool compute_bound;
} RooflineModel;

void roofline_init(RooflineModel *rm, double peak_tflops, double peak_bw);
bool roofline_compute_bound(RooflineModel *rm, double op_intensity);
bool roofline_memory_bound(RooflineModel *rm, double op_intensity);
double roofline_achieved(RooflineModel *rm, double op_intensity);
void roofline_optimization_suggest(RooflineModel *rm, double op_intensity);
void roofline_report(RooflineModel *rm, double op_intensity);
#endif
