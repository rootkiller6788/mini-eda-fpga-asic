#ifndef RISCV_DECODE_H
#define RISCV_DECODE_H

#include <stdint.h>
#include <stdbool.h>

#define OP_LUI      0b0110111
#define OP_AUIPC    0b0010111
#define OP_JAL      0b1101111
#define OP_JALR     0b1100111
#define OP_BRANCH   0b1100011
#define OP_LOAD     0b0000011
#define OP_STORE    0b0100011
#define OP_ALUI     0b0010011
#define OP_ALU      0b0110011
#define OP_SYSTEM   0b1110011

#define F3_BEQ  0x0
#define F3_BNE  0x1
#define F3_BLT  0x4
#define F3_BGE  0x5
#define F3_BLTU 0x6
#define F3_BGEU 0x7

#define F3_LB   0x0
#define F3_LH   0x1
#define F3_LW   0x2
#define F3_LBU  0x4
#define F3_LHU  0x5

#define F3_SB   0x0
#define F3_SH   0x1
#define F3_SW   0x2

#define F3_ADDI  0x0
#define F3_SLTI  0x2
#define F3_SLTIU 0x3
#define F3_XORI  0x4
#define F3_ORI   0x6
#define F3_ANDI  0x7
#define F3_SLLI  0x1
#define F3_SRLI_SRAI 0x5

#define F3_ADD_SUB 0x0
#define F3_SLL     0x1
#define F3_SLT     0x2
#define F3_SLTU    0x3
#define F3_XOR     0x4
#define F3_SRL_SRA 0x5
#define F3_OR      0x6
#define F3_AND     0x7

#define F3_ECALL_EBREAK  0x0
#define F3_MRET  0x0

#define F7_ADD  0x00
#define F7_SUB  0x20
#define F7_SRA  0x20
#define F7_SRL  0x00

#define F12_ECALL  0x000
#define F12_EBREAK 0x001
#define F12_MRET   0x302

typedef enum {
    INSTR_ADD,   INSTR_SUB,   INSTR_SLL,   INSTR_SLT,
    INSTR_SLTU,  INSTR_XOR,   INSTR_SRL,   INSTR_SRA,
    INSTR_OR,    INSTR_AND,   INSTR_ADDI,  INSTR_SLTI,
    INSTR_SLTIU, INSTR_XORI,  INSTR_ORI,   INSTR_ANDI,
    INSTR_SLLI,  INSTR_SRLI,  INSTR_SRAI,  INSTR_LB,
    INSTR_LH,    INSTR_LW,    INSTR_LBU,   INSTR_LHU,
    INSTR_SB,    INSTR_SH,    INSTR_SW,    INSTR_BEQ,
    INSTR_BNE,   INSTR_BLT,   INSTR_BGE,   INSTR_BLTU,
    INSTR_BGEU,  INSTR_JAL,   INSTR_JALR,  INSTR_LUI,
    INSTR_AUIPC, INSTR_ECALL, INSTR_EBREAK, INSTR_MRET,
    INSTR_UNKNOWN
} RiscvInstr;

typedef enum {
    TYPE_R = 0,
    TYPE_I,
    TYPE_S,
    TYPE_B,
    TYPE_U,
    TYPE_J
} InstrType;

typedef enum {
    ALU_ADD  = 0,
    ALU_SUB  = 1,
    ALU_SLL  = 2,
    ALU_SLT  = 3,
    ALU_SLTU = 4,
    ALU_XOR  = 5,
    ALU_SRL  = 6,
    ALU_SRA  = 7,
    ALU_OR   = 8,
    ALU_AND  = 9,
    ALU_COPY  = 10,
    ALU_ADD_PC = 11
} ALUOper;

typedef struct {
    uint32_t    raw;
    RiscvInstr  instr;
    InstrType   type;
    uint8_t     opcode;
    uint8_t     rd;
    uint8_t     rs1;
    uint8_t     rs2;
    uint8_t     funct3;
    uint8_t     funct7;
    int32_t     imm;
    ALUOper     alu_op;
    bool        is_branch;
    bool        is_jump;
    bool        is_jalr;
    bool        is_load;
    bool        is_store;
    bool        is_alu_r;
    bool        is_alu_i;
    bool        is_system;
    bool        has_rd;
    bool        has_rs1;
    bool        has_rs2;
    bool        wb_enable;
    const char *mnemonic;
    const char *op_format;
} DecodedInst;

DecodedInst decode_instruction(uint32_t inst);

RiscvInstr decode_instr_type(const DecodedInst *d);
ALUOper    decode_alu_operation(const DecodedInst *d);
InstrType  decode_format(uint8_t opcode);

uint8_t  decode_rd(uint32_t inst);
uint8_t  decode_rs1(uint32_t inst);
uint8_t  decode_rs2(uint32_t inst);
uint8_t  decode_funct3(uint32_t inst);
uint8_t  decode_funct7(uint32_t inst);
uint8_t  decode_opcode(uint32_t inst);

int32_t  imm_generate_i(uint32_t inst);
int32_t  imm_generate_s(uint32_t inst);
int32_t  imm_generate_b(uint32_t inst);
int32_t  imm_generate_u(uint32_t inst);
int32_t  imm_generate_j(uint32_t inst);

bool     is_branch_taken(uint8_t funct3, uint32_t rs1_val, uint32_t rs2_val);
uint32_t alu_execute(ALUOper op, uint32_t a, uint32_t b);
uint32_t load_mem(uint8_t funct3, uint32_t addr, const uint8_t *mem, uint32_t mem_size);
void     store_mem(uint8_t funct3, uint32_t addr, uint32_t data, uint8_t *mem, uint32_t mem_size);

bool     is_compressed(uint16_t half);
uint32_t decompress(uint16_t half);

void     print_instruction(const DecodedInst *d, uint32_t pc);
const char *instr_name(RiscvInstr i);

#endif
