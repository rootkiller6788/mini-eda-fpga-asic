/* ================================================================
 * dnn_isa.c — DNN ISA implementation
 *
 * Implements: instruction encoding/decoding, program construction,
 * execution engine, statistics collection, validation.
 *
 * L1: ISA struct/typedef implementations
 * L2: Instruction set semantics (fetch/decode/execute cycle)
 * L3: Pipeline modeling, register allocation
 * L4: Amdahl's Law verification via ISA specialization analysis
 * L5: Instruction scheduling / basic block analysis
 *
 * Course mapping:
 *   MIT 6.004 (ISA design), Berkeley CS 152 (Processor architecture)
 *   CMU 15-740 (Computer Architecture), Stanford CS 149 (Parallel)
 * ================================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dnn_isa.h"

/* ================================================================
 * Opcode metadata — L1: complete opcode table with categories
 * ================================================================ */

typedef struct {
    dnn_opcode_t opcode;
    const char  *name;
    const char  *category;
    uint32_t     min_operands;
    uint32_t     max_operands;
    uint32_t     latency;       /* base latency in cycles */
    uint32_t     throughput;    /* throughput (1 = fully pipelined) */
    bool         is_fp;
    bool         is_int;
} opcode_meta_t;

static const opcode_meta_t opcode_table[] = {
    {DNN_OP_NOP,       "NOP",       "control", 0, 0, 0, 1, false, false},
    {DNN_OP_HALT,      "HALT",      "control", 0, 0, 0, 1, false, false},
    {DNN_OP_SYNC,      "SYNC",      "control", 0, 0, 1, 1, false, false},
    {DNN_OP_BARRIER,   "BARRIER",   "control", 0, 0, 2, 1, false, false},
    {DNN_OP_ADD,       "ADD",       "scalar",  2, 3, 1, 1, true,  true},
    {DNN_OP_SUB,       "SUB",       "scalar",  2, 3, 1, 1, true,  true},
    {DNN_OP_MUL,       "MUL",       "scalar",  2, 3, 2, 1, true,  true},
    {DNN_OP_DIV,       "DIV",       "scalar",  2, 3, 4, 1, true,  true},
    {DNN_OP_MAX,       "MAX",       "scalar",  2, 3, 1, 1, true,  true},
    {DNN_OP_MIN,       "MIN",       "scalar",  2, 3, 1, 1, true,  true},
    {DNN_OP_RELU,      "RELU",      "scalar",  1, 2, 1, 1, true,  false},
    {DNN_OP_LEAKY_RELU,"LEAKY_RELU","scalar",  1, 3, 1, 1, true,  false},
    {DNN_OP_SIGMOID,   "SIGMOID",   "scalar",  1, 2, 8, 1, true,  false},
    {DNN_OP_TANH,      "TANH",      "scalar",  1, 2, 8, 1, true,  false},
    {DNN_OP_EXP,       "EXP",       "scalar",  1, 2, 8, 1, true,  false},
    {DNN_OP_SQRT,      "SQRT",      "scalar",  1, 2, 4, 1, true,  false},
    {DNN_OP_ABS,       "ABS",       "scalar",  1, 2, 1, 1, true,  false},
    {DNN_OP_VADD,      "VADD",      "vector",  2, 4, 1, 1, true,  true},
    {DNN_OP_VMUL,      "VMUL",      "vector",  2, 4, 1, 1, true,  true},
    {DNN_OP_VMADD,     "VMADD",     "vector",  3, 5, 1, 1, true,  true},
    {DNN_OP_VDOT,      "VDOT",      "vector",  2, 4, 1, 1, true,  true},
    {DNN_OP_VRELU,     "VRELU",     "vector",  1, 3, 1, 1, true,  false},
    {DNN_OP_VSOFTMAX,  "VSOFTMAX",  "vector",  1, 3, 12, 1, true,  false},
    {DNN_OP_VLAYERNORM,"VLAYERNORM","vector",  2, 5, 6, 1, true,  false},
    {DNN_OP_VBATCHNORM,"VBATCHNORM","vector",  2, 5, 4, 1, true,  false},
    {DNN_OP_VREDUCE_SUM,"VREDUCE_SUM","vector",1, 3, 2, 1, true,  false},
    {DNN_OP_VREDUCE_MAX,"VREDUCE_MAX","vector",1, 3, 2, 1, true,  false},
    {DNN_OP_MATMUL,    "MATMUL",    "matrix",  2, 5, 32, 1, true,  true},
    {DNN_OP_MATMUL_BIAS,"MATMUL_BIAS","matrix",3, 6, 33, 1, true,  true},
    {DNN_OP_CONV2D,    "CONV2D",    "matrix",  2, 8, 64, 1, true,  true},
    {DNN_OP_DEPTHWISE_CONV,"DEPTHWISE_CONV","matrix",2,8,48,1,true,true},
    {DNN_OP_MAXPOOL,   "MAXPOOL",   "matrix",  1, 4, 4, 1, true,  false},
    {DNN_OP_AVGPOOL,   "AVGPOOL",   "matrix",  1, 4, 4, 1, true,  false},
    {DNN_OP_TRANSPOSE, "TRANSPOSE", "matrix",  1, 3, 2, 1, true,  false},
    {DNN_OP_IM2COL,    "IM2COL",    "matrix",  1, 6, 8, 1, true,  false},
    {DNN_OP_LD,        "LD",        "memory",  1, 3, 4, 1, false, false},
    {DNN_OP_ST,        "ST",        "memory",  1, 3, 4, 1, false, false},
    {DNN_OP_LD_MATRIX, "LD_MATRIX", "memory",  1, 4, 8, 1, false, false},
    {DNN_OP_ST_MATRIX, "ST_MATRIX", "memory",  1, 4, 8, 1, false, false},
    {DNN_OP_PREFETCH,  "PREFETCH",  "memory",  1, 3, 2, 1, false, false},
    {DNN_OP_DMA_COPY,  "DMA_COPY",  "memory",  2, 4, 8, 1, false, false},
    {DNN_OP_DMA_WAIT,  "DMA_WAIT",  "memory",  0, 0, 1, 1, false, false},
    {DNN_OP_ZERO,      "ZERO",      "memory",  1, 2, 1, 1, false, false},
    {DNN_OP_JMP,       "JMP",       "control", 1, 1, 1, 1, false, false},
    {DNN_OP_JZ,        "JZ",        "control", 2, 2, 1, 1, false, false},
    {DNN_OP_JNZ,       "JNZ",       "control", 2, 2, 1, 1, false, false},
    {DNN_OP_LOOP,      "LOOP",      "control", 2, 2, 1, 1, false, false},
    {DNN_OP_CALL,      "CALL",      "control", 1, 1, 2, 1, false, false},
    {DNN_OP_RET,       "RET",       "control", 0, 0, 2, 1, false, false},
    {DNN_OP_SET_DTYPE, "SET_DTYPE", "config",  1, 1, 1, 1, false, false},
    {DNN_OP_SET_DATAFLOW,"SET_DATAFLOW","config",1,1,1,1,false,false},
    {DNN_OP_SET_TILE,  "SET_TILE",  "config",  4, 4, 1, 1, false, false},
    {DNN_OP_SET_ARRAY, "SET_ARRAY", "config",  2, 2, 1, 1, false, false},
    {DNN_OP_SET_PRECISION,"SET_PRECISION","config",1,1,1,1,false,false},
    {DNN_OP_SPARSE_MATMUL,"SPARSE_MATMUL","sparse",2,6,20,1,true,true},
    {DNN_OP_DENSE_TO_SPARSE,"DENSE_TO_SPARSE","sparse",1,3,4,1,false,false},
    {DNN_OP_SPARSE_TO_DENSE,"SPARSE_TO_DENSE","sparse",1,3,4,1,false,false},
    {DNN_OP_SPARSITY_MASK,"SPARSITY_MASK","sparse",1,3,2,1,false,false},
};

#define OPMETA_COUNT (sizeof(opcode_table) / sizeof(opcode_table[0]))

static const opcode_meta_t *find_meta(dnn_opcode_t op) {
    for (size_t i = 0; i < OPMETA_COUNT; i++) {
        if (opcode_table[i].opcode == op) return &opcode_table[i];
    }
    return NULL;
}

/* ================================================================
 * String utilities
 * ================================================================ */

const char *dnn_opcode_name(dnn_opcode_t op) {
    const opcode_meta_t *m = find_meta(op);
    return m ? m->name : "UNKNOWN";
}

const char *dnn_dtype_name(dnn_dtype_t dt) {
    static const char *names[] = {"FP32","FP16","BF16","INT8","INT4","INT16","FP8"};
    if (dt < DNN_DTYPE_COUNT) return names[dt];
    return "UNKNOWN";
}

const char *dnn_dataflow_name(dnn_dataflow_t df) {
    static const char *names[] = {"WS","OS","RS","NLR","IS"};
    if (df <= DNN_DATAFLOW_INPUT_STATIONARY) return names[df];
    return "UNKNOWN";
}

int dnn_dtype_bytes(dnn_dtype_t dt) {
    static const int bytes[] = {4, 2, 2, 1, 1, 2, 1};
    if (dt < DNN_DTYPE_COUNT) return bytes[dt];
    return 0;
}

/* ================================================================
 * Program management
 * ================================================================ */

void dnn_program_init(dnn_program_t *prog, const char *name) {
    if (!prog) return;
    memset(prog, 0, sizeof(*prog));
    if (name) {
        strncpy(prog->name, name, sizeof(prog->name) - 1);
    }
    prog->version = 1;
    /* Pre-allocate architectural registers */
    for (uint32_t i = 0; i < DNN_ISA_REG_COUNT; i++) {
        prog->reg_file[i].index = i;
        snprintf(prog->reg_file[i].name, DNN_ISA_REG_NAME_LEN, "r%u", i);
    }
    prog->reg_count = DNN_ISA_REG_COUNT;
}

void dnn_program_reset(dnn_program_t *prog) {
    if (!prog) return;
    dnn_program_init(prog, prog->name);
}

int dnn_program_add_instr(dnn_program_t *prog, dnn_opcode_t op, dnn_dtype_t dt) {
    if (!prog || prog->instr_count >= DNN_ISA_PROGRAM_SIZE) return -1;
    const opcode_meta_t *meta = find_meta(op);
    if (!meta) return -1;

    dnn_instruction_t *instr = &prog->instructions[prog->instr_count];
    memset(instr, 0, sizeof(*instr));
    instr->opcode = op;
    instr->dtype  = dt;
    instr->pc     = prog->instr_count;
    instr->latency_cycles = meta->latency;
    instr->throughput_cycles = meta->throughput;

    prog->instr_count++;
    return instr->pc;
}

dnn_instruction_t *dnn_program_get_instr(dnn_program_t *prog, uint32_t idx) {
    if (!prog || idx >= prog->instr_count) return NULL;
    return &prog->instructions[idx];
}

/* ================================================================
 * Operand construction — L1: operand encoding helpers
 * ================================================================ */

dnn_operand_t dnn_operand_reg(int32_t reg_idx) {
    dnn_operand_t op;
    memset(&op, 0, sizeof(op));
    op.type = DNN_OPERAND_REG;
    op.data.reg_index = reg_idx;
    return op;
}

dnn_operand_t dnn_operand_imm(int64_t value) {
    dnn_operand_t op;
    memset(&op, 0, sizeof(op));
    op.type = DNN_OPERAND_IMM;
    op.data.imm_value = value;
    return op;
}

dnn_operand_t dnn_operand_mem(uint64_t addr) {
    dnn_operand_t op;
    memset(&op, 0, sizeof(op));
    op.type = DNN_OPERAND_MEM;
    op.data.mem_addr = addr;
    return op;
}

dnn_operand_t dnn_operand_label(uint32_t label_id) {
    dnn_operand_t op;
    memset(&op, 0, sizeof(op));
    op.type = DNN_OPERAND_LABEL;
    op.data.label_id = label_id;
    return op;
}

dnn_operand_t dnn_operand_matrix(int32_t reg, uint32_t row, uint32_t col) {
    dnn_operand_t op;
    memset(&op, 0, sizeof(op));
    op.type = DNN_OPERAND_MATRIX;
    op.data.matrix.mat_reg = reg;
    op.data.matrix.row_offset = row;
    op.data.matrix.col_offset = col;
    return op;
}

dnn_operand_t dnn_operand_vector(int32_t reg, uint32_t offset, uint32_t count) {
    dnn_operand_t op;
    memset(&op, 0, sizeof(op));
    op.type = DNN_OPERAND_VECTOR;
    op.data.vector.vec_reg = reg;
    op.data.vector.element_offset = offset;
    op.data.vector.element_count = count;
    return op;
}

/* ================================================================
 * High-level emit helpers — L2: ISA macro construction
 * ================================================================ */

void dnn_emit_scalar_op(dnn_program_t *prog, dnn_opcode_t op, dnn_dtype_t dt,
                        int32_t dst, int32_t src1, int32_t src2) {
    int idx = dnn_program_add_instr(prog, op, dt);
    if (idx < 0) return;
    dnn_instruction_t *instr = &prog->instructions[idx];
    instr->operands[0] = dnn_operand_reg(dst);
    instr->operands[1] = dnn_operand_reg(src1);
    if (src2 >= 0) {
        instr->operands[2] = dnn_operand_reg(src2);
        instr->num_operands = 3;
    } else {
        instr->num_operands = 2;
    }
}

void dnn_emit_vector_op(dnn_program_t *prog, dnn_opcode_t op, dnn_dtype_t dt,
                        int32_t dst, int32_t src1, int32_t src2, uint32_t len) {
    int idx = dnn_program_add_instr(prog, op, dt);
    if (idx < 0) return;
    dnn_instruction_t *instr = &prog->instructions[idx];
    instr->operands[0] = dnn_operand_vector(dst, 0, len);
    instr->operands[1] = dnn_operand_vector(src1, 0, len);
    instr->operands[2] = dnn_operand_vector(src2, 0, len);
    instr->operands[3] = dnn_operand_imm(len);
    instr->num_operands = 4;
}

void dnn_emit_load(dnn_program_t *prog, dnn_dtype_t dt, int32_t dst,
                   uint64_t addr, uint32_t size) {
    int idx = dnn_program_add_instr(prog, DNN_OP_LD, dt);
    if (idx < 0) return;
    dnn_instruction_t *instr = &prog->instructions[idx];
    instr->operands[0] = dnn_operand_reg(dst);
    instr->operands[1] = dnn_operand_mem(addr);
    instr->operands[2] = dnn_operand_imm(size);
    instr->num_operands = 3;
}

void dnn_emit_store(dnn_program_t *prog, dnn_dtype_t dt, int32_t src,
                    uint64_t addr, uint32_t size) {
    int idx = dnn_program_add_instr(prog, DNN_OP_ST, dt);
    if (idx < 0) return;
    dnn_instruction_t *instr = &prog->instructions[idx];
    instr->operands[0] = dnn_operand_reg(src);
    instr->operands[1] = dnn_operand_mem(addr);
    instr->operands[2] = dnn_operand_imm(size);
    instr->num_operands = 3;
}

void dnn_emit_matmul(dnn_program_t *prog, dnn_dtype_t dt,
                     int32_t dst, int32_t lhs, int32_t rhs,
                     uint32_t M, uint32_t N, uint32_t K) {
    int idx = dnn_program_add_instr(prog, DNN_OP_MATMUL, dt);
    if (idx < 0) return;
    dnn_instruction_t *instr = &prog->instructions[idx];
    instr->operands[0] = dnn_operand_matrix(dst, 0, 0);
    instr->operands[1] = dnn_operand_matrix(lhs, 0, 0);
    instr->operands[2] = dnn_operand_matrix(rhs, 0, 0);
    instr->operands[3] = dnn_operand_imm(M);
    instr->operands[4] = dnn_operand_imm(N);
    instr->operands[5] = dnn_operand_imm(K);
    instr->num_operands = 6;
}

void dnn_emit_conv2d(dnn_program_t *prog, dnn_dtype_t dt,
                     int32_t dst, int32_t input, int32_t weight,
                     uint32_t H, uint32_t W, uint32_t C_in, uint32_t C_out,
                     uint32_t KH, uint32_t KW, uint32_t stride) {
    (void)KH; (void)KW; (void)stride;
    int idx = dnn_program_add_instr(prog, DNN_OP_CONV2D, dt);
    if (idx < 0) return;
    dnn_instruction_t *instr = &prog->instructions[idx];
    instr->operands[0] = dnn_operand_reg(dst);
    instr->operands[1] = dnn_operand_reg(input);
    instr->operands[2] = dnn_operand_reg(weight);
    instr->operands[3] = dnn_operand_imm(H);
    instr->operands[4] = dnn_operand_imm(W);
    instr->num_operands = 6;
    instr->operands[5] = dnn_operand_imm((uint64_t)C_in | ((uint64_t)C_out << 32));
}

void dnn_emit_loop(dnn_program_t *prog, int32_t counter_reg, uint32_t body_start) {
    int idx = dnn_program_add_instr(prog, DNN_OP_LOOP, DNN_DTYPE_INT8);
    if (idx < 0) return;
    dnn_instruction_t *instr = &prog->instructions[idx];
    instr->operands[0] = dnn_operand_reg(counter_reg);
    instr->operands[1] = dnn_operand_imm(body_start);
    instr->num_operands = 2;
    instr->is_branch = true;
    instr->branch_target = body_start;
}

void dnn_emit_jmp(dnn_program_t *prog, uint32_t target) {
    int idx = dnn_program_add_instr(prog, DNN_OP_JMP, DNN_DTYPE_INT8);
    if (idx < 0) return;
    dnn_instruction_t *instr = &prog->instructions[idx];
    instr->operands[0] = dnn_operand_imm(target);
    instr->num_operands = 1;
    instr->is_branch = true;
    instr->branch_target = target;
}

void dnn_emit_halt(dnn_program_t *prog) {
    dnn_program_add_instr(prog, DNN_OP_HALT, DNN_DTYPE_INT8);
}

/* ================================================================
 * Execution engine — L3: Pipeline model with stall/warmup
 * ================================================================ */

void dnn_exec_init(dnn_exec_context_t *ctx, dnn_program_t *prog) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->program = prog;
    ctx->pc = 0;
    ctx->cycle = 0;
    ctx->running = true;
    ctx->halted = false;
}

void dnn_exec_reset(dnn_exec_context_t *ctx) {
    if (!ctx) return;
    dnn_exec_context_t fresh;
    memset(&fresh, 0, sizeof(fresh));
    fresh.program = ctx->program;
    fresh.running = true;
    *ctx = fresh;
}

/* Execute one instruction — returns cycles consumed */
int dnn_exec_step(dnn_exec_context_t *ctx) {
    if (!ctx || !ctx->program || ctx->halted || !ctx->running) return 0;

    if (ctx->pc >= ctx->program->instr_count) {
        ctx->halted = true;
        ctx->running = false;
        return 0;
    }

    dnn_instruction_t *instr = &ctx->program->instructions[ctx->pc];
    const opcode_meta_t *meta = find_meta(instr->opcode);

    uint32_t lat = meta ? meta->latency : 1;

    /* Category tracking for statistics */
    if (meta) {
        if (strcmp(meta->category, "scalar") == 0 && instr->dtype >= DNN_DTYPE_FP32) {
            ctx->total_flops += 1;
        } else if (strcmp(meta->category, "vector") == 0) {
            uint32_t vlen = 16;
            if (instr->num_operands >= 4 && instr->operands[3].type == DNN_OPERAND_IMM)
                vlen = (uint32_t)instr->operands[3].data.imm_value;
            ctx->total_flops += vlen;
        } else if (strcmp(meta->category, "matrix") == 0) {
            uint32_t M = 0, N = 0, K = 0;
            if (instr->num_operands >= 4 && instr->operands[3].type == DNN_OPERAND_IMM)
                M = (uint32_t)instr->operands[3].data.imm_value;
            if (instr->num_operands >= 5 && instr->operands[4].type == DNN_OPERAND_IMM)
                N = (uint32_t)instr->operands[4].data.imm_value;
            if (instr->num_operands >= 6 && instr->operands[5].type == DNN_OPERAND_IMM)
                K = (uint32_t)instr->operands[5].data.imm_value;
            if (M > 0 && N > 0 && K > 0)
                ctx->total_flops += (uint64_t)M * N * K * 2;
            else
                ctx->total_flops += 64 * 64 * 2; /* default estimate */
        }
    }

    /* Memory operations count bytes */
    if (instr->opcode == DNN_OP_LD || instr->opcode == DNN_OP_ST) {
        if (instr->num_operands >= 3)
            ctx->total_bytes += (uint64_t)instr->operands[2].data.imm_value;
    }

    ctx->total_ops++;
    uint32_t next_pc = ctx->pc + 1;

    /* Branch handling */
    if (instr->is_branch) {
        ctx->branch_count++;
        next_pc = instr->branch_target;
    }

    /* HALT terminates */
    if (instr->opcode == DNN_OP_HALT) {
        ctx->halted = true;
        ctx->running = false;
    }

    ctx->pc = next_pc;
    ctx->cycle += lat;

    /* Stall modeling: stall if next PC exceeds program size */
    if (ctx->pc >= ctx->program->instr_count && !ctx->halted) {
        ctx->stall_cycles += 1;
    }

    return lat;
}

void dnn_exec_run(dnn_exec_context_t *ctx, uint32_t max_cycles) {
    if (!ctx) return;
    ctx->running = true;
    while (ctx->running && !ctx->halted && ctx->cycle < max_cycles) {
        dnn_exec_step(ctx);
    }
}

/* ================================================================
 * ISA Statistics Collection — L4: quantitative analysis
 * ================================================================ */

void dnn_isa_collect_stats(const dnn_program_t *prog, dnn_isa_stats_t *stats) {
    if (!prog || !stats) return;
    memset(stats, 0, sizeof(*stats));
    stats->total_instr = prog->instr_count;

    uint32_t total_latency = 0;
    for (uint32_t i = 0; i < prog->instr_count; i++) {
        const dnn_instruction_t *instr = &prog->instructions[i];
        const opcode_meta_t *meta = find_meta(instr->opcode);
        const char *cat = meta ? meta->category : "unknown";

        if (strcmp(cat, "scalar") == 0)       stats->scalar_ops++;
        else if (strcmp(cat, "vector") == 0)  stats->vector_ops++;
        else if (strcmp(cat, "matrix") == 0)  stats->matrix_ops++;
        else if (strcmp(cat, "memory") == 0)  stats->memory_ops++;
        else if (strcmp(cat, "control") == 0) stats->control_ops++;
        else if (strcmp(cat, "config") == 0)  stats->config_ops++;
        else if (strcmp(cat, "sparse") == 0)  stats->sparse_ops++;

        total_latency += instr->latency_cycles;
    }
    stats->cycles = total_latency;

    uint64_t total_bytes = (uint64_t)prog->instr_count * DNN_ISA_INSTR_SIZE_BYTES;
    uint64_t total_ops = stats->scalar_ops + stats->vector_ops * 16 +
                         stats->matrix_ops * 64;
    stats->ops_per_byte = (total_bytes > 0)
        ? (double)total_ops / (double)total_bytes : 0.0;

    /* Arithmetic intensity = total FLOP / total DRAM bytes */
    uint64_t dram_bytes_est = stats->memory_ops * 64;
    stats->arithmetic_intensity = (dram_bytes_est > 0)
        ? (double)total_ops / (double)dram_bytes_est : INFINITY;
    stats->compute_utilization = (prog->total_cycles > 0)
        ? (double)stats->cycles / (double)prog->total_cycles : 1.0;
}

double dnn_isa_compute_arithmetic_intensity(const dnn_program_t *prog) {
    dnn_isa_stats_t stats;
    dnn_isa_collect_stats(prog, &stats);
    return stats.arithmetic_intensity;
}

/* ================================================================
 * ISA Validation — L4: structural and semantic checks
 * ================================================================ */

bool dnn_isa_validate(const dnn_program_t *prog) {
    if (!prog) return false;
    if (prog->instr_count == 0) return true;  /* empty program is valid */
    if (prog->instr_count > DNN_ISA_PROGRAM_SIZE) return false;

    for (uint32_t i = 0; i < prog->instr_count; i++) {
        const dnn_instruction_t *instr = &prog->instructions[i];
        const opcode_meta_t *meta = find_meta(instr->opcode);
        if (!meta) return false;  /* unknown opcode */

        /* Check operand count range */
        if (instr->num_operands < meta->min_operands ||
            instr->num_operands > meta->max_operands)
            return false;

        /* Check register references */
        for (uint32_t j = 0; j < instr->num_operands; j++) {
            const dnn_operand_t *op = &instr->operands[j];
            if (op->type == DNN_OPERAND_REG) {
                if (op->data.reg_index < 0 ||
                    (uint32_t)op->data.reg_index >= DNN_ISA_REG_COUNT)
                    return false;
            }
            if (op->type == DNN_OPERAND_MATRIX) {
                if (op->data.matrix.mat_reg < 0 ||
                    (uint32_t)op->data.matrix.mat_reg >= DNN_ISA_REG_COUNT)
                    return false;
            }
        }

        /* Branch target must be valid */
        if (instr->is_branch && instr->branch_target >= prog->instr_count)
            return false;
    }
    return true;
}

/* ================================================================
 * Print utilities
 * ================================================================ */

void dnn_isa_print_program(const dnn_program_t *prog) {
    if (!prog) { printf("(null program)\n"); return; }
    printf("=== DNN ISA Program: %s (v%u, %u instrs) ===\n",
           prog->name, prog->version, prog->instr_count);
    for (uint32_t i = 0; i < prog->instr_count; i++) {
        const dnn_instruction_t *instr = &prog->instructions[i];
        printf("[%04u] %-16s %-4s", i,
               dnn_opcode_name(instr->opcode),
               dnn_dtype_name(instr->dtype));
        for (uint32_t j = 0; j < instr->num_operands; j++) {
            const dnn_operand_t *op = &instr->operands[j];
            switch (op->type) {
            case DNN_OPERAND_REG:    printf(" r%d", op->data.reg_index); break;
            case DNN_OPERAND_IMM:    printf(" #%lld", (long long)op->data.imm_value); break;
            case DNN_OPERAND_MEM:    printf(" [%llu]", (unsigned long long)op->data.mem_addr); break;
            case DNN_OPERAND_LABEL:  printf(" L%u", op->data.label_id); break;
            case DNN_OPERAND_MATRIX: printf(" M%d[%u,%u]", op->data.matrix.mat_reg,
                                        op->data.matrix.row_offset, op->data.matrix.col_offset); break;
            case DNN_OPERAND_VECTOR: printf(" V%d[%u:%u]", op->data.vector.vec_reg,
                                        op->data.vector.element_offset,
                                        op->data.vector.element_count); break;
            case DNN_OPERAND_NONE:   break;
            }
        }
        if (instr->is_branch) printf(" ->%u", instr->branch_target);
        printf(" [lat=%u]", instr->latency_cycles);
        printf("\n");
    }
}

void dnn_isa_print_stats(const dnn_isa_stats_t *stats) {
    if (!stats) return;
    printf("=== ISA Statistics ===\n");
    printf("  Total instructions: %u\n", stats->total_instr);
    printf("  Scalar ops:  %u\n", stats->scalar_ops);
    printf("  Vector ops:  %u\n", stats->vector_ops);
    printf("  Matrix ops:  %u\n", stats->matrix_ops);
    printf("  Memory ops:  %u\n", stats->memory_ops);
    printf("  Control ops: %u\n", stats->control_ops);
    printf("  Config ops:  %u\n", stats->config_ops);
    printf("  Sparse ops:  %u\n", stats->sparse_ops);
    printf("  Total cycles: %u\n", stats->cycles);
    printf("  Ops/byte:     %.2f\n", stats->ops_per_byte);
    printf("  Arithmetic intensity: %.2f FLOP/byte\n", stats->arithmetic_intensity);
    printf("  Compute utilization:  %.1f%%\n", stats->compute_utilization * 100.0);
}

/* ================================================================
 * Register allocation — simple linear allocator
 * ================================================================ */

int32_t dnn_alloc_register(dnn_program_t *prog, const char *name, dnn_dtype_t dt) {
    if (!prog) return -1;
    for (uint32_t i = 0; i < prog->reg_count; i++) {
        if (!prog->reg_file[i].in_use) {
            prog->reg_file[i].in_use = true;
            prog->reg_file[i].dtype = dt;
            if (name) strncpy(prog->reg_file[i].name, name, DNN_ISA_REG_NAME_LEN - 1);
            return (int32_t)i;
        }
    }
    return -1; /* no free registers */
}

void dnn_free_register(dnn_program_t *prog, int32_t reg) {
    if (!prog || reg < 0 || (uint32_t)reg >= prog->reg_count) return;
    prog->reg_file[reg].in_use = false;
    prog->reg_file[reg].last_access_cycle = 0;
}

/* ================================================================
 * Binary encoding/decoding — L3: RTL hardware interface
 * Fixed 16-byte instruction format:
 *   [0]    opcode (1 byte)
 *   [1]    dtype + flags (1 byte)
 *   [2:3]  num_operands + latency (2 bytes)
 *   [4:15] operands (12 bytes, 2 bytes per operand)
 * ================================================================ */

void dnn_encode_instruction(const dnn_instruction_t *instr, uint8_t *bytes, size_t max_bytes) {
    if (!instr || !bytes || max_bytes < DNN_ISA_INSTR_SIZE_BYTES) return;
    memset(bytes, 0, DNN_ISA_INSTR_SIZE_BYTES);
    bytes[0] = (uint8_t)instr->opcode;
    bytes[1] = (uint8_t)(instr->dtype & 0x0F) | (instr->is_branch ? 0x80 : 0x00);
    bytes[2] = (uint8_t)(instr->num_operands & 0xFF);
    bytes[3] = (uint8_t)(instr->latency_cycles & 0xFF);

    for (uint32_t i = 0; i < instr->num_operands && i < DNN_ISA_MAX_OPERANDS; i++) {
        bytes[4 + i * 2]     = (uint8_t)(instr->operands[i].type & 0xFF);
        bytes[4 + i * 2 + 1] = (uint8_t)(instr->operands[i].data.reg_index & 0xFF);
    }
}

bool dnn_decode_instruction(const uint8_t *bytes, size_t max_bytes, dnn_instruction_t *instr) {
    if (!bytes || !instr || max_bytes < DNN_ISA_INSTR_SIZE_BYTES) return false;
    memset(instr, 0, sizeof(*instr));
    instr->opcode = (dnn_opcode_t)bytes[0];
    instr->dtype  = (dnn_dtype_t)(bytes[1] & 0x0F);
    instr->is_branch = (bytes[1] & 0x80) != 0;
    instr->num_operands = bytes[2];
    instr->latency_cycles = bytes[3];

    for (uint32_t i = 0; i < instr->num_operands && i < DNN_ISA_MAX_OPERANDS; i++) {
        instr->operands[i].type = (dnn_operand_type_t)bytes[4 + i * 2];
        instr->operands[i].data.reg_index = bytes[4 + i * 2 + 1];
    }
    return true;
}