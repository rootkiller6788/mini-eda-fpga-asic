#ifndef PE_MICROARCH_H
#define PE_MICROARCH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ================================================================
 * PE Microarchitecture — Processing Element for Systolic Array
 * Modeled after Google TPU v1 PE and Eyeriss PE designs.
 *
 * L1: PE data structures, pipeline stage definitions
 * L2: MAC operation, data gating, precision modes
 * L3: Pipeline design (IF/ID/EX/MEM/WB equivalent for PE)
 * L5: MAC pipeline scheduling, precision conversion
 * ================================================================ */

#define PE_MAX_PIPELINE_STAGES 8
#define PE_MAX_PRECISION_MODES 6
#define PE_NAME_LEN            32

/* --- Precision / Numeric Format --- */
typedef enum {
    PE_FP32  = 0,
    PE_FP16  = 1,
    PE_BF16  = 2,
    PE_INT8  = 3,
    PE_INT4  = 4,
    PE_INT16 = 5,
    PE_FP8   = 6
} pe_precision_t;

/* --- Activation Function --- */
typedef enum {
    PE_ACT_NONE       = 0,
    PE_ACT_RELU       = 1,
    PE_ACT_LEAKY_RELU = 2,
    PE_ACT_SIGMOID    = 3,
    PE_ACT_TANH       = 4,
    PE_ACT_GELU       = 5,
    PE_ACT_SWISH      = 6,
    PE_ACT_SOFTMAX    = 7,
    PE_ACT_IDENTITY   = 8
} pe_activation_t;

/* --- Pipeline Stage --- */
typedef enum {
    PE_STAGE_IDLE     = 0,
    PE_STAGE_WEIGHT_LD = 1,
    PE_STAGE_INPUT_LD = 2,
    PE_STAGE_MULTIPLY = 3,
    PE_STAGE_ACCUMULATE = 4,
    PE_STAGE_ACTIVATE = 5,
    PE_STAGE_OUTPUT_ST = 6,
    PE_STAGE_DONE     = 7
} pe_pipeline_stage_t;

/* --- PE Configuration --- */
typedef struct {
    pe_precision_t  input_precision;
    pe_precision_t  weight_precision;
    pe_precision_t  accum_precision;
    pe_precision_t  output_precision;
    pe_activation_t activation;
    double          leaky_relu_alpha;
    bool            use_fma;
    bool            enable_zero_gating;
    bool            enable_near_zero_skip;
    double          zero_threshold;
    uint32_t        pipeline_depth;
    bool            enable_sparse_skip;
} pe_config_t;

/* --- PE Internal State --- */
typedef struct {
    uint32_t            pe_id;
    uint32_t            row;
    uint32_t            col;
    pe_config_t         config;

    /* Pipeline registers */
    double              weight_reg;
    double              input_reg;
    double              product_reg;
    double              accum_reg;
    double              output_reg;

    /* Control */
    bool                weight_ready;
    bool                input_ready;
    bool                accum_valid;
    bool                stalled;
    pe_pipeline_stage_t stage;

    /* Pipeline tracking */
    uint32_t            stage_latency[PE_MAX_PIPELINE_STAGES];
    uint32_t            cycle_in_stage;

    /* Statistics */
    uint64_t            mac_count;
    uint64_t            zero_skipped;
    uint64_t            stall_cycles;
    uint64_t            active_cycles;
} pe_state_t;

/* --- PE Array (collection of PEs) --- */
typedef struct {
    pe_state_t  **pes;
    uint32_t    rows;
    uint32_t    cols;
    pe_config_t default_config;
    uint32_t    global_cycle;
} pe_array_t;

/* --- PE Performance Metrics --- */
typedef struct {
    double      throughput_gmac_s;
    double      utilization;
    double      energy_per_mac_pj;
    double      zero_gating_rate;
    double      pipeline_efficiency;
    uint64_t    total_macs;
    uint64_t    total_cycles;
    uint64_t    total_stalls;
} pe_perf_t;

/* API */
void        pe_init(pe_state_t *pe, uint32_t id, uint32_t row, uint32_t col,
                    const pe_config_t *cfg);
void        pe_reset(pe_state_t *pe);
void        pe_set_config(pe_state_t *pe, const pe_config_t *cfg);
pe_config_t pe_default_config(void);

/* Data input */
void        pe_load_weight(pe_state_t *pe, double w);
void        pe_load_input(pe_state_t *pe, double x);
void        pe_set_accum(pe_state_t *pe, double acc);
double      pe_read_output(const pe_state_t *pe);

/* Pipeline execution */
void        pe_cycle(pe_state_t *pe);
void        pe_flush_pipeline(pe_state_t *pe);
bool        pe_is_idle(const pe_state_t *pe);
bool        pe_is_stalled(const pe_state_t *pe);

/* Arithmetic operations */
double      pe_compute_mac(double a, double b, double c);
double      pe_compute_fma(double a, double b, double c);
double      pe_apply_activation(pe_activation_t act, double x, double alpha);
void        pe_convert_precision(double *val, pe_precision_t from, pe_precision_t to);

/* Precision conversion utilities */
double      pe_truncate_to_precision(double val, pe_precision_t prec);
int         pe_precision_bits(pe_precision_t prec);
double      pe_precision_min_value(pe_precision_t prec);
double      pe_precision_max_value(pe_precision_t prec);

/* Zero gating */
bool        pe_check_zero_gate(const pe_state_t *pe);
bool        pe_check_near_zero(const pe_state_t *pe, double threshold);

/* PE Array batch operations */
void        pe_array_init(pe_array_t *arr, uint32_t rows, uint32_t cols,
                          const pe_config_t *cfg);
void        pe_array_free(pe_array_t *arr);
void        pe_array_step(pe_array_t *arr);
void        pe_array_load_weights(pe_array_t *arr, const double *weights,
                                   uint32_t w_rows, uint32_t w_cols);
void        pe_array_load_inputs(pe_array_t *arr, const double *inputs,
                                  uint32_t i_rows, uint32_t i_cols);
void        pe_array_read_outputs(pe_array_t *arr, double *outputs,
                                   uint32_t o_rows, uint32_t o_cols);
void        pe_array_collect_perf(const pe_array_t *arr, pe_perf_t *perf);

/* Named activation functions (standalone, for testing) */
double      pe_relu(double x);
double      pe_leaky_relu(double x, double alpha);
double      pe_sigmoid(double x);
double      pe_tanh_approx(double x);
double      pe_gelu(double x);
double      pe_swish(double x);

/* PE utilization and analysis */
double      pe_calculate_utilization(const pe_state_t *pe);
void        pe_print_state(const pe_state_t *pe);
const char *pe_stage_name(pe_pipeline_stage_t stage);
const char *pe_precision_name(pe_precision_t prec);
const char *pe_activation_name(pe_activation_t act);

#endif /* PE_MICROARCH_H */