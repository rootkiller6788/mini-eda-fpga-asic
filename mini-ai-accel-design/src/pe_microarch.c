/* ================================================================
 * pe_microarch.c — Processing Element microarchitecture
 *
 * Implements: MAC pipeline, activation functions, precision
 * conversion, zero-gating, PE array operations.
 *
 * L1: PE state management
 * L2: MAC operation semantics, pipeline stages
 * L3: 7-stage PE pipeline with data gating
 * L5: Precision conversion algorithm, activation functions
 * L7: Application — PE array for CNN inference
 *
 * Course mapping:
 *   MIT 6.004, Berkeley CS 152, CMU 15-740
 *   Stanford CS 149 (Parallel Computing)
 * ================================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "pe_microarch.h"

/* ================================================================
 * Default configuration
 * ================================================================ */

pe_config_t pe_default_config(void) {
    pe_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.input_precision  = PE_FP16;
    cfg.weight_precision = PE_FP16;
    cfg.accum_precision  = PE_FP32;
    cfg.output_precision = PE_FP16;
    cfg.activation       = PE_ACT_RELU;
    cfg.leaky_relu_alpha = 0.01;
    cfg.use_fma          = true;
    cfg.enable_zero_gating = true;
    cfg.enable_near_zero_skip = false;
    cfg.zero_threshold   = 1e-8;
    cfg.pipeline_depth   = 4;
    cfg.enable_sparse_skip = false;
    return cfg;
}

/* ================================================================
 * PE initialization and reset
 * ================================================================ */

void pe_init(pe_state_t *pe, uint32_t id, uint32_t row, uint32_t col,
             const pe_config_t *cfg) {
    if (!pe) return;
    memset(pe, 0, sizeof(*pe));
    pe->pe_id = id;
    pe->row   = row;
    pe->col   = col;
    pe->stage = PE_STAGE_IDLE;
    if (cfg) {
        pe->config = *cfg;
    } else {
        pe->config = pe_default_config();
    }
    /* Pipeline stage latencies (cycles per stage) */
    pe->stage_latency[PE_STAGE_WEIGHT_LD]  = 1;
    pe->stage_latency[PE_STAGE_INPUT_LD]   = 1;
    pe->stage_latency[PE_STAGE_MULTIPLY]   = 2;
    pe->stage_latency[PE_STAGE_ACCUMULATE] = 2;
    pe->stage_latency[PE_STAGE_ACTIVATE]   = 1;
    pe->stage_latency[PE_STAGE_OUTPUT_ST]  = 1;
}

void pe_reset(pe_state_t *pe) {
    if (!pe) return;
    pe->weight_reg    = 0.0;
    pe->input_reg     = 0.0;
    pe->product_reg   = 0.0;
    pe->accum_reg     = 0.0;
    pe->output_reg    = 0.0;
    pe->weight_ready  = false;
    pe->input_ready   = false;
    pe->accum_valid   = false;
    pe->stalled       = false;
    pe->stage         = PE_STAGE_IDLE;
    pe->cycle_in_stage = 0;
    pe->active_cycles  = 0;
}

void pe_set_config(pe_state_t *pe, const pe_config_t *cfg) {
    if (!pe || !cfg) return;
    pe->config = *cfg;
}

/* ================================================================
 * Data loading — feed PE inputs
 * ================================================================ */

void pe_load_weight(pe_state_t *pe, double w) {
    if (!pe) return;
    /* L2: Weight stationary — weight is preloaded and reused */
    pe->weight_reg   = w;
    pe->weight_ready  = true;
    pe->stage         = PE_STAGE_WEIGHT_LD;
    pe->cycle_in_stage = 0;
}

void pe_load_input(pe_state_t *pe, double x) {
    if (!pe) return;
    /* Zero gating: skip multiplication if input is near zero */
    if (pe->config.enable_zero_gating && fabs(x) < pe->config.zero_threshold) {
        pe->zero_skipped++;
        pe->input_ready = false;
        return;
    }
    if (pe->config.enable_near_zero_skip && fabs(x) < pe->config.zero_threshold * 10.0) {
        pe->zero_skipped++;
    }
    pe->input_reg   = x;
    pe->input_ready  = true;
    pe->stage        = PE_STAGE_INPUT_LD;
    pe->cycle_in_stage = 0;
}

void pe_set_accum(pe_state_t *pe, double acc) {
    if (!pe) return;
    pe->accum_reg   = acc;
    pe->accum_valid  = true;
}

double pe_read_output(const pe_state_t *pe) {
    if (!pe) return 0.0;
    return pe->output_reg;
}

/* ================================================================
 * Arithmetic operations — L2: MAC and activation functions
 * MAC = multiply-accumulate: c += a * b
 * FMA = fused multiply-add: single-rounding a * b + c
 * ================================================================ */

double pe_compute_mac(double a, double b, double c) {
    /* Standard MAC: first multiply, then add */
    double product = a * b;
    return c + product;
}

double pe_compute_fma(double a, double b, double c) {
    /* FMA: fused — in real hardware uses single rounding step.
     * In C simulation we approximate as a single expression. */
    return a * b + c;
}

double pe_apply_activation(pe_activation_t act, double x, double alpha) {
    switch (act) {
    case PE_ACT_NONE:
    case PE_ACT_IDENTITY:
        return x;
    case PE_ACT_RELU:
        return (x > 0.0) ? x : 0.0;
    case PE_ACT_LEAKY_RELU:
        return (x > 0.0) ? x : alpha * x;
    case PE_ACT_SIGMOID:
        return 1.0 / (1.0 + exp(-x));
    case PE_ACT_TANH:
        return tanh(x);
    case PE_ACT_GELU:
        return pe_gelu(x);
    case PE_ACT_SWISH:
        return x / (1.0 + exp(-x));  /* x * sigmoid(x) */
    case PE_ACT_SOFTMAX:
        return exp(x);  /* softmax is typically vector-level; here just exp */
    default:
        return x;
    }
}

/* ================================================================
 * Named activation functions (standalone implementations)
 * ================================================================ */

double pe_relu(double x) {
    return (x > 0.0) ? x : 0.0;
}

double pe_leaky_relu(double x, double alpha) {
    return (x > 0.0) ? x : alpha * x;
}

double pe_sigmoid(double x) {
    /* Numerically stable sigmoid */
    if (x >= 0.0) {
        return 1.0 / (1.0 + exp(-x));
    } else {
        double ex = exp(x);
        return ex / (1.0 + ex);
    }
}

double pe_tanh_approx(double x) {
    /* Standard tanh; for hardware could use piecewise linear */
    return tanh(x);
}

double pe_gelu(double x) {
    /* GELU: x * Phi(x) where Phi is Gaussian CDF
     * Approximation: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
     */
    double x3 = x * x * x;
    double inner = sqrt(2.0 / M_PI) * (x + 0.044715 * x3);
    return 0.5 * x * (1.0 + tanh(inner));
}

double pe_swish(double x) {
    /* Swish: x * sigmoid(x) */
    return x * pe_sigmoid(x);
}

/* ================================================================
 * Precision conversion — L5: quantization and truncation
 * Simulates hardware precision by truncating mantissa bits.
 * ================================================================ */

int pe_precision_bits(pe_precision_t prec) {
    switch (prec) {
    case PE_FP32:  return 32;
    case PE_FP16:  return 16;
    case PE_BF16:  return 16;
    case PE_INT8:  return 8;
    case PE_INT4:  return 4;
    case PE_INT16: return 16;
    case PE_FP8:   return 8;
    default:       return 32;
    }
}

double pe_precision_min_value(pe_precision_t prec) {
    switch (prec) {
    case PE_FP32:  return -3.4e38;
    case PE_FP16:  return -65504.0;
    case PE_BF16:  return -3.4e38;
    case PE_INT8:  return -128.0;
    case PE_INT4:  return -8.0;
    case PE_INT16: return -32768.0;
    case PE_FP8:   return -240.0;
    default:       return -3.4e38;
    }
}

double pe_precision_max_value(pe_precision_t prec) {
    switch (prec) {
    case PE_FP32:  return 3.4e38;
    case PE_FP16:  return 65504.0;
    case PE_BF16:  return 3.4e38;
    case PE_INT8:  return 127.0;
    case PE_INT4:  return 7.0;
    case PE_INT16: return 32767.0;
    case PE_FP8:   return 240.0;
    default:       return 3.4e38;
    }
}

double pe_truncate_to_precision(double val, pe_precision_t prec) {
    /* Simulate reduced precision by truncating mantissa.
     * For integer types, round to nearest integer. */
    switch (prec) {
    case PE_INT8:  return round(val);
    case PE_INT4:  return round(val);
    case PE_INT16: return round(val);
    case PE_FP16:
    case PE_BF16: {
        /* Simulate reduced mantissa precision by quantizing */
        double scale = (double)(1 << 10); /* 10-bit mantissa for FP16 */
        return round(val * scale) / scale;
    }
    case PE_FP8: {
        double scale = (double)(1 << 3); /* 3-bit mantissa for FP8 */
        return round(val * scale) / scale;
    }
    case PE_FP32:
    default:
        return val;
    }
}

void pe_convert_precision(double *val, pe_precision_t from, pe_precision_t to) {
    if (!val) return;
    if (from == to) return;
    double min_val = pe_precision_min_value(to);
    double max_val = pe_precision_max_value(to);
    double result = pe_truncate_to_precision(*val, to);
    /* Clamp to range */
    if (result < min_val) result = min_val;
    if (result > max_val) result = max_val;
    *val = result;
}

/* ================================================================
 * Zero gating — L5: energy-saving technique
 * ================================================================ */

bool pe_check_zero_gate(const pe_state_t *pe) {
    if (!pe || !pe->config.enable_zero_gating) return false;
    return (fabs(pe->input_reg) < pe->config.zero_threshold);
}

bool pe_check_near_zero(const pe_state_t *pe, double threshold) {
    if (!pe) return false;
    return (fabs(pe->input_reg) < threshold) || (fabs(pe->weight_reg) < threshold);
}

/* ================================================================
 * Pipeline execution — L3: single-cycle PE step
 * 7-stage pipeline: IDLE → W_LD → I_LD → MUL → ACC → ACT → OUT → IDLE
 * ================================================================ */

void pe_cycle(pe_state_t *pe) {
    if (!pe) return;

    if (pe->stalled) {
        pe->stall_cycles++;
        return;
    }

    pe->active_cycles++;
    pe->cycle_in_stage++;

    switch (pe->stage) {
    case PE_STAGE_IDLE:
        /* Wait for weight and input */
        if (pe->weight_ready && pe->input_ready) {
            pe->stage = PE_STAGE_MULTIPLY;
            pe->cycle_in_stage = 0;
        }
        break;

    case PE_STAGE_WEIGHT_LD:
        /* Weight load complete — transition to input wait */
        if (pe->cycle_in_stage >= pe->stage_latency[PE_STAGE_WEIGHT_LD]) {
            pe->stage = PE_STAGE_IDLE;
            pe->cycle_in_stage = 0;
        }
        break;

    case PE_STAGE_INPUT_LD:
        /* Input load complete */
        if (pe->cycle_in_stage >= pe->stage_latency[PE_STAGE_INPUT_LD]) {
            pe->stage = PE_STAGE_IDLE;
            pe->cycle_in_stage = 0;
        }
        break;

    case PE_STAGE_MULTIPLY:
        /* {weight, input} → product */
        if (pe->cycle_in_stage >= pe->stage_latency[PE_STAGE_MULTIPLY]) {
            if (pe->config.use_fma) {
                pe->product_reg = pe_compute_fma(pe->weight_reg, pe->input_reg, 0.0);
            } else {
                pe->product_reg = pe->weight_reg * pe->input_reg;
            }
            pe->mac_count++;
            pe->stage = PE_STAGE_ACCUMULATE;
            pe->cycle_in_stage = 0;
        }
        break;

    case PE_STAGE_ACCUMULATE:
        /* product + accum_reg → new accum */
        if (pe->cycle_in_stage >= pe->stage_latency[PE_STAGE_ACCUMULATE]) {
            if (pe->config.use_fma) {
                pe->accum_reg = pe_compute_fma(pe->weight_reg, pe->input_reg, pe->accum_reg);
            } else {
                pe->accum_reg = pe_compute_mac(pe->weight_reg, pe->input_reg, pe->accum_reg);
            }
            pe->accum_valid = true;
            pe->mac_count++;
            pe->stage = PE_STAGE_ACTIVATE;
            pe->cycle_in_stage = 0;
        }
        break;

    case PE_STAGE_ACTIVATE:
        /* Apply activation function to accum */
        if (pe->cycle_in_stage >= pe->stage_latency[PE_STAGE_ACTIVATE]) {
            pe->output_reg = pe_apply_activation(pe->config.activation,
                                                   pe->accum_reg,
                                                   pe->config.leaky_relu_alpha);
            /* Apply output precision truncation */
            pe_convert_precision(&pe->output_reg, pe->config.accum_precision,
                                  pe->config.output_precision);
            pe->stage = PE_STAGE_OUTPUT_ST;
            pe->cycle_in_stage = 0;
        }
        break;

    case PE_STAGE_OUTPUT_ST:
        /* Output is ready — signal completion */
        if (pe->cycle_in_stage >= pe->stage_latency[PE_STAGE_OUTPUT_ST]) {
            pe->stage = PE_STAGE_DONE;
            pe->cycle_in_stage = 0;
        }
        break;

    case PE_STAGE_DONE:
        /* Reset for next computation */
        pe->input_ready  = false;
        pe->weight_ready = false;
        pe->accum_valid  = false;
        pe->product_reg  = 0.0;
        /* Keep output_reg for reading */
        pe->stage = PE_STAGE_IDLE;
        pe->cycle_in_stage = 0;
        break;
    }
}

void pe_flush_pipeline(pe_state_t *pe) {
    if (!pe) return;
    /* Force-complete all stages */
    pe->product_reg = pe->weight_reg * pe->input_reg;
    pe->accum_reg  += pe->product_reg;
    pe->output_reg  = pe_apply_activation(pe->config.activation,
                                           pe->accum_reg,
                                           pe->config.leaky_relu_alpha);
    pe->stage = PE_STAGE_DONE;
    pe->weight_ready = false;
    pe->input_ready  = false;
}

bool pe_is_idle(const pe_state_t *pe) {
    return pe && (pe->stage == PE_STAGE_IDLE || pe->stage == PE_STAGE_DONE);
}

bool pe_is_stalled(const pe_state_t *pe) {
    return pe && pe->stalled;
}

/* ================================================================
 * Utilization analysis
 * ================================================================ */

double pe_calculate_utilization(const pe_state_t *pe) {
    if (!pe || pe->active_cycles == 0) return 0.0;
    return (double)(pe->active_cycles - pe->stall_cycles) / (double)pe->active_cycles;
}

/* ================================================================
 * PE Array — L3: collection of PEs operating in parallel
 * ================================================================ */

void pe_array_init(pe_array_t *arr, uint32_t rows, uint32_t cols,
                   const pe_config_t *cfg) {
    if (!arr) return;
    memset(arr, 0, sizeof(*arr));
    arr->rows = rows;
    arr->cols = cols;
    if (cfg) arr->default_config = *cfg;
    else arr->default_config = pe_default_config();

    arr->pes = (pe_state_t **)malloc(rows * sizeof(pe_state_t *));
    if (!arr->pes) { arr->rows = 0; arr->cols = 0; return; }
    for (uint32_t r = 0; r < rows; r++) {
        arr->pes[r] = (pe_state_t *)malloc(cols * sizeof(pe_state_t));
        if (!arr->pes[r]) {
            for (uint32_t rr = 0; rr < r; rr++) free(arr->pes[rr]);
            free(arr->pes);
            arr->pes = NULL; arr->rows = 0; arr->cols = 0;
            return;
        }
        for (uint32_t c = 0; c < cols; c++) {
            pe_init(&arr->pes[r][c], r * cols + c, r, c, &arr->default_config);
        }
    }
}

void pe_array_free(pe_array_t *arr) {
    if (!arr || !arr->pes) return;
    for (uint32_t r = 0; r < arr->rows; r++) {
        free(arr->pes[r]);
    }
    free(arr->pes);
    arr->pes = NULL;
    arr->rows = 0;
    arr->cols = 0;
}

void pe_array_step(pe_array_t *arr) {
    if (!arr || !arr->pes) return;
    arr->global_cycle++;
    for (uint32_t r = 0; r < arr->rows; r++) {
        for (uint32_t c = 0; c < arr->cols; c++) {
            pe_cycle(&arr->pes[r][c]);
        }
    }
}

void pe_array_load_weights(pe_array_t *arr, const double *weights,
                            uint32_t w_rows, uint32_t w_cols) {
    if (!arr || !arr->pes || !weights) return;
    for (uint32_t r = 0; r < w_rows && r < arr->rows; r++) {
        for (uint32_t c = 0; c < w_cols && c < arr->cols; c++) {
            pe_load_weight(&arr->pes[r][c], weights[r * w_cols + c]);
        }
    }
}

void pe_array_load_inputs(pe_array_t *arr, const double *inputs,
                           uint32_t i_rows, uint32_t i_cols) {
    if (!arr || !arr->pes || !inputs) return;
    for (uint32_t r = 0; r < i_rows && r < arr->rows; r++) {
        for (uint32_t c = 0; c < i_cols && c < arr->cols; c++) {
            pe_load_input(&arr->pes[r][c], inputs[r * i_cols + c]);
        }
    }
}

void pe_array_read_outputs(pe_array_t *arr, double *outputs,
                            uint32_t o_rows, uint32_t o_cols) {
    if (!arr || !arr->pes || !outputs) return;
    for (uint32_t r = 0; r < o_rows && r < arr->rows; r++) {
        for (uint32_t c = 0; c < o_cols && c < arr->cols; c++) {
            outputs[r * o_cols + c] = pe_read_output(&arr->pes[r][c]);
        }
    }
}

void pe_array_collect_perf(const pe_array_t *arr, pe_perf_t *perf) {
    if (!arr || !perf) return;
    memset(perf, 0, sizeof(*perf));
    for (uint32_t r = 0; r < arr->rows; r++) {
        for (uint32_t c = 0; c < arr->cols; c++) {
            const pe_state_t *pe = &arr->pes[r][c];
            perf->total_macs += pe->mac_count;
            perf->total_cycles += pe->active_cycles;
            perf->total_stalls += pe->stall_cycles;
            perf->zero_gating_rate += pe->zero_skipped;
        }
    }
    uint64_t total_pe = arr->rows * arr->cols;
    if (total_pe > 0) {
        perf->zero_gating_rate /= (double)total_pe;
        if (perf->total_cycles > 0) {
            perf->utilization = 1.0 - (double)perf->total_stalls / (double)perf->total_cycles;
        } else {
            perf->utilization = 0.0;
        }
    }
    /* Throughput estimate: MACs / cycles * clock (assume 1 GHz) */
    if (arr->global_cycle > 0) {
        perf->throughput_gmac_s = (double)perf->total_macs / (double)arr->global_cycle;
    }
    perf->energy_per_mac_pj = 0.9;  /* typical 45nm ~0.9 pJ/MAC */
    perf->pipeline_efficiency = perf->utilization;
}

/* ================================================================
 * String / debug utilities
 * ================================================================ */

const char *pe_stage_name(pe_pipeline_stage_t stage) {
    static const char *names[] = {
        "IDLE","WEIGHT_LD","INPUT_LD","MULTIPLY",
        "ACCUMULATE","ACTIVATE","OUTPUT_ST","DONE"
    };
    if (stage <= PE_STAGE_DONE) return names[stage];
    return "UNKNOWN";
}

const char *pe_precision_name(pe_precision_t prec) {
    static const char *names[] = {"FP32","FP16","BF16","INT8","INT4","INT16","FP8"};
    if (prec <= PE_FP8) return names[prec];
    return "UNKNOWN";
}

const char *pe_activation_name(pe_activation_t act) {
    static const char *names[] = {
        "NONE","RELU","LEAKY_RELU","SIGMOID","TANH","GELU","SWISH","SOFTMAX","IDENTITY"
    };
    if (act <= PE_ACT_IDENTITY) return names[act];
    return "UNKNOWN";
}

void pe_print_state(const pe_state_t *pe) {
    if (!pe) return;
    printf("PE[%u]@(%u,%u) stage=%s w=%.4f x=%.4f acc=%.4f out=%.4f macs=%llu stalls=%llu\n",
           pe->pe_id, pe->row, pe->col,
           pe_stage_name(pe->stage),
           pe->weight_reg, pe->input_reg,
           pe->accum_reg, pe->output_reg,
           (unsigned long long)pe->mac_count,
           (unsigned long long)pe->stall_cycles);
}