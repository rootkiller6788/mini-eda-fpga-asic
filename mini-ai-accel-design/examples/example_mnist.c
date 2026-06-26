/* example_mnist.c - MNIST digit classification on AI accelerator
 * Demonstrates: ISA programming, systolic matmul, PE activation,
 * buffer hierarchy for a 2-layer MLP (784-256-10).
 * L6: Classic ML problem, L7: Application example */
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

int main(void) {
    printf("=== MNIST Inference on AI Accelerator ===
");

    /* 1. Configure accelerator with roofline analysis */
    roof_accel_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    spec.num_mac_units = 256; spec.clock_ghz = 1.0; spec.num_cores = 1;
    spec.peak_dram_bw_gb_s = 50.0; spec.peak_sram_bw_gb_s = 200.0;
    spec.l1_size_kb = 64; spec.l2_size_kb = 512; spec.process_node_nm = 7.0;
    printf("Accelerator: %u MACs @ %.1f GHz = %.0f GFLOP/s peak
", spec.num_mac_units, spec.clock_ghz, roofline_compute_peak_gflops(&spec));

    /* 2. Build ISA program for MNIST (simplified) */
    dnn_program_t prog;
    dnn_program_init(&prog, "mnist_inference");
    int32_t r_img = dnn_alloc_register(&prog, "image", DNN_DTYPE_FP32);
    int32_t r_w1  = dnn_alloc_register(&prog, "w1", DNN_DTYPE_FP32);
    int32_t r_h1  = dnn_alloc_register(&prog, "h1", DNN_DTYPE_FP32);
    int32_t r_w2  = dnn_alloc_register(&prog, "w2", DNN_DTYPE_FP32);
    int32_t r_out = dnn_alloc_register(&prog, "out", DNN_DTYPE_FP32);

    dnn_emit_load(&prog, DNN_DTYPE_FP32, r_img, 0x1000, 784*4);
    dnn_emit_load(&prog, DNN_DTYPE_FP32, r_w1,  0x2000, 784*256*4);
    dnn_emit_load(&prog, DNN_DTYPE_FP32, r_w2,  0x3000, 256*10*4);
    dnn_emit_matmul(&prog, DNN_DTYPE_FP32, r_h1, r_img, r_w1, 1, 256, 784);
    dnn_emit_scalar_op(&prog, DNN_OP_RELU, DNN_DTYPE_FP32, r_h1, r_h1, -1);
    dnn_emit_matmul(&prog, DNN_DTYPE_FP32, r_out, r_h1, r_w2, 1, 10, 256);
    dnn_emit_halt(&prog);

    printf("ISA program: %u instructions
", prog.instr_count);
    dnn_isa_stats_t istats;
    dnn_isa_collect_stats(&prog, &istats);
    printf("  Matrix ops: %u  Memory ops: %u  Total cycles: %u
", istats.matrix_ops, istats.memory_ops, istats.cycles);

    /* 3. Simulate systolic array for layer 1 matmul */
    systolic_array_t sa;
    sa_config_t cfg = sa_default_config();
    cfg.rows = 16; cfg.cols = 16;
    sa_init(&sa, &cfg);
    printf(" Systolic array: %ux%u %s dataflow
", cfg.rows, cfg.cols, cfg.dataflow == SA_DATAFLOW_WEIGHT_STATIONARY ? "WS" : "OS");

    /* Dummy matmul for demonstration */
    double A[64] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    double B[64] = {2,0,0,0, 0,3,0,0, 0,0,4,0, 0,0,0,5};
    double C[64] = {0};
    sa_matmul_tiled(&sa, A, 4, 4, B, 4, 4, C, 4, 4, 4);
    printf("Test matmul: C[0]=%.1f C[5]=%.1f C[15]=%.1f
", C[0], C[5], C[15]);

    sa_stats_t sas;
    sa_collect_stats(&sa, &sas);
    printf("  MACs: %llu  Util: %.1f%%  Throughput: %.3f TOPS
", (unsigned long long)sas.mac_operations, sas.utilization*100, sas.throughput_tops);
    sa_free(&sa);

    /* 4. PE microarchitecture demo */
    pe_state_t pe;
    pe_config_t pcfg = pe_default_config();
    pe_init(&pe, 0, 0, 0, &pcfg);
    pe_load_weight(&pe, 0.5);
    pe_load_input(&pe, 0.8);
    int i;
    for (i = 0; i < 10; i++) pe_cycle(&pe);
    printf(" PE result: MAC(0.5*0.8) = %.4f (ReLU)
", pe_read_output(&pe));
    printf("PE MAC count: %llu  Stall cycles: %llu
", (unsigned long long)pe.mac_count, (unsigned long long)pe.stall_cycles);

    /* 5. Buffer hierarchy analysis */
    buffer_hierarchy_t bh;
    buf_hierarchy_init(&bh);
    buf_hierarchy_add_level(&bh, BUF_LEVEL_L1_LOCAL, "L1", 64*1024, 2, 400.0);
    buf_hierarchy_add_level(&bh, BUF_LEVEL_L2_GLOBAL, "L2", 512*1024, 10, 200.0);
    buf_hierarchy_add_level(&bh, BUF_LEVEL_DRAM, "DRAM", 1024*1024, 100, 50.0);
    printf(" Buffer hierarchy: L1=%uKB L2=%uKB DRAM=%uMB
", 64, 512, 1);

    buf_perf_t bperf;
    buf_hierarchy_collect_perf(&bh, &bperf);
    printf("  Estimated energy: %.2f uJ
", bperf.total_energy_uj);

    /* 6. Roofline analysis for MNIST workload */
    roofline_model_t rm;
    roofline_init(&rm, &spec);
    roofline_compute_ceilings(&rm);
    double ops_784x256 = 784.0 * 256 * 2;
    double bytes_784x256 = (784 + 256) * 4;
    roofline_add_point(&rm, "FC_784x256", ops_784x256, bytes_784x256, 1e-6);
    roofline_add_point(&rm, "FC_256x10", 256.0*10*2, (256+10)*4.0, 1e-7);
    roofline_print_summary(&rm);

    printf(" === MNIST Demo Complete ===
");
    return 0;
}
