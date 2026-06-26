/* example_resnet.c - ResNet-18 layer mapping on AI accelerator
 * Demonstrates: Conv2D ISA mapping, tiled execution,
 * roofline analysis for convolution layers.
 * L6: Classic CNN problem, L7: Application */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dnn_isa.h"
#include "systolic_array.h"
#include "pe_microarch.h"
#include "accel_roofline.h"

typedef struct {
    uint32_t H, W, C_in, C_out, KH, KW, stride;
    double flops, weights_bytes, act_bytes;
} conv_layer_t;

int main(void) {
    printf("=== ResNet-18 Layer Mapping on AI Accelerator ===
");

    conv_layer_t layers[] = {
        {224,224,3,64,7,7,2,  0,0,0},
        {56,56,64,64,3,3,1,   0,0,0},
        {56,56,64,128,3,3,2,  0,0,0},
        {28,28,128,256,3,3,2, 0,0,0},
        {14,14,256,512,3,3,2, 0,0,0},
    };
    int n_layers = sizeof(layers)/sizeof(layers[0]);

    for (int i = 0; i < n_layers; i++) {
        conv_layer_t *L = &layers[i];
        uint32_t OH = (L->H - L->KH) / L->stride + 1;
        uint32_t OW = (L->W - L->KW) / L->stride + 1;
        L->flops = (double)OH * OW * L->C_out * L->KH * L->KW * L->C_in * 2;
        L->weights_bytes = L->C_out * L->C_in * L->KH * L->KW * 2;
        L->act_bytes = L->H * L->W * L->C_in * 2;
        printf("Layer %d: %ux%ux%u -> %ux%ux%u (kernel %ux%u, s=%u)
", i, L->H, L->W, L->C_in, OH, OW, L->C_out, L->KH, L->KW, L->stride);
        printf("  FLOPs: %.2e  Weights: %.1f KB  Activations: %.1f KB
", L->flops, L->weights_bytes/1024.0, L->act_bytes/1024.0);
    }

    /* Roofline analysis */
    roof_accel_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    spec.num_mac_units = 4096; spec.clock_ghz = 1.0; spec.num_cores = 1;
    spec.peak_dram_bw_gb_s = 100.0; spec.peak_sram_bw_gb_s = 500.0;
    spec.l1_size_kb = 128; spec.l2_size_kb = 2048;

    roofline_model_t rm;
    roofline_init(&rm, &spec);
    roofline_compute_ceilings(&rm);

    printf(" [Roofline Analysis]
");
    for (int i = 0; i < n_layers; i++) {
        conv_layer_t *L = &layers[i];
        double bytes = L->weights_bytes + L->act_bytes;
        double time_est = L->flops / (roofline_compute_peak_gflops(&spec) * 1e9);
        char name[32];
        snprintf(name, sizeof(name), "Conv_L%d", i);
        roofline_add_point(&rm, name, L->flops, bytes, time_est);
        printf("  %s: I=%.1f FLOP/byte %s
", name, L->flops/bytes, rm.points[i].compute_bound ? "[compute]" : "[memory]");
    }
    roofline_print_summary(&rm);

    /* Systolic array: simulate conv as im2col + matmul */
    systolic_array_t sa;
    sa_config_t cfg = sa_default_config();
    cfg.rows = 32; cfg.cols = 32;
    sa_init(&sa, &cfg);
    printf(" Systolic array: %ux%u (peak %.0f TOPS)
", cfg.rows, cfg.cols, (double)cfg.rows * cfg.cols * 2 * cfg.clock_ghz / 1000.0);

    double W_test[256], X_test[256], Y_test[256];
    memset(W_test, 0, sizeof(W_test));
    for (int j = 0; j < 16; j++) W_test[j*16+j] = 1.0;
    for (int j = 0; j < 16; j++) X_test[j] = 2.0;
    sa_map_weight_stationary(&sa, W_test, 16, 16, X_test, 16, 16, Y_test);

    sa_stats_t sas;
    sa_collect_stats(&sa, &sas);
    printf("  Conv layer 0 sim: MACs=%llu Util=%.1f%%
", (unsigned long long)sas.mac_operations, sas.utilization*100);
    sa_free(&sa);

    printf(" === ResNet Demo Complete ===
");
    return 0;
}
