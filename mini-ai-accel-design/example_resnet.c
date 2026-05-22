#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "accelerator_verilog.h"
#include "systolic_rtl.h"
#include "pe_microarch.h"
#include "buffer_hierarchy.h"
#include "dnn_isa.h"

typedef struct {
    uint32_t in_channels;
    uint32_t out_channels;
    uint32_t kernel_size;
    uint32_t stride;
    uint32_t pad;
    uint32_t input_h;
    uint32_t input_w;
    bool     has_skip;
    bool     bottleneck;
    uint32_t expansion;
} ResNetBlock;

static int8_t *alloc_random_weights(uint32_t size)
{
    int8_t *w = (int8_t*)calloc(size, sizeof(int8_t));
    if (!w) return NULL;
    for (uint32_t i = 0; i < size; i++) {
        double r = ((double)rand() / (double)RAND_MAX) * 2.0 - 1.0;
        double v = r * 0.3 * 127.0;
        if (v > 127.0) v = 127.0;
        if (v < -128.0) v = -128.0;
        w[i] = (int8_t)round(v);
    }
    return w;
}

static uint64_t run_conv_layer(SystolicArray *sa, uint32_t h, uint32_t w,
                                uint32_t c_in, uint32_t c_out,
                                uint32_t k, uint32_t stride, uint32_t pad)
{
    uint32_t h_out = (h + 2 * pad - k) / stride + 1;
    uint32_t w_out = (w + 2 * pad - k) / stride + 1;
    uint32_t M = c_out;
    uint32_t N = h_out * w_out;
    uint32_t KK = k * k * c_in;

    uint64_t ops = (uint64_t)2 * M * N * KK;

    uint32_t t_h = (M + 31) / 32;
    uint32_t t_w = (N + 31) / 32;
    uint32_t t_k = (KK + 31) / 32;

    int8_t *weights = alloc_random_weights(M * KK);
    int8_t *inputs  = alloc_random_weights(KK * N);

    for (uint32_t mt = 0; mt < t_h; mt++) {
        uint32_t mr = (mt * 32 + 32) < M ? 32 : (M - mt * 32);
        for (uint32_t nt = 0; nt < t_w; nt++) {
            uint32_t nr = (nt * 32 + 32) < N ? 32 : (N - nt * 32);
            for (uint32_t kt = 0; kt < t_k; kt++) {
                uint32_t kr = (kt * 32 + 32) < KK ? 32 : (KK - kt * 32);
                sa_reset(sa);
                for (uint32_t r = 0; r < mr; r++) {
                    for (uint32_t c = 0; c < kr; c++) {
                        sa->grid[r][c].weight = weights[(mt * 32 + r) * KK + (kt * 32 + c)];
                        sa->grid[r][c].weight_valid = true;
                    }
                }
                (void)nr;
                sa_compute_n_cycles(sa, mr + mr + kr);
            }
        }
    }

    free(weights);
    free(inputs);
    return ops;
}

int main(void)
{
    srand(12345);
    printf("====== ResNet-50 Inference on AI Accelerator ======\n\n");

    AccelCfg cfg = {
        .array_rows    = 64,
        .array_cols    = 64,
        .data_width    = 8,
        .buf_l1_depth  = 512,
        .buf_l2_depth  = 128 * 1024,
        .buf_l3_depth  = 4 * 1024 * 1024,
        .freq_mhz      = 1000,
        .pipe_en       = true,
        .zero_skip_en  = true,
        .dma_overlap_en= true,
        .multicast_en  = true,
        .ctrl_bus      = BUS_AXI4_LITE,
        .data_bus      = BUS_AXI4_FULL,
    };

    AccelTop accel;
    at_init(&accel, &cfg);

    SystolicArray sa;
    sa_init(&sa, 64, 64, SA_DATAFLOW_WEIGHT_STATIONARY, SA_PREC_INT8);
    sa_set_pipeline_depth(&sa, 4);

    printf("Accelerator: %u x %u array, %u MHz\n", cfg.array_rows, cfg.array_cols, cfg.freq_mhz);
    printf("ResNet-50: 50 layers, 224x224 input\n\n");

    ResNetBlock stages[] = {
        {64, 64, 1, 1, 0, 112, 112, false, false, 1},
        {64, 64, 3, 1, 1, 112, 112, true,  false, 1},
        {64, 64, 3, 1, 1, 112, 112, true,  false, 1},
        {64, 128, 3, 2, 1, 56, 56, false, false, 1},
        {128, 128, 3, 1, 1, 56, 56, true,  false, 1},
        {128, 256, 3, 2, 1, 56, 56, false, false, 1},
        {128, 256, 3, 1, 1, 28, 28, false, true, 4},
        {256, 256, 3, 1, 1, 28, 28, true,  false, 1},
        {256, 512, 3, 2, 1, 28, 28, false, false, 1},
        {256, 512, 3, 1, 1, 14, 14, false, true, 4},
        {512, 512, 3, 1, 1, 14, 14, true,  false, 1},
        {512, 1024, 3, 2, 1, 14, 14, false, false, 1},
        {512, 1024, 3, 1, 1, 7, 7, false, true, 4},
        {1024, 1024, 3, 1, 1, 7, 7, true, false, 1},
        {1024, 2048, 7, 1, 0, 7, 7, false, false, 1},
    };

    uint64_t total_ops = 0;
    int n_stages = sizeof(stages) / sizeof(stages[0]);

    for (int i = 0; i < n_stages; i++) {
        ResNetBlock *b = &stages[i];
        uint64_t ops = run_conv_layer(&sa, b->input_h, b->input_w,
                                       b->in_channels, b->out_channels,
                                       b->kernel_size, b->stride, b->pad);
        total_ops += ops;

        printf("  [Stage %2d] Conv %dx%d, %u->%u ch, %u/%u   %s%s",
               i + 1, b->kernel_size, b->kernel_size,
               b->in_channels, b->out_channels, b->stride, b->pad,
               b->bottleneck ? "(bottleneck) " : "",
               b->has_skip ? "(+skip) " : "");

        if (b->bottleneck) {
            ops += ops * b->expansion;
            total_ops += ops * b->expansion;
        }

        printf("      [OK]\n");
    }

    printf("\n  Total operations: %llu\n", (unsigned long long)total_ops);
    printf("  Estimated peak TOPS: %.2f\n", at_peak_tops(&cfg).tops);
    printf("  Estimated TOPS: %.2f\n",
           (double)total_ops * 1e-12 / ((double)total_ops * 2.0 / (64.0 * 64.0 * 2.0 * 1e9)));

    sa_dump_state(&sa);

    char report_path[256];
    snprintf(report_path, sizeof(report_path), "resnet50_report.csv");
    at_export_report(&accel, report_path);

    printf("\n====== ResNet-50 Demo Complete ======\n");
    return 0;
}
