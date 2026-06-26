/* example_bert.c - BERT attention on AI accelerator
 * Demonstrates: MatMul-heavy transformer workload mapping,
 * multi-head attention ISA sequence, performance analysis.
 * L6: Classic NLP problem, L7: Application */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dnn_isa.h"
#include "systolic_array.h"
#include "pe_microarch.h"
#include "accel_roofline.h"

typedef struct {
    uint32_t batch, seq_len, d_model, n_heads;
    double total_flops;
} bert_config_t;

int main(void) {
    printf("=== BERT-Base Attention on AI Accelerator ===
");

    bert_config_t bert = {1, 128, 768, 12, 0};
    uint32_t d_k = bert.d_model / bert.n_heads;

    printf("Config: batch=%u seq=%u d_model=%u heads=%u d_k=%u
", bert.batch, bert.seq_len, bert.d_model, bert.n_heads, d_k);

    /* Compute FLOPs for self-attention: Q*K^T * V */
    double qkv_proj = 3.0 * bert.seq_len * bert.d_model * bert.d_model * 2;
    double attn_scores = bert.n_heads * bert.seq_len * bert.seq_len * d_k * 2;
    double attn_output = bert.seq_len * bert.seq_len * d_k * 2;
    double output_proj = bert.seq_len * bert.d_model * bert.d_model * 2;
    bert.total_flops = qkv_proj + attn_scores + attn_output + output_proj;

    printf("FLOPs breakdown:
");
    printf("  QKV projection:   %.2e
", qkv_proj);
    printf("  Attention scores: %.2e
", attn_scores);
    printf("  Attention output: %.2e
", attn_output);
    printf("  Output projection:%.2e
", output_proj);
    printf("  TOTAL:            %.2e FLOPs
", bert.total_flops);

    /* Build ISA program for attention */
    dnn_program_t prog;
    dnn_program_init(&prog, "bert_attention");
    int32_t r_q = dnn_alloc_register(&prog, "Q", DNN_DTYPE_FP16);
    int32_t r_k = dnn_alloc_register(&prog, "K", DNN_DTYPE_FP16);
    int32_t r_v = dnn_alloc_register(&prog, "V", DNN_DTYPE_FP16);
    int32_t r_s = dnn_alloc_register(&prog, "S", DNN_DTYPE_FP32);
    int32_t r_a = dnn_alloc_register(&prog, "A", DNN_DTYPE_FP32);
    int32_t r_o = dnn_alloc_register(&prog, "O", DNN_DTYPE_FP32);

    /* Q * K^T */
    dnn_emit_matmul(&prog, DNN_DTYPE_FP16, r_s, r_q, r_k,
                     bert.seq_len, bert.seq_len, d_k);
    /* Softmax (approximated as ReLU + normalization) */
    dnn_emit_scalar_op(&prog, DNN_OP_RELU, DNN_DTYPE_FP32, r_s, r_s, -1);
    /* S * V */
    dnn_emit_matmul(&prog, DNN_DTYPE_FP16, r_a, r_s, r_v,
                     bert.seq_len, d_k, bert.seq_len);
    /* Output projection */
    dnn_emit_matmul(&prog, DNN_DTYPE_FP16, r_o, r_a, r_a,
                     bert.seq_len, bert.d_model, d_k);
    dnn_emit_halt(&prog);

    dnn_isa_stats_t stats;
    dnn_isa_collect_stats(&prog, &stats);
    printf("ISA: %u instrs, %u matrix ops, %u cycles, AI=%.2f FLOP/byte
", stats.total_instr, stats.matrix_ops, stats.cycles, stats.arithmetic_intensity);

    /* Roofline analysis */
    roof_accel_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    spec.num_mac_units = 16384; spec.clock_ghz = 1.0; spec.num_cores = 1;
    spec.peak_dram_bw_gb_s = 200.0; spec.peak_sram_bw_gb_s = 800.0;
    spec.l1_size_kb = 256; spec.l2_size_kb = 8192;

    roofline_model_t rm;
    roofline_init(&rm, &spec);
    roofline_compute_ceilings(&rm);

    double bytes = bert.seq_len * bert.d_model * 4 * 5;
    double time_est = bert.total_flops / (roofline_compute_peak_gflops(&spec) * 1e9);
    roofline_add_point(&rm, "BERT_Attention", bert.total_flops, bytes, time_est);
    roofline_print_summary(&rm);

    /* Systolic array for matmul */
    systolic_array_t sa;
    sa_config_t cfg = sa_default_config();
    cfg.rows = 64; cfg.cols = 64;
    cfg.clock_ghz = 1.0;
    sa_init(&sa, &cfg);

    double test_W[64], test_X[64], test_Y[64];
    for (int i = 0; i < 8; i++) test_W[i*8+i] = 1.0;
    for (int i = 0; i < 8; i++) test_X[i] = 3.0;
    sa_map_weight_stationary(&sa, test_W, 8, 8, test_X, 8, 8, test_Y);

    sa_stats_t sas;
    sa_collect_stats(&sa, &sas);
    printf(" SA throughput: %.3f TOPS Util: %.1f%%
", sas.throughput_tops, sas.utilization*100);

    double latency_ms = bert.total_flops / (sas.throughput_tops * 1e12) * 1000;
    printf("Estimated attention latency: %.2f ms
", latency_ms);
    sa_free(&sa);

    printf(" === BERT Demo Complete ===
");
    return 0;
}
