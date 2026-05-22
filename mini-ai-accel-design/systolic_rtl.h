#ifndef SYSTOLIC_RTL_H
#define SYSTOLIC_RTL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SA_MAX_ROWS          256
#define SA_MAX_COLS          256
#define SA_MAX_PIPELINE_DEPTH 8

typedef enum {
    SA_DATAFLOW_WEIGHT_STATIONARY,
    SA_DATAFLOW_OUTPUT_STATIONARY,
    SA_DATAFLOW_ROW_STATIONARY
} SaDataflow;

typedef enum {
    SA_PREC_INT8,
    SA_PREC_INT16,
    SA_PREC_BF16,
    SA_PREC_FP32
} SaPrecision;

typedef struct {
    int32_t accumulator;
    int8_t  weight;
    int8_t  weight_alt;
    int8_t  act_in;
    int8_t  act_out;
    bool    weight_valid;
    bool    weight_alt_valid;
    bool    act_valid;
    bool    mac_done;
    uint8_t pipe_stage;
    uint32_t stall_cycles;
} SAPE;

typedef struct {
    SAPE grid[SA_MAX_ROWS][SA_MAX_COLS];
    int32_t col_accum[SA_MAX_COLS];
    int32_t row_partial[SA_MAX_ROWS];
    uint32_t num_rows;
    uint32_t num_cols;
    uint32_t pipeline_depth;
    SaDataflow dataflow;
    SaPrecision precision;
    uint64_t cycle_counter;
    uint64_t mac_counter;
    uint64_t idle_cycles;
    bool clock_gated[SA_MAX_ROWS][SA_MAX_COLS];
    bool global_reset;
} SystolicArray;

void sa_init(SystolicArray *sa, uint32_t rows, uint32_t cols,
             SaDataflow df, SaPrecision prec);
void sa_reset(SystolicArray *sa);
void sa_set_pipeline_depth(SystolicArray *sa, uint32_t depth);
void sa_load_weights(SystolicArray *sa, const int8_t *weights,
                     uint32_t weight_rows, uint32_t weight_cols);
void sa_load_weights_tiled(SystolicArray *sa, const int8_t *weights,
                           uint32_t w_rows, uint32_t w_cols,
                           uint32_t tile_h, uint32_t tile_w);
void sa_broadcast_activation(SystolicArray *sa, uint32_t row,
                             const int8_t *activations, uint32_t count);
void sa_broadcast_all_rows(SystolicArray *sa, const int8_t *activations,
                           uint32_t rows, uint32_t cols);
void sa_compute_cycle(SystolicArray *sa);
void sa_compute_n_cycles(SystolicArray *sa, uint32_t n);
void sa_read_col_outputs(SystolicArray *sa, int32_t *outputs, uint32_t col_count);
void sa_read_row_outputs(SystolicArray *sa, int32_t *outputs, uint32_t row_count);
void sa_drain_pipeline(SystolicArray *sa);
int32_t sa_pe_read_accumulator(const SystolicArray *sa, uint32_t row, uint32_t col);
bool sa_is_pipeline_full(const SystolicArray *sa);
bool sa_is_idle(const SystolicArray *sa);
uint64_t sa_total_macs(const SystolicArray *sa);
double sa_utilization(const SystolicArray *sa);
uint64_t sa_estimate_ops_per_cycle(const SystolicArray *sa);
double sa_estimate_tops(const SystolicArray *sa, uint32_t frequency_mhz);
uint64_t sa_estimate_latency_cycles(const SystolicArray *sa,
                                     uint32_t m, uint32_t n, uint32_t k);
void sa_dump_state(const SystolicArray *sa);
void sa_enable_clock_gating(SystolicArray *sa, uint32_t row, uint32_t col, bool en);
uint32_t sa_active_pe_count(const SystolicArray *sa);

#endif
