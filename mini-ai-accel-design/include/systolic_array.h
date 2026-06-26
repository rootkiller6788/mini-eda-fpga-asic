#ifndef SYSTOLIC_ARRAY_H
#define SYSTOLIC_ARRAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ================================================================
 * Systolic Array — 2D MAC array for DNN acceleration
 * L1: Core array structures and configuration
 * L2: Systolic execution concept — data rhythm, wavefront parallelism
 * L3: Engineering structure — weight/output/row stationary dataflows
 * L5: Matrix multiply mapping onto 2D grid
 * ================================================================ */

#define SA_MAX_ROWS       64
#define SA_MAX_COLS       64
#define SA_MAX_PIPELINE   8
#define SA_MAX_BUFFERS    16
#define SA_MAX_IFMAP_CH   2048

/* --- Systolic Array Configuration --- */
typedef enum {
    SA_DATAFLOW_WEIGHT_STATIONARY,
    SA_DATAFLOW_OUTPUT_STATIONARY,
    SA_DATAFLOW_ROW_STATIONARY,
    SA_DATAFLOW_INPUT_STATIONARY,
    SA_DATAFLOW_NO_LOCAL_REUSE,
    SA_DATAFLOW_COUNT
} sa_dataflow_t;

typedef enum {
    SA_PRECISION_FP32,
    SA_PRECISION_FP16,
    SA_PRECISION_BF16,
    SA_PRECISION_INT8,
    SA_PRECISION_INT4,
    SA_PRECISION_MIXED,
    SA_PRECISION_COUNT
} sa_precision_t;

typedef struct {
    uint32_t        rows;
    uint32_t        cols;
    sa_dataflow_t   dataflow;
    sa_precision_t  precision;
    uint32_t        mac_per_cycle;
    uint32_t        pipeline_depth;
    double          clock_ghz;
    bool            use_double_buffering;
    bool            enable_sparse;
    uint32_t        sram_kb;
} sa_config_t;

/* --- Processing Element State --- */
typedef struct {
    uint32_t    row;
    uint32_t    col;
    double      weight;
    double      input;
    double      accum;
    double      output;
    bool        weight_valid;
    bool        input_valid;
    bool        accum_valid;
    uint32_t    stall_cycles;
    uint64_t    ops_completed;
} sa_pe_state_t;

/* --- Systolic Array Instance --- */
typedef struct {
    sa_config_t     config;
    sa_pe_state_t   pe[SA_MAX_ROWS][SA_MAX_COLS];
    uint32_t        cycle;
    bool            running;
    bool            done;
    /* Double buffering */
    double         *weight_buffer[SA_MAX_BUFFERS];
    double         *input_buffer[SA_MAX_BUFFERS];
    double         *output_buffer[SA_MAX_BUFFERS];
    uint32_t        active_wbuf;
    uint32_t        active_ibuf;
    uint32_t        active_obuf;
    /* Performance counters */
    uint64_t        total_macs;
    uint64_t        total_bytes_read;
    uint64_t        total_bytes_written;
    uint64_t        total_cycles;
    uint64_t        stall_cycles;
    uint64_t        active_cycles;
} systolic_array_t;

/* --- Matrix Tile --- */
typedef struct {
    uint32_t    rows;
    uint32_t    cols;
    double     *data;
    uint32_t    ld;
    bool        owns_data;
} sa_matrix_tile_t;

/* --- Execution Statistics --- */
typedef struct {
    double      throughput_tops;
    double      utilization;
    double      energy_efficiency_pj_per_mac;
    double      total_energy_uj;
    uint64_t    mac_operations;
    uint64_t    cycles;
    uint64_t    stall_cycles;
    double      weight_reuse_factor;
    double      input_reuse_factor;
    double      output_reuse_factor;
} sa_stats_t;

/* API */
void            sa_init(systolic_array_t *sa, const sa_config_t *cfg);
void            sa_reset(systolic_array_t *sa);
void            sa_free(systolic_array_t *sa);

/* Configuration */
void            sa_set_dataflow(systolic_array_t *sa, sa_dataflow_t df);
void            sa_set_precision(systolic_array_t *sa, sa_precision_t prec);
sa_config_t     sa_default_config(void);

/* Weight loading and input feeding */
void            sa_load_weights(systolic_array_t *sa, const double *weights,
                                uint32_t rows, uint32_t cols);
void            sa_load_inputs(systolic_array_t *sa, const double *inputs,
                               uint32_t rows, uint32_t cols);
void            sa_read_outputs(systolic_array_t *sa, double *outputs,
                                uint32_t rows, uint32_t cols);

/* Execution */
void            sa_step(systolic_array_t *sa);
void            sa_run_cycles(systolic_array_t *sa, uint32_t cycles);
void            sa_run_to_completion(systolic_array_t *sa);

/* Matrix multiply on systolic array — full implementation */
void            sa_matmul(systolic_array_t *sa,
                          const double *A, uint32_t A_rows, uint32_t A_cols,
                          const double *B, uint32_t B_rows, uint32_t B_cols,
                          double *C);

/* Tiled matrix multiply for large matrices */
void            sa_matmul_tiled(systolic_array_t *sa,
                                const double *A, uint32_t M, uint32_t K,
                                const double *B, uint32_t K_dim, uint32_t N,
                                double *C, uint32_t tile_M, uint32_t tile_K, uint32_t tile_N);

/* Statistics */
void            sa_collect_stats(const systolic_array_t *sa, sa_stats_t *stats);
void            sa_print_stats(const sa_stats_t *stats);
void            sa_print_config(const systolic_array_t *sa);

/* Dataflow-specific mapping */
void            sa_map_weight_stationary(systolic_array_t *sa,
                                         const double *weights, uint32_t w_rows, uint32_t w_cols,
                                         const double *inputs,  uint32_t i_rows, uint32_t i_cols,
                                         double *outputs);
void            sa_map_output_stationary(systolic_array_t *sa,
                                         const double *weights, uint32_t w_rows, uint32_t w_cols,
                                         const double *inputs,  uint32_t i_rows, uint32_t i_cols,
                                         double *outputs);
void            sa_map_row_stationary(systolic_array_t *sa,
                                      const double *weights, uint32_t w_rows, uint32_t w_cols,
                                      const double *inputs,  uint32_t i_rows, uint32_t i_cols,
                                      double *outputs);

/* Tile management */
sa_matrix_tile_t sa_tile_allocate(uint32_t rows, uint32_t cols);
void             sa_tile_free(sa_matrix_tile_t *tile);
void             sa_tile_copy_from(sa_matrix_tile_t *tile, const double *src, uint32_t ld);
void             sa_tile_copy_to(const sa_matrix_tile_t *tile, double *dst, uint32_t ld);

#endif /* SYSTOLIC_ARRAY_H */