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

#define BERT_SEQ_LEN    128
#define BERT_D_MODEL    768
#define BERT_HEADS      12
#define BERT_FFN        3072
#define BERT_LAYERS     12

static double drand_norm(void)
{
    double u1 = (double)rand() / (double)RAND_MAX;
    double u2 = (double)rand() / (double)RAND_MAX;
    return sqrt(-2.0 * log(u1 + 1e-10)) * cos(2.0 * M_PI * u2);
}

static int8_t quantize(double val)
{
    double v = val * 127.0;
    if (v > 127.0) v = 127.0;
    if (v < -128.0) v = -128.0;
    return (int8_t)round(v);
}

static void generate_weights_normal(int8_t *w, uint32_t rows, uint32_t cols)
{
    double scale = 1.0 / sqrt((double)cols);
    for (uint32_t i = 0; i < rows * cols; i++) {
        w[i] = quantize(drand_norm() * scale);
    }
}

typedef struct {
    int8_t *Q_weight;
    int8_t *K_weight;
    int8_t *V_weight;
    int8_t *O_weight;
    int8_t *ffn1_weight;
    int8_t *ffn2_weight;
    uint32_t d_model;
    uint32_t heads;
    uint32_t d_head;
    uint32_t ffn_dim;
} BertLayer;

static BertLayer *bert_layer_create(uint32_t d_model, uint32_t heads, uint32_t ffn)
{
    BertLayer *l = (BertLayer*)calloc(1, sizeof(BertLayer));
    if (!l) return NULL;

    uint32_t d_head = d_model / heads;
    l->d_model = d_model;
    l->heads   = heads;
    l->d_head  = d_head;
    l->ffn_dim = ffn;

    l->Q_weight = (int8_t*)malloc(d_model * d_model * sizeof(int8_t));
    l->K_weight = (int8_t*)malloc(d_model * d_model * sizeof(int8_t));
    l->V_weight = (int8_t*)malloc(d_model * d_model * sizeof(int8_t));
    l->O_weight = (int8_t*)malloc(d_model * d_model * sizeof(int8_t));
    l->ffn1_weight = (int8_t*)malloc(d_model * ffn * sizeof(int8_t));
    l->ffn2_weight = (int8_t*)malloc(ffn * d_model * sizeof(int8_t));

    generate_weights_normal(l->Q_weight, d_model, d_model);
    generate_weights_normal(l->K_weight, d_model, d_model);
    generate_weights_normal(l->V_weight, d_model, d_model);
    generate_weights_normal(l->O_weight, d_model, d_model);
    generate_weights_normal(l->ffn1_weight, d_model, ffn);
    generate_weights_normal(l->ffn2_weight, ffn, d_model);

    return l;
}

static void bert_layer_free(BertLayer *l)
{
    if (!l) return;
    free(l->Q_weight);
    free(l->K_weight);
    free(l->V_weight);
    free(l->O_weight);
    free(l->ffn1_weight);
    free(l->ffn2_weight);
    free(l);
}

static uint64_t run_self_attention(SystolicArray *sa, const BertLayer *layer, uint32_t seq_len)
{
    uint32_t M = seq_len;
    uint32_t N = layer->d_head;
    uint32_t K = layer->d_model;
    uint64_t ops = 0;

    int8_t *proj_weights[] = {layer->Q_weight, layer->K_weight, layer->V_weight};
    for (int p = 0; p < 3; p++) {
        for (uint32_t mt = 0; mt < (M + 31) / 32; mt++) {
            uint32_t mr = (mt * 32 + 32) < M ? 32 : (M - mt * 32);
            for (uint32_t kt = 0; kt < (K + 31) / 32; kt++) {
                uint32_t kr = (kt * 32 + 32) < K ? 32 : (K - kt * 32);
                sa_reset(sa);
                for (uint32_t r = 0; r < mr; r++) {
                    for (uint32_t c = 0; c < kr; c++) {
                        sa->grid[r][c].weight = proj_weights[p][(mt * 32 + r) * K + (kt * 32 + c)];
                        sa->grid[r][c].weight_valid = true;
                    }
                }
                sa_compute_n_cycles(sa, mr + mr + kr);
            }
        }
    }
    ops += (uint64_t)3 * 2 * M * N * K;

    // Q x K^T
    uint32_t att_M = seq_len, att_K = layer->d_head, att_N = seq_len;
    for (uint32_t mt = 0; mt < (att_M + 31) / 32; mt++) {
        uint32_t mr = (mt * 32 + 32) < att_M ? 32 : (att_M - mt * 32);
        for (uint32_t kt = 0; kt < (att_K + 31) / 32; kt++) {
            uint32_t kr = (kt * 32 + 32) < att_K ? 32 : (att_K - kt * 32);
            sa_reset(sa);
            sa_compute_n_cycles(sa, mr + mr + kr);
        }
    }
    ops += (uint64_t)2 * att_M * att_K * att_N;

    // Attention x V
    ops += (uint64_t)2 * seq_len * seq_len * layer->d_head;

    // Output projection
    att_M = seq_len; att_K = layer->d_model; att_N = layer->d_model;
    for (uint32_t mt = 0; mt < (att_M + 31) / 32; mt++) {
        uint32_t mr = (mt * 32 + 32) < att_M ? 32 : (att_M - mt * 32);
        for (uint32_t kt = 0; kt < (att_K + 31) / 32; kt++) {
            uint32_t kr = (kt * 32 + 32) < att_K ? 32 : (att_K - kt * 32);
            sa_reset(sa);
            for (uint32_t r = 0; r < mr; r++) {
                for (uint32_t c = 0; c < kr; c++) {
                    sa->grid[r][c].weight = layer->O_weight[(mt * 32 + r) * att_K + (kt * 32 + c)];
                    sa->grid[r][c].weight_valid = true;
                }
            }
            sa_compute_n_cycles(sa, mr + mr + kr);
        }
    }
    ops += (uint64_t)2 * seq_len * layer->d_model * layer->d_model;

    return ops;
}

static uint64_t run_ffn(SystolicArray *sa, const BertLayer *layer, uint32_t seq_len)
{
    uint64_t ops = 0;

    // FFN layer 1: d_model -> ffn_dim
    uint32_t M = seq_len, K = layer->d_model, N_dest = layer->ffn_dim;
    for (uint32_t mt = 0; mt < (M + 31) / 32; mt++) {
        uint32_t mr = (mt * 32 + 32) < M ? 32 : (M - mt * 32);
        for (uint32_t kt = 0; kt < (K + 31) / 32; kt++) {
            uint32_t kr = (kt * 32 + 32) < K ? 32 : (K - kt * 32);
            sa_reset(sa);
            for (uint32_t r = 0; r < mr; r++) {
                for (uint32_t c = 0; c < kr; c++) {
                    sa->grid[r][c].weight = layer->ffn1_weight[(mt * 32 + r) * K + (kt * 32 + c)];
                    sa->grid[r][c].weight_valid = true;
                }
            }
            sa_compute_n_cycles(sa, mr + mr + kr);
        }
    }
    ops += (uint64_t)2 * M * K * N_dest;

    // FFN layer 2: ffn_dim -> d_model
    K = layer->ffn_dim; N_dest = layer->d_model;
    for (uint32_t mt = 0; mt < (M + 31) / 32; mt++) {
        uint32_t mr = (mt * 32 + 32) < M ? 32 : (M - mt * 32);
        for (uint32_t kt = 0; kt < (K + 31) / 32; kt++) {
            uint32_t kr = (kt * 32 + 32) < K ? 32 : (K - kt * 32);
            sa_reset(sa);
            for (uint32_t r = 0; r < mr; r++) {
                for (uint32_t c = 0; c < kr; c++) {
                    sa->grid[r][c].weight = layer->ffn2_weight[(mt * 32 + r) * K + (kt * 32 + c)];
                    sa->grid[r][c].weight_valid = true;
                }
            }
            sa_compute_n_cycles(sa, mr + mr + kr);
        }
    }
    ops += (uint64_t)2 * M * K * N_dest;

    return ops;
}

int main(void)
{
    srand(42);
    printf("====== BERT-Base Inference on AI Accelerator ======\n\n");

    AccelCfg cfg = {
        .array_rows    = 64,
        .array_cols    = 64,
        .data_width    = 8,
        .buf_l1_depth  = 512,
        .buf_l2_depth  = 256 * 1024,
        .buf_l3_depth  = 8 * 1024 * 1024,
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

    printf("BERT-Base: %d layers, d_model=%d, heads=%d, seq_len=%d\n",
           BERT_LAYERS, BERT_D_MODEL, BERT_HEADS, BERT_SEQ_LEN);
    printf("Accelerator: %u x %u array, %u MHz\n\n",
           cfg.array_rows, cfg.array_cols, cfg.freq_mhz);

    BertLayer *layers[BERT_LAYERS];
    for (int i = 0; i < BERT_LAYERS; i++) {
        layers[i] = bert_layer_create(BERT_D_MODEL, BERT_HEADS, BERT_FFN);
        printf("  [Layer %2d] Weights allocated, d_model=%d, heads=%d\n",
               i + 1, layers[i]->d_model, layers[i]->heads);
    }

    uint64_t total_ops = 0;
    struct timespec t_start, t_end;
    timespec_get(&t_start, TIME_UTC);

    for (int i = 0; i < BERT_LAYERS; i++) {
        printf("  [Layer %2d] Multi-Head Self-Attention ... ", i + 1);
        uint64_t attn_ops = run_self_attention(&sa, layers[i], BERT_SEQ_LEN);
        printf("%llu ops    ", (unsigned long long)attn_ops);

        printf("FFN ... ");
        uint64_t ffn_ops = run_ffn(&sa, layers[i], BERT_SEQ_LEN);
        printf("%llu ops    [OK]\n", (unsigned long long)ffn_ops);

        total_ops += attn_ops + ffn_ops;
    }

    timespec_get(&t_end, TIME_UTC);
    double elapsed = (t_end.tv_sec - t_start.tv_sec)
                     + (t_end.tv_nsec - t_start.tv_nsec) * 1e-9;

    printf("\n===== Results =====\n");
    printf("  Total operations: %llu\n", (unsigned long long)total_ops);
    printf("  Wall time: %.3f s\n", elapsed);
    printf("  Effective TOPS: %.4f\n", (double)total_ops * 1e-12 / elapsed);

    TOPS peak = at_peak_tops(&cfg);
    printf("  Peak TOPS: %.4f\n", peak.tops);
    printf("  Efficiency: %.1f%%\n",
           (((double)total_ops * 1e-12 / elapsed) / peak.tops) * 100.0);

    sa_dump_state(&sa);

    for (int i = 0; i < BERT_LAYERS; i++) bert_layer_free(layers[i]);

    printf("\n====== BERT Demo Complete ======\n");
    return 0;
}
