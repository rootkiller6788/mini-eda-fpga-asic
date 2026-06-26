#ifndef DNN_ISA_H
#define DNN_ISA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ================================================================
 * DNN ISA — Custom Instruction Set Architecture for AI Accelerator
 * Inspired by Google TPU ISA, Eyeriss, and Simba architectures.
 *
 * L1: Core Definitions (struct/typedef/enum/API)
 * L2: Instruction format design, SIMD/vector semantics
 * L3: Execution model with pipelined decode/execute
 * L4: Amdahl's Law applied to ISA specialization
 * L5: Instruction scheduling algorithm
 * ================================================================ */

/* --- ISA Configuration Constants --- */
#define DNN_ISA_MAX_OPERANDS       6
#define DNN_ISA_MAX_IMM_BYTES      8
#define DNN_ISA_INSTR_SIZE_BYTES   16
#define DNN_ISA_OPCODE_BITS        8
#define DNN_ISA_REG_COUNT          64
#define DNN_ISA_REG_NAME_LEN       16
#define DNN_ISA_LABEL_MAX          64
#define DNN_ISA_PROGRAM_SIZE       4096
#define DNN_ISA_MAX_CORES          16

/* --- Data Types --- */
typedef enum {
    DNN_DTYPE_FP32  = 0,
    DNN_DTYPE_FP16  = 1,
    DNN_DTYPE_BF16  = 2,
    DNN_DTYPE_INT8   = 3,
    DNN_DTYPE_INT4   = 4,
    DNN_DTYPE_INT16  = 5,
    DNN_DTYPE_FP8    = 6,
    DNN_DTYPE_COUNT
} dnn_dtype_t;

/* --- Opcode Categories --- */
typedef enum {
    /* Control */
    DNN_OP_NOP       = 0x00,
    DNN_OP_HALT      = 0x01,
    DNN_OP_SYNC      = 0x02,
    DNN_OP_BARRIER   = 0x03,

    /* Scalar ALU */
    DNN_OP_ADD       = 0x10,
    DNN_OP_SUB       = 0x11,
    DNN_OP_MUL       = 0x12,
    DNN_OP_DIV       = 0x13,
    DNN_OP_MAX       = 0x14,
    DNN_OP_MIN       = 0x15,
    DNN_OP_RELU      = 0x16,
    DNN_OP_LEAKY_RELU = 0x17,
    DNN_OP_SIGMOID   = 0x18,
    DNN_OP_TANH      = 0x19,
    DNN_OP_EXP       = 0x1A,
    DNN_OP_SQRT      = 0x1B,
    DNN_OP_ABS       = 0x1C,

    /* Vector / SIMD */
    DNN_OP_VADD      = 0x20,
    DNN_OP_VMUL      = 0x21,
    DNN_OP_VMADD     = 0x22,
    DNN_OP_VDOT      = 0x23,
    DNN_OP_VRELU     = 0x24,
    DNN_OP_VSOFTMAX  = 0x25,
    DNN_OP_VLAYERNORM = 0x26,
    DNN_OP_VBATCHNORM = 0x27,
    DNN_OP_VREDUCE_SUM = 0x28,
    DNN_OP_VREDUCE_MAX = 0x29,

    /* Matrix operations */
    DNN_OP_MATMUL    = 0x30,
    DNN_OP_MATMUL_BIAS = 0x31,
    DNN_OP_CONV2D    = 0x32,
    DNN_OP_DEPTHWISE_CONV = 0x33,
    DNN_OP_MAXPOOL   = 0x34,
    DNN_OP_AVGPOOL   = 0x35,
    DNN_OP_TRANSPOSE = 0x36,
    DNN_OP_IM2COL    = 0x37,

    /* Memory / Data Movement */
    DNN_OP_LD        = 0x40,
    DNN_OP_ST        = 0x41,
    DNN_OP_LD_MATRIX = 0x42,
    DNN_OP_ST_MATRIX = 0x43,
    DNN_OP_PREFETCH  = 0x44,
    DNN_OP_DMA_COPY  = 0x45,
    DNN_OP_DMA_WAIT  = 0x46,
    DNN_OP_ZERO      = 0x47,

    /* Control Flow */
    DNN_OP_JMP       = 0x50,
    DNN_OP_JZ        = 0x51,
    DNN_OP_JNZ       = 0x52,
    DNN_OP_LOOP      = 0x53,
    DNN_OP_CALL      = 0x54,
    DNN_OP_RET       = 0x55,

    /* Configuration */
    DNN_OP_SET_DTYPE = 0x60,
    DNN_OP_SET_DATAFLOW = 0x61,
    DNN_OP_SET_TILE  = 0x62,
    DNN_OP_SET_ARRAY = 0x63,
    DNN_OP_SET_PRECISION = 0x64,

    /* Sparse */
    DNN_OP_SPARSE_MATMUL = 0x70,
    DNN_OP_DENSE_TO_SPARSE = 0x71,
    DNN_OP_SPARSE_TO_DENSE = 0x72,
    DNN_OP_SPARSITY_MASK = 0x73,

    DNN_OP_COUNT
} dnn_opcode_t;

/* --- Operand Types --- */
typedef enum {
    DNN_OPERAND_NONE    = 0,
    DNN_OPERAND_REG     = 1,
    DNN_OPERAND_IMM     = 2,
    DNN_OPERAND_MEM     = 3,
    DNN_OPERAND_LABEL   = 4,
    DNN_OPERAND_MATRIX  = 5,
    DNN_OPERAND_VECTOR  = 6
} dnn_operand_type_t;

/* --- Dataflow Types --- */
typedef enum {
    DNN_DATAFLOW_WEIGHT_STATIONARY  = 0,
    DNN_DATAFLOW_OUTPUT_STATIONARY  = 1,
    DNN_DATAFLOW_ROW_STATIONARY     = 2,
    DNN_DATAFLOW_NO_LOCAL_REUSE    = 3,
    DNN_DATAFLOW_INPUT_STATIONARY  = 4
} dnn_dataflow_t;

/* --- Register File --- */
typedef struct {
    char        name[DNN_ISA_REG_NAME_LEN];
    uint32_t    index;
    dnn_dtype_t dtype;
    uint32_t    vector_len;
    bool        is_matrix;
    uint32_t    rows;
    uint32_t    cols;
    bool        in_use;
    uint32_t    last_access_cycle;
} dnn_register_t;

/* --- Operand --- */
typedef struct {
    dnn_operand_type_t  type;
    union {
        int32_t         reg_index;
        int64_t         imm_value;
        uint64_t        mem_addr;
        uint32_t        label_id;
        struct {
            int32_t     mat_reg;
            uint32_t    row_offset;
            uint32_t    col_offset;
        } matrix;
        struct {
            int32_t     vec_reg;
            uint32_t    element_offset;
            uint32_t    element_count;
        } vector;
    } data;
} dnn_operand_t;

/* --- Complete Instruction --- */
typedef struct {
    dnn_opcode_t    opcode;
    dnn_dtype_t     dtype;
    uint32_t        num_operands;
    dnn_operand_t   operands[DNN_ISA_MAX_OPERANDS];
    uint32_t        latency_cycles;
    uint32_t        throughput_cycles;
    char            label[DNN_ISA_LABEL_MAX];
    uint32_t        pc;
    bool            is_branch;
    uint32_t        branch_target;
} dnn_instruction_t;

/* --- ISA Program --- */
typedef struct {
    dnn_instruction_t   instructions[DNN_ISA_PROGRAM_SIZE];
    uint32_t            instr_count;
    dnn_register_t      reg_file[DNN_ISA_REG_COUNT];
    uint32_t            reg_count;
    char                name[128];
    uint32_t            version;
    uint32_t            total_cycles;
    uint32_t            total_energy_pj;
} dnn_program_t;

/* --- ISA Statistics --- */
typedef struct {
    uint32_t    total_instr;
    uint32_t    scalar_ops;
    uint32_t    vector_ops;
    uint32_t    matrix_ops;
    uint32_t    memory_ops;
    uint32_t    control_ops;
    uint32_t    config_ops;
    uint32_t    sparse_ops;
    uint32_t    cycles;
    double      ops_per_byte;
    double      compute_utilization;
    double      arithmetic_intensity;
} dnn_isa_stats_t;

/* --- Execution Context --- */
typedef struct {
    dnn_program_t  *program;
    uint32_t        pc;
    uint32_t        cycle;
    bool            running;
    bool            halted;
    uint32_t        stall_cycles;
    uint32_t        stall_reason;
    dnn_instruction_t fetch_reg;
    dnn_instruction_t decode_reg;
    bool            pipeline_stall;
    uint64_t        total_ops;
    uint64_t        total_bytes;
    uint64_t        total_flops;
    uint32_t        branch_count;
    uint32_t        mispredict_count;
} dnn_exec_context_t;

/* ================================================================
 * API
 * ================================================================ */

/* Program management */
void          dnn_program_init(dnn_program_t *prog, const char *name);
int           dnn_program_add_instr(dnn_program_t *prog, dnn_opcode_t op, dnn_dtype_t dt);
void          dnn_program_reset(dnn_program_t *prog);
dnn_instruction_t *dnn_program_get_instr(dnn_program_t *prog, uint32_t idx);

/* Operand construction helpers */
dnn_operand_t dnn_operand_reg(int32_t reg_idx);
dnn_operand_t dnn_operand_imm(int64_t value);
dnn_operand_t dnn_operand_mem(uint64_t addr);
dnn_operand_t dnn_operand_label(uint32_t label_id);
dnn_operand_t dnn_operand_matrix(int32_t reg, uint32_t row, uint32_t col);
dnn_operand_t dnn_operand_vector(int32_t reg, uint32_t offset, uint32_t count);

/* Instruction construction (high-level API) */
void          dnn_emit_scalar_op(dnn_program_t *prog, dnn_opcode_t op, dnn_dtype_t dt,
                                 int32_t dst, int32_t src1, int32_t src2);
void          dnn_emit_vector_op(dnn_program_t *prog, dnn_opcode_t op, dnn_dtype_t dt,
                                 int32_t dst, int32_t src1, int32_t src2, uint32_t len);
void          dnn_emit_load(dnn_program_t *prog, dnn_dtype_t dt, int32_t dst, uint64_t addr, uint32_t size);
void          dnn_emit_store(dnn_program_t *prog, dnn_dtype_t dt, int32_t src, uint64_t addr, uint32_t size);
void          dnn_emit_matmul(dnn_program_t *prog, dnn_dtype_t dt,
                              int32_t dst, int32_t lhs, int32_t rhs,
                              uint32_t M, uint32_t N, uint32_t K);
void          dnn_emit_conv2d(dnn_program_t *prog, dnn_dtype_t dt,
                              int32_t dst, int32_t input, int32_t weight,
                              uint32_t H, uint32_t W, uint32_t C_in, uint32_t C_out,
                              uint32_t KH, uint32_t KW, uint32_t stride);
void          dnn_emit_loop(dnn_program_t *prog, int32_t counter_reg, uint32_t body_start);
void          dnn_emit_jmp(dnn_program_t *prog, uint32_t target);
void          dnn_emit_halt(dnn_program_t *prog);

/* ISA Execution */
void          dnn_exec_init(dnn_exec_context_t *ctx, dnn_program_t *prog);
int           dnn_exec_step(dnn_exec_context_t *ctx);
void          dnn_exec_run(dnn_exec_context_t *ctx, uint32_t max_cycles);
void          dnn_exec_reset(dnn_exec_context_t *ctx);

/* ISA Analysis & Validation */
void          dnn_isa_collect_stats(const dnn_program_t *prog, dnn_isa_stats_t *stats);
double        dnn_isa_compute_arithmetic_intensity(const dnn_program_t *prog);
bool          dnn_isa_validate(const dnn_program_t *prog);
void          dnn_isa_print_program(const dnn_program_t *prog);
void          dnn_isa_print_stats(const dnn_isa_stats_t *stats);
const char   *dnn_opcode_name(dnn_opcode_t op);
const char   *dnn_dtype_name(dnn_dtype_t dt);
const char   *dnn_dataflow_name(dnn_dataflow_t df);
int           dnn_dtype_bytes(dnn_dtype_t dt);

/* Register allocation */
int32_t       dnn_alloc_register(dnn_program_t *prog, const char *name, dnn_dtype_t dt);
void          dnn_free_register(dnn_program_t *prog, int32_t reg);

/* Binary encoding / decoding (for RTL generation) */
void          dnn_encode_instruction(const dnn_instruction_t *instr, uint8_t *bytes, size_t max_bytes);
bool          dnn_decode_instruction(const uint8_t *bytes, size_t max_bytes, dnn_instruction_t *instr);

#endif /* DNN_ISA_H */