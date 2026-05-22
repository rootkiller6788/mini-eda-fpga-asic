#include "systolic_rtl.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

void sa_init(SystolicArray *sa, uint32_t rows, uint32_t cols,
             SaDataflow df, SaPrecision prec)
{
    if (!sa) return;
    if (rows > SA_MAX_ROWS) rows = SA_MAX_ROWS;
    if (cols > SA_MAX_COLS) cols = SA_MAX_COLS;

    memset(sa, 0, sizeof(SystolicArray));
    sa->num_rows       = rows;
    sa->num_cols       = cols;
    sa->pipeline_depth = 1;
    sa->dataflow       = df;
    sa->precision      = prec;
    sa->global_reset   = false;
}

void sa_reset(SystolicArray *sa)
{
    if (!sa) return;
    sa->global_reset = true;

    for (uint32_t r = 0; r < sa->num_rows; r++) {
        for (uint32_t c = 0; c < sa->num_cols; c++) {
            SAPE *pe = &sa->grid[r][c];
            pe->accumulator    = 0;
            pe->weight         = 0;
            pe->weight_alt     = 0;
            pe->act_in         = 0;
            pe->act_out        = 0;
            pe->weight_valid   = false;
            pe->weight_alt_valid = false;
            pe->act_valid      = false;
            pe->mac_done       = false;
            pe->pipe_stage     = 0;
            pe->stall_cycles   = 0;
            sa->clock_gated[r][c] = false;
        }
    }

    for (uint32_t c = 0; c < sa->num_cols; c++) sa->col_accum[c] = 0;
    for (uint32_t r = 0; r < sa->num_rows; r++) sa->row_partial[r] = 0;

    sa->cycle_counter = 0;
    sa->mac_counter   = 0;
    sa->idle_cycles   = 0;
    sa->global_reset  = false;
}

void sa_set_pipeline_depth(SystolicArray *sa, uint32_t depth)
{
    if (!sa) return;
    if (depth > SA_MAX_PIPELINE_DEPTH) depth = SA_MAX_PIPELINE_DEPTH;
    if (depth < 1) depth = 1;
    sa->pipeline_depth = depth;
}

void sa_load_weights(SystolicArray *sa, const int8_t *weights,
                     uint32_t weight_rows, uint32_t weight_cols)
{
    if (!sa || !weights) return;

    uint32_t rlim = weight_rows < sa->num_rows ? weight_rows : sa->num_rows;
    uint32_t clim = weight_cols < sa->num_cols ? weight_cols : sa->num_cols;

    for (uint32_t r = 0; r < rlim; r++) {
        for (uint32_t c = 0; c < clim; c++) {
            int8_t w = weights[r * weight_cols + c];
            sa->grid[r][c].weight       = w;
            sa->grid[r][c].weight_valid = true;
        }
    }
}

void sa_load_weights_tiled(SystolicArray *sa, const int8_t *weights,
                           uint32_t w_rows, uint32_t w_cols,
                           uint32_t tile_h, uint32_t tile_w)
{
    if (!sa || !weights || tile_h == 0 || tile_w == 0) return;

    uint32_t rlim = w_rows < sa->num_rows ? w_rows : sa->num_rows;
    uint32_t clim = w_cols < sa->num_cols ? w_cols : sa->num_cols;

    for (uint32_t tr = 0; tr < rlim; tr += tile_h) {
        uint32_t tr_end = (tr + tile_h) < rlim ? (tr + tile_h) : rlim;
        for (uint32_t tc = 0; tc < clim; tc += tile_w) {
            uint32_t tc_end = (tc + tile_w) < clim ? (tc + tile_w) : clim;
            for (uint32_t r = tr; r < tr_end; r++) {
                for (uint32_t c = tc; c < tc_end; c++) {
                    sa->grid[r][c].weight       = weights[r * w_cols + c];
                    sa->grid[r][c].weight_valid = true;
                }
            }
        }
    }
}

void sa_broadcast_activation(SystolicArray *sa, uint32_t row,
                             const int8_t *activations, uint32_t count)
{
    if (!sa || !activations || row >= sa->num_rows) return;

    uint32_t lim = count < sa->num_cols ? count : sa->num_cols;
    for (uint32_t c = 0; c < lim; c++) {
        sa->grid[row][c].act_in    = activations[c];
        sa->grid[row][c].act_valid = true;
    }
}

void sa_broadcast_all_rows(SystolicArray *sa, const int8_t *activations,
                           uint32_t rows, uint32_t cols)
{
    if (!sa || !activations) return;

    uint32_t rlim = rows < sa->num_rows ? rows : sa->num_rows;
    uint32_t clim = cols < sa->num_cols ? cols : sa->num_cols;

    for (uint32_t r = 0; r < rlim; r++) {
        const int8_t *row_acts = activations + r * cols;
        for (uint32_t c = 0; c < clim; c++) {
            sa->grid[r][c].act_in    = row_acts[c];
            sa->grid[r][c].act_valid = true;
        }
    }
}

void sa_compute_cycle(SystolicArray *sa)
{
    if (!sa || sa->global_reset) return;

    bool any_active = false;

    for (uint32_t r = 0; r < sa->num_rows; r++) {
        for (uint32_t c = 0; c < sa->num_cols; c++) {
            SAPE *pe = &sa->grid[r][c];

            if (sa->clock_gated[r][c]) {
                pe->stall_cycles++;
                continue;
            }

            if (pe->act_valid && pe->weight_valid) {
                int16_t prod = (int16_t)pe->act_in * (int16_t)pe->weight;
                pe->accumulator += (int32_t)prod;
                pe->mac_done = true;
                pe->act_valid = false;
                sa->mac_counter++;
                pe->stall_cycles = 0;
                any_active = true;
            } else {
                pe->stall_cycles++;
                pe->mac_done = false;
            }

            if (pe->pipe_stage > 0) pe->pipe_stage--;
        }
    }

    if (!any_active) sa->idle_cycles++;
    sa->cycle_counter++;
}

void sa_compute_n_cycles(SystolicArray *sa, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) sa_compute_cycle(sa);
}

void sa_read_col_outputs(SystolicArray *sa, int32_t *outputs, uint32_t col_count)
{
    if (!sa || !outputs) return;

    uint32_t lim = col_count < sa->num_cols ? col_count : sa->num_cols;
    for (uint32_t c = 0; c < lim; c++) {
        int32_t sum = 0;
        for (uint32_t r = 0; r < sa->num_rows; r++) {
            sum += sa->grid[r][c].accumulator;
        }
        outputs[c] = sum;
    }
}

void sa_read_row_outputs(SystolicArray *sa, int32_t *outputs, uint32_t row_count)
{
    if (!sa || !outputs) return;

    uint32_t lim = row_count < sa->num_rows ? row_count : sa->num_rows;
    for (uint32_t r = 0; r < lim; r++) {
        int32_t sum = 0;
        for (uint32_t c = 0; c < sa->num_cols; c++) {
            sum += sa->grid[r][c].accumulator;
        }
        outputs[r] = sum;
    }
}

void sa_drain_pipeline(SystolicArray *sa)
{
    if (!sa) return;
    sa_compute_n_cycles(sa, sa->pipeline_depth * 2);
}

int32_t sa_pe_read_accumulator(const SystolicArray *sa, uint32_t row, uint32_t col)
{
    if (!sa || row >= sa->num_rows || col >= sa->num_cols) return 0;
    return sa->grid[row][col].accumulator;
}

bool sa_is_pipeline_full(const SystolicArray *sa)
{
    if (!sa) return false;
    for (uint32_t r = 0; r < sa->num_rows; r++)
        for (uint32_t c = 0; c < sa->num_cols; c++)
            if (sa->grid[r][c].act_valid) return false;
    return true;
}

bool sa_is_idle(const SystolicArray *sa)
{
    if (!sa) return true;
    if (sa->cycle_counter == 0) return true;
    return (double)sa->idle_cycles / (double)sa->cycle_counter > 0.95;
}

uint64_t sa_total_macs(const SystolicArray *sa)
{
    return sa ? sa->mac_counter : 0;
}

double sa_utilization(const SystolicArray *sa)
{
    if (!sa || sa->cycle_counter == 0) return 0.0;

    uint64_t ideal_macs = (uint64_t)sa->num_rows * (uint64_t)sa->num_cols
                          * sa->cycle_counter;
    if (ideal_macs == 0) return 0.0;
    return (double)sa->mac_counter / (double)ideal_macs * 100.0;
}

uint64_t sa_estimate_ops_per_cycle(const SystolicArray *sa)
{
    if (!sa) return 0;
    return (uint64_t)sa->num_rows * (uint64_t)sa->num_cols * 2;
}

double sa_estimate_tops(const SystolicArray *sa, uint32_t frequency_mhz)
{
    if (!sa) return 0.0;
    uint64_t ops_per_cycle = sa_estimate_ops_per_cycle(sa);
    return (double)ops_per_cycle * (double)frequency_mhz * 1e-6;
}

uint64_t sa_estimate_latency_cycles(const SystolicArray *sa,
                                     uint32_t m, uint32_t n, uint32_t k)
{
    if (!sa) return 0;
    (void)m; (void)n;
    uint32_t fill   = sa->num_rows + sa->num_cols - 1;
    uint32_t rounds = (k + sa->num_rows - 1) / sa->num_rows;
    uint32_t drain  = sa->pipeline_depth * sa->num_cols;
    return fill * rounds + drain;
}

void sa_dump_state(const SystolicArray *sa)
{
    if (!sa) return;

    printf("=== Systolic Array State ===\n");
    printf("  Grid: %u x %u\n", sa->num_rows, sa->num_cols);
    printf("  Dataflow: %d, Precision: %d\n", sa->dataflow, sa->precision);
    printf("  Pipeline depth: %u\n", sa->pipeline_depth);
    printf("  Cycles: %llu, MACs: %llu\n",
           (unsigned long long)sa->cycle_counter,
           (unsigned long long)sa->mac_counter);
    printf("  Utilization: %.2f%%\n", sa_utilization(sa));
    printf("  Est. TOPS @1GHz: %.4f\n", sa_estimate_tops(sa, 1000));
}

void sa_enable_clock_gating(SystolicArray *sa, uint32_t row, uint32_t col, bool en)
{
    if (!sa || row >= sa->num_rows || col >= sa->num_cols) return;
    sa->clock_gated[row][col] = en;
}

uint32_t sa_active_pe_count(const SystolicArray *sa)
{
    if (!sa) return 0;
    uint32_t cnt = 0;
    for (uint32_t r = 0; r < sa->num_rows; r++)
        for (uint32_t c = 0; c < sa->num_cols; c++)
            if (!sa->clock_gated[r][c]) cnt++;
    return cnt;
}
