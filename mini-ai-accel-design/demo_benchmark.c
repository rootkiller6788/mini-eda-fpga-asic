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
    const char *name;
    uint32_t m;
    uint32_t n;
    uint32_t k;
    uint32_t tile_m;
    uint32_t tile_n;
    uint32_t tile_k;
    uint32_t array_rows;
    uint32_t array_cols;
    bool     zero_skip;
    double   expected_tops;
} BenchCase;

static const BenchCase bench_suite[] = {
    {"GEMM-128",            128,  128,  128,   32, 32, 32, 32, 32, false, 0.5},
    {"GEMM-256",            256,  256,  256,   32, 32, 32, 64, 64, false, 1.0},
    {"GEMM-512",            512,  512,  512,   64, 64, 64, 64, 64, false, 2.0},
    {"GEMM-1024",          1024, 1024, 1024,   64, 64, 64, 64, 64, true,  4.0},
    {"GEMM-2048",          2048, 2048, 2048,  128,128,128,128,128, true,  8.0},
    {"LargeM-4096x128",    4096,  128,  128,  128, 32, 32,128, 32, true,  2.0},
    {"LargeN-128x4096",     128, 4096,  128,   32,128, 32, 32,128, true,  2.0},
    {"LargeK-128x128x4096", 128,  128, 4096,   32, 32,128, 32, 32, true,  1.5},
    {"BERT-Attention",      128,  768,  768,   64, 64, 64, 64, 64, true,  3.0},
    {"BERT-FFN",            128, 3072,  768,   64,128, 64, 64,128, true,  4.0},
    {"ResNet-Conv",          64, 3136,  147,   64, 64, 64, 64, 64, true,  2.5},
    {"GPT2-Attention",     1024,  768,  768,   64, 64, 64,128, 64, true,  5.0},
    {"DLRM-Bottom",         128, 1024,  128,   64, 64, 64, 64, 64, true,  1.0},
    {"DeepGEMM-4096",      4096, 4096, 4096,  128,128,128,128,128, true, 12.0},
    {"TinyGEMM-16",          16,   16,   16,   16, 16, 16, 16, 16, false, 0.01},
};

#define N_BENCHES (sizeof(bench_suite) / sizeof(bench_suite[0]))

static void run_matmul_gemm(SystolicArray *sa, const BenchCase *bc)
{
    sa_init(sa, bc->array_rows, bc->array_cols,
            SA_DATAFLOW_WEIGHT_STATIONARY, SA_PREC_INT8);

    if (bc->tile_m > sa->num_rows) sa->num_rows = bc->tile_m;
    if (bc->tile_n > sa->num_cols) sa->num_cols = bc->tile_n;

    uint32_t mt = (bc->m + bc->tile_m - 1) / bc->tile_m;
    uint32_t nt = (bc->n + bc->tile_n - 1) / bc->tile_n;
    uint32_t kt = (bc->k + bc->tile_k - 1) / bc->tile_k;

    for (uint32_t mi = 0; mi < mt; mi++) {
        uint32_t m_start = mi * bc->tile_m;
        uint32_t m_cur   = (m_start + bc->tile_m) < bc->m ? bc->tile_m : (bc->m - m_start);

        for (uint32_t ni = 0; ni < nt; ni++) {
            uint32_t n_cur = (ni * bc->tile_n + bc->tile_n) < bc->n ? bc->tile_n : (bc->n - ni * bc->tile_n);

            for (uint32_t ki = 0; ki < kt; ki++) {
                uint32_t k_cur = (ki * bc->tile_k + bc->tile_k) < bc->k ? bc->tile_k : (bc->k - ki * bc->tile_k);

                sa_reset(sa);

                for (uint32_t r = 0; r < m_cur && r < sa->num_rows; r++) {
                    for (uint32_t c = 0; c < k_cur && c < sa->num_cols; c++) {
                        sa->grid[r][c].weight       = (int8_t)((r * 7 + c * 13) % 127);
                        sa->grid[r][c].weight_valid = true;
                    }
                }

                if (bc->zero_skip) {
                    for (uint32_t r = 0; r < m_cur && r < sa->num_rows; r++) {
                        for (uint32_t c = 0; c < k_cur && c < sa->num_cols; c++) {
                            sa->grid[r][c].act_in    = ((r + c) % 5 == 0) ? 0 : (int8_t)((r + c * 3) % 127);
                            sa->grid[r][c].act_valid = true;
                        }
                    }
                } else {
                    for (uint32_t r = 0; r < m_cur && r < sa->num_rows; r++) {
                        for (uint32_t c = 0; c < k_cur && c < sa->num_cols; c++) {
                            sa->grid[r][c].act_in    = (int8_t)((r * 11 + c * 7) % 127);
                            sa->grid[r][c].act_valid = true;
                        }
                    }
                }

                uint32_t pipeline_cycles = sa->num_rows + sa->num_cols + k_cur;
                sa_compute_n_cycles(sa, pipeline_cycles);
                (void)n_cur;
            }
        }
    }
}

static double run_benchmark(SystolicArray *sa, const BenchCase *bc)
{
    struct timespec t0, t1;
    timespec_get(&t0, TIME_UTC);

    run_matmul_gemm(sa, bc);

    timespec_get(&t1, TIME_UTC);
    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;

    uint64_t total_ops = (uint64_t)2 * bc->m * bc->n * bc->k;
    double tops = (elapsed > 0.0) ? (double)total_ops * 1e-12 / elapsed : 0.0;

    return tops;
}

static void sweep_array_sizes(void)
{
    printf("\n--- Array Size Sweep (GEMM-512, 1 GHz) ---\n");
    printf("%-10s %-12s %-12s %-12s\n", "Rows", "Cols", "Peak TOPS", "Eff TOPS");

    uint32_t sizes[] = {16, 32, 64, 128, 256};
    for (int i = 0; i < 5; i++) {
        uint32_t r = sizes[i];
        uint32_t c = sizes[i];

        BenchCase bc = {"sweep", 512, 512, 512, r < 64 ? r : 64, c < 64 ? c : 64, 64, r, c, true, 0};
        SystolicArray sa;
        double tops = run_benchmark(&sa, &bc);
        double peak = (double)r * (double)c * 2.0;

        printf("%-10u %-12u %-12.2f %-12.4f\n", r, c, peak, tops);
    }
}

static void sweep_pipeline_depth(void)
{
    printf("\n--- Pipeline Depth Sweep (64x64, GEMM-512) ---\n");
    printf("%-12s %-12s %-12s %-12s\n", "Depth", "Latency", "MACs", "Util%");

    uint32_t depths[] = {0, 1, 2, 3, 4, 6, 8};

    for (int i = 0; i < 7; i++) {
        SystolicArray sa;
        sa_init(&sa, 64, 64, SA_DATAFLOW_WEIGHT_STATIONARY, SA_PREC_INT8);

        uint32_t d = depths[i];
        if (d > 0) sa_set_pipeline_depth(&sa, d);

        BenchCase bc = {"pipe_sweep", 512, 512, 512, 64, 64, 64, 64, 64, false, 0};
        run_matmul_gemm(&sa, &bc);

        uint64_t latency = sa_estimate_latency_cycles(&sa, 512, 512, 512);
        double util = sa_utilization(&sa);

        printf("%-12u %-12llu %-12llu %-12.1f\n",
               d, (unsigned long long)latency,
               (unsigned long long)sa_total_macs(&sa), util);
    }
}

static void sweep_frequency(void)
{
    printf("\n--- Frequency Sweep (64x64) ---\n");
    printf("%-12s %-12s %-12s %-12s\n", "Freq MHz", "Peak TOPS", "Dyn Pwr mW", "TOPS/W");

    uint32_t freqs[] = {100, 250, 500, 750, 1000, 1500, 2000};
    for (int i = 0; i < 7; i++) {
        AccelCfg cfg = {
            .array_rows = 64, .array_cols = 64, .data_width = 8,
            .buf_l1_depth = 512, .buf_l2_depth = 128 * 1024,
            .buf_l3_depth = 4 * 1024 * 1024, .freq_mhz = freqs[i],
            .pipe_en = true, .zero_skip_en = true,
            .dma_overlap_en = true, .multicast_en = true,
            .ctrl_bus = BUS_AXI4_LITE, .data_bus = BUS_AXI4_FULL,
        };

        TOPS peak = at_peak_tops(&cfg);
        Power pwr = at_est_power(&cfg);
        double tops_per_w = (pwr.p_tot_mw > 0) ? peak.tops / (pwr.p_tot_mw * 1e-3) : 0;

        printf("%-12u %-12.2f %-12.2f %-12.2f\n",
               freqs[i], peak.tops, pwr.p_dyn_mw, tops_per_w);
    }
}

static void compare_dataflows(void)
{
    printf("\n--- Dataflow Comparison (32x32, GEMM-256) ---\n");
    printf("%-24s %-12s %-12s\n", "Dataflow", "Cycles", "Util%");

    SaDataflow dfs[] = {SA_DATAFLOW_WEIGHT_STATIONARY,
                        SA_DATAFLOW_OUTPUT_STATIONARY,
                        SA_DATAFLOW_ROW_STATIONARY};
    const char *df_names[] = {"Weight-Stationary", "Output-Stationary", "Row-Stationary"};

    for (int i = 0; i < 3; i++) {
        SystolicArray sa;
        sa_init(&sa, 32, 32, dfs[i], SA_PREC_INT8);

        BenchCase bc = {"df_comp", 256, 256, 256, 32, 32, 32, 32, 32, false, 0};
        run_matmul_gemm(&sa, &bc);

        printf("%-24s %-12llu %-12.1f\n",
               df_names[i], (unsigned long long)sa.cycle_counter,
               sa_utilization(&sa));
    }
}

static void print_power_analysis(const AccelCfg *cfg)
{
    printf("\n--- Power Analysis ---\n");

    double voltages[] = {0.60, 0.65, 0.70, 0.75, 0.80, 0.85, 0.90};
    printf("%-10s %-14s %-14s %-14s %-14s\n", "VDD(V)", "Dyn(mW)", "Sta(mW)", "Tot(mW)", "pJ/op");

    for (int i = 0; i < 7; i++) {
        double alpha = 0.15;
        double cap   = (double)(cfg->array_rows * cfg->array_cols) * 0.002 + 50.0;
        double freq  = (double)cfg->freq_mhz / 1000.0;
        Power pwr = at_est_power_detailed(alpha, cap, voltages[i], freq);

        double ops_per_s = (double)cfg->array_rows * (double)cfg->array_cols
                           * 2.0 * freq * 1e9;
        double pj_per_op = (ops_per_s > 0) ? pwr.p_tot_mw * 1e-3 / ops_per_s * 1e12 : 0;

        printf("%-10.2f %-14.2f %-14.2f %-14.2f %-14.4f\n",
               voltages[i], pwr.p_dyn_mw, pwr.p_sta_mw, pwr.p_tot_mw, pj_per_op);
    }
}

int main(void)
{
    printf("====== AI Accelerator Benchmark Suite ======\n\n");

    printf("Running %u benchmark configurations...\n\n", (unsigned int)N_BENCHES);

    double total_tops = 0;
    struct timespec t_global_start, t_global_end;
    timespec_get(&t_global_start, TIME_UTC);

    printf("%-22s %8s x%8s x%8s  %6s x%6s  %7s  %8s  %8s\n",
           "Name", "M", "N", "K", "Arr", "TOPS", "Util%", "Status");
    printf("---------------------- -------- -------- --------  "
           "------ ------  -------  --------  --------\n");

    for (int i = 0; i < (int)N_BENCHES; i++) {
        const BenchCase *bc = &bench_suite[i];

        SystolicArray sa;
        double tops = run_benchmark(&sa, bc);
        double util = sa_utilization(&sa);

        bool pass = (tops >= bc->expected_tops * 0.5);
        total_tops += tops;

        printf("%-22s %8u x%8u x%8u  %3ux%-3u  %7.4f  %7.1f%%  %s\n",
               bc->name, bc->m, bc->n, bc->k,
               bc->array_rows, bc->array_cols,
               tops, util,
               pass ? "  PASS" : "  FAIL");
    }

    timespec_get(&t_global_end, TIME_UTC);
    double global_elapsed = (t_global_end.tv_sec - t_global_start.tv_sec)
                            + (t_global_end.tv_nsec - t_global_start.tv_nsec) * 1e-9;

    printf("\n===== Summary =====\n");
    printf("  Benchmarks: %u\n", (unsigned int)N_BENCHES);
    printf("  Total time: %.3f s\n", global_elapsed);
    printf("  Avg TOPS: %.4f\n", total_tops / N_BENCHES);

    AccelCfg ref_cfg = {
        .array_rows = 64, .array_cols = 64, .data_width = 8,
        .buf_l1_depth = 512, .buf_l2_depth = 128 * 1024,
        .buf_l3_depth = 4 * 1024 * 1024, .freq_mhz = 1000,
        .pipe_en = true, .zero_skip_en = true,
        .dma_overlap_en = true, .multicast_en = true,
        .ctrl_bus = BUS_AXI4_LITE, .data_bus = BUS_AXI4_FULL,
    };

    AccelTop accel;
    at_init(&accel, &ref_cfg);
    accel.perf.tops = total_tops / N_BENCHES;

    sweep_array_sizes();
    sweep_pipeline_depth();
    sweep_frequency();
    compare_dataflows();
    print_power_analysis(&ref_cfg);

    printf("\n");
    at_print_summary(&accel);

    printf("\n====== Benchmark Suite Complete ======\n");
    return 0;
}
