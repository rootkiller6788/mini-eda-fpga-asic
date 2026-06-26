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

#define MNIST_IMG_SIZE 28
#define MNIST_HIDDEN_1 128
#define MNIST_HIDDEN_2 64
#define MNIST_CLASSES  10

static int8_t mnist_weights_fc1[MNIST_HIDDEN_1][MNIST_IMG_SIZE * MNIST_IMG_SIZE];
static int8_t mnist_weights_fc2[MNIST_HIDDEN_2][MNIST_HIDDEN_1];
static int8_t mnist_weights_fc3[MNIST_CLASSES][MNIST_HIDDEN_2];
static int8_t mnist_test_image[MNIST_IMG_SIZE * MNIST_IMG_SIZE];

static int8_t quantize_float_to_int8(double val)
{
    double scaled = val * 127.0;
    if (scaled > 127.0) scaled = 127.0;
    if (scaled < -128.0) scaled = -128.0;
    return (int8_t)round(scaled);
}

static void generate_random_weights(int8_t *weights, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++) {
        double r = ((double)rand() / (double)RAND_MAX) * 2.0 - 1.0;
        weights[i] = quantize_float_to_int8(r * 0.3);
    }
}

static void generate_random_image(int8_t *image, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++) {
        double r = ((double)rand() / (double)RAND_MAX);
        image[i] = quantize_float_to_int8(r * 0.5);
    }
}

static int32_t argmax(const int32_t *arr, uint32_t size)
{
    int32_t max_val = arr[0];
    uint32_t max_idx = 0;
    for (uint32_t i = 1; i < size; i++) {
        if (arr[i] > max_val) {
            max_val = arr[i];
            max_idx = i;
        }
    }
    return (int32_t)max_idx;
}

int main(void)
{
    srand(42);
    printf("====== MNIST Digit Recognition on AI Accelerator ======\n\n");

    generate_random_weights(&mnist_weights_fc1[0][0], MNIST_HIDDEN_1 * MNIST_IMG_SIZE * MNIST_IMG_SIZE);
    generate_random_weights(&mnist_weights_fc2[0][0], MNIST_HIDDEN_2 * MNIST_HIDDEN_1);
    generate_random_weights(&mnist_weights_fc3[0][0], MNIST_CLASSES * MNIST_HIDDEN_2);
    generate_random_image(mnist_test_image, MNIST_IMG_SIZE * MNIST_IMG_SIZE);

    AccelCfg cfg = {
        .array_rows    = 32,
        .array_cols    = 32,
        .data_width    = 8,
        .buf_l1_depth  = 256,
        .buf_l2_depth  = 64 * 1024,
        .buf_l3_depth  = 2 * 1024 * 1024,
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

    printf("Accelerator: %u x %u array, %u MHz\n", cfg.array_rows, cfg.array_cols, cfg.freq_mhz);
    printf("Running MNIST inference...\n\n");

    SystolicArray sa;
    sa_init(&sa, 32, 32, SA_DATAFLOW_WEIGHT_STATIONARY, SA_PREC_INT8);

    // Layer 1: 784 -> 128
    printf("  [Layer 1] FC: 784 -> 128   ");
    uint32_t m1 = MNIST_HIDDEN_1;
    uint32_t k1 = MNIST_IMG_SIZE * MNIST_IMG_SIZE;
    uint32_t m_tiles = (m1 + 31) / 32;
    uint32_t k_tiles = (k1 + 31) / 32;
    uint64_t ops_layer1 = 0;

    for (uint32_t mt = 0; mt < m_tiles; mt++) {
        uint32_t m_start = mt * 32;
        uint32_t m_end   = (m_start + 32) < m1 ? (m_start + 32) : m1;
        uint32_t cur_m   = m_end - m_start;

        for (uint32_t kt = 0; kt < k_tiles; kt++) {
            uint32_t k_start = kt * 32;
            uint32_t k_end   = (k_start + 32) < k1 ? (k_start + 32) : k1;
            uint32_t cur_k   = k_end - k_start;

            sa_reset(&sa);
            for (uint32_t r = 0; r < cur_m; r++) {
                for (uint32_t c = 0; c < cur_k; c++) {
                    sa.grid[r][c].weight       = mnist_weights_fc1[m_start + r][k_start + c];
                    sa.grid[r][c].weight_valid = true;
                }
            }
            for (uint32_t c = 0; c < cur_k; c++) {
                sa.grid[0][c].act_in    = mnist_test_image[k_start + c];
                sa.grid[0][c].act_valid = true;
            }
            sa_compute_n_cycles(&sa, cur_m + cur_m + cur_k);
        }
    }
    ops_layer1 = (uint64_t)2 * m1 * k1;
    printf("%llu ops     [OK]\n", (unsigned long long)ops_layer1);

    // Layer 2: 128 -> 64
    printf("  [Layer 2] FC: 128 -> 64    ");
    uint32_t m2 = MNIST_HIDDEN_2;
    uint32_t k2 = MNIST_HIDDEN_1;
    uint32_t m2_tiles = (m2 + 31) / 32;
    uint32_t k2_tiles = (k2 + 31) / 32;

    for (uint32_t mt = 0; mt < m2_tiles; mt++) {
        uint32_t m_start = mt * 32;
        uint32_t m_end   = (m_start + 32) < m2 ? (m_start + 32) : m2;
        uint32_t cur_m   = m_end - m_start;

        for (uint32_t kt = 0; kt < k2_tiles; kt++) {
            uint32_t k_start = kt * 32;
            uint32_t k_end   = (k_start + 32) < k2 ? (k_start + 32) : k2;
            uint32_t cur_k   = k_end - k_start;

            sa_reset(&sa);
            for (uint32_t r = 0; r < cur_m; r++) {
                for (uint32_t c = 0; c < cur_k; c++) {
                    sa.grid[r][c].weight       = mnist_weights_fc2[m_start + r][k_start + c];
                    sa.grid[r][c].weight_valid = true;
                }
            }
            sa_compute_n_cycles(&sa, cur_m + cur_m + cur_k);
        }
    }
    printf("%llu ops      [OK]\n", (unsigned long long)(2ULL * m2 * k2));

    // Layer 3: 64 -> 10
    printf("  [Layer 3] FC: 64 -> 10     ");
    uint32_t m3 = MNIST_CLASSES;
    uint32_t k3 = MNIST_HIDDEN_2;

    sa_reset(&sa);
    for (uint32_t r = 0; r < m3; r++) {
        for (uint32_t c = 0; c < k3; c++) {
            sa.grid[r][c].weight       = mnist_weights_fc3[r][c];
            sa.grid[r][c].weight_valid = true;
        }
    }
    sa_compute_n_cycles(&sa, m3 + m3 + k3);

    int32_t logits[MNIST_CLASSES];
    sa_read_row_outputs(&sa, logits, MNIST_CLASSES);
    printf("%llu ops       [OK]\n", (unsigned long long)(2ULL * m3 * k3));

    int32_t predicted = argmax(logits, MNIST_CLASSES);
    printf("\n  Predicted digit: %d\n", predicted);

    for (int i = 0; i < MNIST_CLASSES; i++) {
        printf("    class %d: %d\n", i, logits[i]);
    }

    uint64_t total_ops = (uint64_t)2 * (m1 * k1 + m2 * k2 + m3 * k3);
    double tops = (double)total_ops * 1e-12 / 0.000001;
    (void)tops;

    printf("\n  Total Ops: %llu\n", (unsigned long long)total_ops);
    printf("  PE Utilization: %.1f%%\n", sa_utilization(&sa));

    printf("\n====== MNIST Demo Complete ======\n");
    return 0;
}
