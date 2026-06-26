/* ================================================================
 * systolic_array.c — 2D Systolic Array for matrix multiplication
 *
 * Implements: systolic dataflow (WS/OS/RS), matrix multiply mapping,
 * tiled execution for large matrices, double buffering, performance
 * statistics.
 *
 * L1: Array initialization, PE state array
 * L2: Systolic rhythm — data pulsed through array edges
 * L3: Weight/Output/Row stationary dataflow implementations
 * L4: Little's Law for buffer sizing: L = λ * W
 * L5: Tiled matrix multiply algorithm (GEMM mapping)
 * L7: Application — CNN layer acceleration
 *
 * Course mapping:
 *   CMU 15-740, MIT 6.004, Berkeley CS 152, Stanford CS 149
 *   UT Austin ECE 382V (VLSI), 清华 计算机体系结构
 * ================================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "systolic_array.h"
#include "pe_microarch.h"

/* ================================================================
 * Default configuration — L1
 * ================================================================ */

sa_config_t sa_default_config(void) {
    sa_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.rows               = 16;
    cfg.cols               = 16;
    cfg.dataflow           = SA_DATAFLOW_WEIGHT_STATIONARY;
    cfg.precision          = SA_PRECISION_FP16;
    cfg.mac_per_cycle      = 1;
    cfg.pipeline_depth     = 4;
    cfg.clock_ghz          = 1.0;
    cfg.use_double_buffering = true;
    cfg.enable_sparse      = false;
    cfg.sram_kb            = 256;
    return cfg;
}

/* ================================================================
 * Initialize / reset — L1
 * ================================================================ */

void sa_init(systolic_array_t *sa, const sa_config_t *cfg) {
    if (!sa) return;
    memset(sa, 0, sizeof(*sa));
    if (cfg) sa->config = *cfg;
    else sa->config = sa_default_config();

    for (uint32_t r = 0; r < sa->config.rows; r++) {
        for (uint32_t c = 0; c < sa->config.cols; c++) {
            sa->pe[r][c].row = r;
            sa->pe[r][c].col = c;
        }
    }
}

void sa_reset(systolic_array_t *sa) {
    if (!sa) return;
    for (uint32_t r = 0; r < sa->config.rows; r++) {
        for (uint32_t c = 0; c < sa->config.cols; c++) {
            memset(&sa->pe[r][c], 0, sizeof(sa_pe_state_t));
            sa->pe[r][c].row = r;
            sa->pe[r][c].col = c;
        }
    }
    sa->cycle = 0;
    sa->running = false;
    sa->done = false;
    sa->total_macs = 0;
    sa->total_cycles = 0;
    sa->stall_cycles = 0;
    sa->active_cycles = 0;
    sa->total_bytes_read = 0;
    sa->total_bytes_written = 0;
}

void sa_free(systolic_array_t *sa) {
    if (!sa) return;
    for (int i = 0; i < SA_MAX_BUFFERS; i++) {
        free(sa->weight_buffer[i]);
        free(sa->input_buffer[i]);
        free(sa->output_buffer[i]);
    }
    memset(sa, 0, sizeof(*sa));
}

/* ================================================================
 * Configuration
 * ================================================================ */

void sa_set_dataflow(systolic_array_t *sa, sa_dataflow_t df) {
    if (sa) sa->config.dataflow = df;
}

void sa_set_precision(systolic_array_t *sa, sa_precision_t prec) {
    if (sa) sa->config.precision = prec;
}

/* ================================================================
 * Weight / Input loading
 * ================================================================ */

void sa_load_weights(systolic_array_t *sa, const double *weights,
                     uint32_t rows, uint32_t cols) {
    if (!sa || !weights) return;
    for (uint32_t r = 0; r < rows && r < sa->config.rows; r++) {
        for (uint32_t c = 0; c < cols && c < sa->config.cols; c++) {
            sa->pe[r][c].weight = weights[r * cols + c];
            sa->pe[r][c].weight_valid = true;
        }
    }
}

void sa_load_inputs(systolic_array_t *sa, const double *inputs,
                    uint32_t rows, uint32_t cols) {
    if (!sa || !inputs) return;
    for (uint32_t r = 0; r < rows && r < sa->config.rows; r++) {
        for (uint32_t c = 0; c < cols && c < sa->config.cols; c++) {
            sa->pe[r][c].input = inputs[r * cols + c];
            sa->pe[r][c].input_valid = true;
        }
    }
    sa->total_bytes_read += rows * cols * sizeof(double);
}

void sa_read_outputs(systolic_array_t *sa, double *outputs,
                     uint32_t rows, uint32_t cols) {
    if (!sa || !outputs) return;
    for (uint32_t r = 0; r < rows && r < sa->config.rows; r++) {
        for (uint32_t c = 0; c < cols && c < sa->config.cols; c++) {
            outputs[r * cols + c] = sa->pe[r][c].accum;
        }
    }
}

/* ================================================================
 * Single-cycle step — L2: systolic data propagation
 *
 * In each cycle:
 *   - Each PE multiplies its weight × input and adds to accum
 *   - Inputs propagate right (east) to next PE
 *   - Weights propagate down (south) to next PE (in WS dataflow)
 *   - Partial sums propagate down (in OS dataflow)
 * ================================================================ */

void sa_step(systolic_array_t *sa) {
    if (!sa || sa->done) return;

    sa->cycle++;
    sa->total_cycles++;
    uint32_t active_pe_count = 0;

    for (uint32_t r = 0; r < sa->config.rows; r++) {
        for (uint32_t c = 0; c < sa->config.cols; c++) {
            sa_pe_state_t *pe = &sa->pe[r][c];
            if (!pe->weight_valid || !pe->input_valid) continue;

            /* MAC operation */
            pe->accum += pe->weight * pe->input;
            pe->ops_completed++;
            sa->total_macs++;
            active_pe_count++;

            /* Dataflow-dependent propagation */
            switch (sa->config.dataflow) {
            case SA_DATAFLOW_WEIGHT_STATIONARY:
                /* Weights stay; input propagates right; partial sum down */
                if (c + 1 < sa->config.cols) {
                    sa->pe[r][c + 1].input = pe->input;
                    sa->pe[r][c + 1].input_valid = true;
                }
                if (r + 1 < sa->config.rows) {
                    sa->pe[r + 1][c].accum += pe->accum;
                    sa->pe[r + 1][c].accum_valid = true;
                }
                break;

            case SA_DATAFLOW_OUTPUT_STATIONARY:
                /* Output stays; weights propagate right; inputs propagate down */
                pe->output = pe->accum;
                if (c + 1 < sa->config.cols) {
                    sa->pe[r][c + 1].weight = pe->weight;
                    sa->pe[r][c + 1].weight_valid = true;
                }
                if (r + 1 < sa->config.rows) {
                    sa->pe[r + 1][c].input = pe->input;
                    sa->pe[r + 1][c].input_valid = true;
                }
                break;

            case SA_DATAFLOW_ROW_STATIONARY:
                /* Row of filter stays; input propagates diagonally */
                if (c + 1 < sa->config.cols) {
                    sa->pe[r][c + 1].input = pe->input;
                    sa->pe[r][c + 1].input_valid = true;
                }
                if (r + 1 < sa->config.rows) {
                    sa->pe[r + 1][c].weight = pe->weight;
                    sa->pe[r + 1][c].weight_valid = true;
                }
                break;

            case SA_DATAFLOW_INPUT_STATIONARY:
                /* Input stays; weights propagate down; partial sum right */
                if (r + 1 < sa->config.rows) {
                    sa->pe[r + 1][c].weight = pe->weight;
                    sa->pe[r + 1][c].weight_valid = true;
                }
                if (c + 1 < sa->config.cols) {
                    sa->pe[r][c + 1].accum += pe->accum;
                }
                break;

            case SA_DATAFLOW_NO_LOCAL_REUSE:
            default:
                /* No data reuse — all data comes from scratchpad */
                break;
            }
        }
    }

    if (active_pe_count > 0) {
        sa->active_cycles++;
    } else {
        sa->stall_cycles++;
    }
}

void sa_run_cycles(systolic_array_t *sa, uint32_t cycles) {
    if (!sa) return;
    sa->running = true;
    for (uint32_t i = 0; i < cycles; i++) {
        sa_step(sa);
    }
    sa->running = false;
}

void sa_run_to_completion(systolic_array_t *sa) {
    if (!sa) return;
    /* Run until all PEs are idle (no valid data) */
    uint32_t idle_count = 0;
    sa->running = true;
    while (idle_count < 3) { /* 3 consecutive idle cycles = done */
        bool any_active = false;
        for (uint32_t r = 0; r < sa->config.rows && !any_active; r++) {
            for (uint32_t c = 0; c < sa->config.cols && !any_active; c++) {
                if (sa->pe[r][c].weight_valid && sa->pe[r][c].input_valid) {
                    any_active = true;
                }
            }
        }
        if (any_active) {
            idle_count = 0;
            sa_step(sa);
        } else {
            idle_count++;
            sa->cycle++;
        }
        if (sa->cycle > 1000000) break; /* safety limit */
    }
    sa->running = false;
    sa->done = true;
}

/* ================================================================
 * Matrix Multiply — L5: GEMM on systolic array
 *
 * Computes C = A × B, where A is M×K, B is K×N, C is M×N.
 * Algorithm: tiled matrix multiplication with double buffering.
 *
 * Complexity: O(M * N * K) MAC operations
 * ================================================================ */

void sa_matmul(systolic_array_t *sa,
               const double *A, uint32_t A_rows, uint32_t A_cols,
               const double *B, uint32_t B_rows, uint32_t B_cols,
               double *C) {
    if (!sa || !A || !B || !C) return;
    if (A_cols != B_rows) return; /* dimension mismatch */

    uint32_t M = A_rows;
    uint32_t N = B_cols;
    uint32_t K = A_cols;

    /* Initialize C to zero */
    for (uint32_t i = 0; i < M; i++) {
        for (uint32_t j = 0; j < N; j++) {
            C[i * N + j] = 0.0;
        }
    }

    /* Tiling based on systolic array dimensions */
    uint32_t tile_M = sa->config.rows;
    uint32_t tile_N = sa->config.cols;
    uint32_t tile_K = sa->config.pipeline_depth;

    for (uint32_t m = 0; m < M; m += tile_M) {
        uint32_t m_end = (m + tile_M < M) ? m + tile_M : M;
        for (uint32_t n = 0; n < N; n += tile_N) {
            uint32_t n_end = (n + tile_N < N) ? n + tile_N : N;
            for (uint32_t k = 0; k < K; k += tile_K) {
                uint32_t k_end = (k + tile_K < K) ? k + tile_K : K;

                /* Load weight tile from B */
                for (uint32_t kk = k; kk < k_end; kk++) {
                    for (uint32_t nn = n; nn < n_end; nn++) {
                        uint32_t pe_r = kk - k;
                        uint32_t pe_c = nn - n;
                        if (pe_r < sa->config.rows && pe_c < sa->config.cols) {
                            sa->pe[pe_r][pe_c].weight = B[kk * N + nn];
                            sa->pe[pe_r][pe_c].weight_valid = true;
                        }
                    }
                }

                /* Load input tile from A */
                for (uint32_t mm = m; mm < m_end; mm++) {
                    for (uint32_t kk = k; kk < k_end; kk++) {
                        uint32_t pe_r = mm - m;
                        uint32_t pe_c = kk - k;
                        if (pe_r < sa->config.rows && pe_c < sa->config.cols) {
                            sa->pe[pe_r][pe_c].input = A[mm * K + kk];
                            sa->pe[pe_r][pe_c].input_valid = true;
                        }
                    }
                }

                /* Run systolic execution */
                sa_run_cycles(sa, tile_K + sa->config.rows + sa->config.cols);

                /* Read out partial sums */
                for (uint32_t mm = m; mm < m_end; mm++) {
                    for (uint32_t nn = n; nn < n_end; nn++) {
                        uint32_t pe_r = mm - m;
                        uint32_t pe_c = nn - n;
                        if (pe_r < sa->config.rows && pe_c < sa->config.cols) {
                            C[mm * N + nn] += sa->pe[pe_r][pe_c].accum;
                            sa->pe[pe_r][pe_c].accum = 0.0;
                            sa->pe[pe_r][pe_c].accum_valid = false;
                        }
                    }
                }

                /* Clear weight/input valid flags for next tile */
                for (uint32_t r = 0; r < sa->config.rows; r++) {
                    for (uint32_t c = 0; c < sa->config.cols; c++) {
                        sa->pe[r][c].weight_valid = false;
                        sa->pe[r][c].input_valid = false;
                    }
                }
            }
        }
    }
}

/* ================================================================
 * Tiled Matrix Multiply — L5: for matrices larger than array
 * ================================================================ */

void sa_matmul_tiled(systolic_array_t *sa,
                     const double *A, uint32_t M, uint32_t K,
                     const double *B, uint32_t K_dim, uint32_t N,
                     double *C, uint32_t tile_M, uint32_t tile_K, uint32_t tile_N) {
    if (!sa || !A || !B || !C) return;
    if (K != K_dim) return;

    if (tile_M == 0) tile_M = sa->config.rows;
    if (tile_K == 0) tile_K = sa->config.rows;
    if (tile_N == 0) tile_N = sa->config.cols;

    memset(C, 0, M * N * sizeof(double));

    for (uint32_t m = 0; m < M; m += tile_M) {
        uint32_t m_max = (m + tile_M < M) ? m + tile_M : M;
        for (uint32_t n = 0; n < N; n += tile_N) {
            uint32_t n_max = (n + tile_N < N) ? n + tile_N : N;
            for (uint32_t k = 0; k < K; k += tile_K) {
                uint32_t k_max = (k + tile_K < K) ? k + tile_K : K;

                /* Load weight tile */
                for (uint32_t kk = k; kk < k_max; kk++) {
                    for (uint32_t nn = n; nn < n_max; nn++) {
                        uint32_t r = kk - k, c = nn - n;
                        if (r < sa->config.rows && c < sa->config.cols) {
                            sa->pe[r][c].weight = B[kk * N + nn];
                            sa->pe[r][c].weight_valid = true;
                        }
                    }
                }

                /* Load input tile */
                for (uint32_t mm = m; mm < m_max; mm++) {
                    for (uint32_t kk = k; kk < k_max; kk++) {
                        uint32_t r = mm - m, c = kk - k;
                        if (r < sa->config.rows && c < sa->config.cols) {
                            sa->pe[r][c].input = A[mm * K + kk];
                            sa->pe[r][c].input_valid = true;
                        }
                    }
                }

                /* Execute and accumulate */
                uint32_t run_cycles = (k_max - k) + sa->config.rows + sa->config.cols;
                sa_run_cycles(sa, run_cycles);

                for (uint32_t mm = m; mm < m_max; mm++) {
                    for (uint32_t nn = n; nn < n_max; nn++) {
                        uint32_t r = mm - m, c = nn - n;
                        if (r < sa->config.rows && c < sa->config.cols) {
                            C[mm * N + nn] += sa->pe[r][c].accum;
                            sa->pe[r][c].accum = 0.0;
                        }
                    }
                }

                /* Reset for next tile iteration */
                for (uint32_t rr = 0; rr < sa->config.rows; rr++) {
                    for (uint32_t cc = 0; cc < sa->config.cols; cc++) {
                        sa->pe[rr][cc].weight_valid = false;
                        sa->pe[rr][cc].input_valid = false;
                    }
                }
            }
        }
    }
}

/* ================================================================
 * Dataflow-specific mapping — L3: three canonical dataflows
 * ================================================================ */

/* Weight Stationary: weights are pre-loaded, inputs and partial sums flow */
void sa_map_weight_stationary(systolic_array_t *sa,
                              const double *weights, uint32_t w_rows, uint32_t w_cols,
                              const double *inputs,  uint32_t i_rows, uint32_t i_cols,
                              double *outputs) {
    if (!sa || !weights || !inputs || !outputs) return;
    sa->config.dataflow = SA_DATAFLOW_WEIGHT_STATIONARY;
    sa_load_weights(sa, weights, w_rows, w_cols);
    sa_load_inputs(sa, inputs, i_rows, i_cols);
    sa_run_to_completion(sa);
    sa_read_outputs(sa, outputs, w_rows, i_cols);
}

/* Output Stationary: partial sums stay in place, weights and inputs flow */
void sa_map_output_stationary(systolic_array_t *sa,
                              const double *weights, uint32_t w_rows, uint32_t w_cols,
                              const double *inputs,  uint32_t i_rows, uint32_t i_cols,
                              double *outputs) {
    if (!sa || !weights || !inputs || !outputs) return;
    sa->config.dataflow = SA_DATAFLOW_OUTPUT_STATIONARY;
    sa_load_weights(sa, weights, w_rows, w_cols);
    sa_load_inputs(sa, inputs, i_rows, i_cols);
    sa_run_to_completion(sa);
    sa_read_outputs(sa, outputs, w_rows, i_cols);
}

/* Row Stationary: each PE holds one row of the filter (Eyeriss-style) */
void sa_map_row_stationary(systolic_array_t *sa,
                           const double *weights, uint32_t w_rows, uint32_t w_cols,
                           const double *inputs,  uint32_t i_rows, uint32_t i_cols,
                           double *outputs) {
    if (!sa || !weights || !inputs || !outputs) return;
    sa->config.dataflow = SA_DATAFLOW_ROW_STATIONARY;
    sa_load_weights(sa, weights, w_rows, w_cols);
    sa_load_inputs(sa, inputs, i_rows, i_cols);
    sa_run_to_completion(sa);
    sa_read_outputs(sa, outputs, w_rows, i_cols);
}

/* ================================================================
 * Statistics — L4: performance analysis
 * ================================================================ */

void sa_collect_stats(const systolic_array_t *sa, sa_stats_t *stats) {
    if (!sa || !stats) return;
    memset(stats, 0, sizeof(*stats));
    stats->mac_operations = sa->total_macs;
    stats->cycles         = sa->total_cycles;
    stats->stall_cycles   = sa->stall_cycles;

    /* Throughput in TOPS (total MAC ops / total time) */
    double time_sec = (double)sa->total_cycles / (sa->config.clock_ghz * 1e9);
    if (time_sec > 0) {
        stats->throughput_tops = (double)sa->total_macs * 2.0 / time_sec / 1e12;
    }

    /* Utilization = active cycles / total cycles */
    if (sa->total_cycles > 0) {
        stats->utilization = (double)sa->active_cycles / (double)sa->total_cycles;
    }

    /* Energy (assuming ~1 pJ per MAC at 7nm) */
    stats->energy_efficiency_pj_per_mac = 0.8;
    stats->total_energy_uj = (double)sa->total_macs * 0.8 / 1e6;

    /* Reuse factors from dataflow */
    switch (sa->config.dataflow) {
    case SA_DATAFLOW_WEIGHT_STATIONARY:
        stats->weight_reuse_factor = (double)sa->config.cols;
        stats->input_reuse_factor  = 1.0;
        stats->output_reuse_factor = (double)sa->config.rows;
        break;
    case SA_DATAFLOW_OUTPUT_STATIONARY:
        stats->weight_reuse_factor = (double)sa->config.cols;
        stats->input_reuse_factor  = (double)sa->config.rows;
        stats->output_reuse_factor = 1.0;
        break;
    case SA_DATAFLOW_ROW_STATIONARY:
        stats->weight_reuse_factor = 1.0;
        stats->input_reuse_factor  = (double)sa->config.cols;
        stats->output_reuse_factor = (double)sa->config.rows;
        break;
    case SA_DATAFLOW_INPUT_STATIONARY:
        stats->weight_reuse_factor = (double)sa->config.rows;
        stats->input_reuse_factor  = 1.0;
        stats->output_reuse_factor = (double)sa->config.cols;
        break;
    default:
        stats->weight_reuse_factor = 1.0;
        stats->input_reuse_factor  = 1.0;
        stats->output_reuse_factor = 1.0;
        break;
    }
}

void sa_print_stats(const sa_stats_t *stats) {
    if (!stats) return;
    printf("=== Systolic Array Statistics ===\n");
    printf("  MAC operations:   %llu\n", (unsigned long long)stats->mac_operations);
    printf("  Total cycles:     %llu\n", (unsigned long long)stats->cycles);
    printf("  Stall cycles:     %llu\n", (unsigned long long)stats->stall_cycles);
    printf("  Throughput:       %.3f TOPS\n", stats->throughput_tops);
    printf("  Utilization:      %.1f%%\n", stats->utilization * 100.0);
    printf("  Energy:           %.2f uJ\n", stats->total_energy_uj);
    printf("  Energy/MAC:       %.2f pJ\n", stats->energy_efficiency_pj_per_mac);
    printf("  Weight reuse:     %.1fx\n", stats->weight_reuse_factor);
    printf("  Input reuse:      %.1fx\n", stats->input_reuse_factor);
    printf("  Output reuse:     %.1fx\n", stats->output_reuse_factor);
}

void sa_print_config(const systolic_array_t *sa) {
    if (!sa) return;
    static const char *df_names[] = {"WS","OS","RS","IS","NLR"};
    static const char *prec_names[] = {"FP32","FP16","BF16","INT8","INT4","MIXED"};
    printf("=== Systolic Array Config ===\n");
    printf("  Size:       %u×%u\n", sa->config.rows, sa->config.cols);
    printf("  Dataflow:   %s\n", df_names[sa->config.dataflow % 5]);
    printf("  Precision:  %s\n", prec_names[sa->config.precision % 6]);
    printf("  Clock:      %.1f GHz\n", sa->config.clock_ghz);
    printf("  Pipeline:   %u stages\n", sa->config.pipeline_depth);
    printf("  SRAM:       %u KB\n", sa->config.sram_kb);
    printf("  Double buf: %s\n", sa->config.use_double_buffering ? "yes" : "no");
    printf("  Sparse:     %s\n", sa->config.enable_sparse ? "yes" : "no");
}

/* ================================================================
 * Tile management
 * ================================================================ */

sa_matrix_tile_t sa_tile_allocate(uint32_t rows, uint32_t cols) {
    sa_matrix_tile_t tile;
    memset(&tile, 0, sizeof(tile));
    tile.rows = rows;
    tile.cols = cols;
    tile.ld   = cols;
    tile.data = (double *)calloc(rows * cols, sizeof(double));
    tile.owns_data = true;
    return tile;
}

void sa_tile_free(sa_matrix_tile_t *tile) {
    if (!tile) return;
    if (tile->owns_data) {
        free(tile->data);
    }
    memset(tile, 0, sizeof(*tile));
}

void sa_tile_copy_from(sa_matrix_tile_t *tile, const double *src, uint32_t ld) {
    if (!tile || !src) return;
    for (uint32_t r = 0; r < tile->rows; r++) {
        for (uint32_t c = 0; c < tile->cols; c++) {
            tile->data[r * tile->ld + c] = src[r * ld + c];
        }
    }
}

void sa_tile_copy_to(const sa_matrix_tile_t *tile, double *dst, uint32_t ld) {
    if (!tile || !dst) return;
    for (uint32_t r = 0; r < tile->rows; r++) {
        for (uint32_t c = 0; c < tile->cols; c++) {
            dst[r * ld + c] = tile->data[r * tile->ld + c];
        }
    }
}