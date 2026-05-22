#ifndef RISCV_CORE_H
#define RISCV_CORE_H

#include <stdint.h>
#include <stdbool.h>

#define NUM_REGS      32
#define REG_ZERO       0
#define MEM_SIZE    4096
#define RESET_PC      0x00000000

typedef enum {
    STAGE_IF = 0,
    STAGE_ID,
    STAGE_EX,
    STAGE_MEM,
    STAGE_WB,
    STAGE_COUNT
} PipelineStage;

typedef enum {
    TYPE_R = 0,
    TYPE_I,
    TYPE_S,
    TYPE_B,
    TYPE_U,
    TYPE_J,
    TYPE_UNKNOWN
} InstType;

typedef enum {
    OPCODE_LUI      = 0b0110111,
    OPCODE_AUIPC    = 0b0010111,
    OPCODE_JAL      = 0b1101111,
    OPCODE_JALR     = 0b1100111,
    OPCODE_BRANCH   = 0b1100011,
    OPCODE_LOAD     = 0b0000011,
    OPCODE_STORE    = 0b0100011,
    OPCODE_ALUI     = 0b0010011,
    OPCODE_ALU      = 0b0110011,
    OPCODE_SYSTEM   = 0b1110011
} Opcode;

typedef enum {
    ALU_ADD  = 0,  ALU_SUB  = 1,
    ALU_SLL  = 2,  ALU_SLT  = 3,
    ALU_SLTU = 4,  ALU_XOR  = 5,
    ALU_SRL  = 6,  ALU_SRA  = 7,
    ALU_OR   = 8,  ALU_AND  = 9,
    ALU_PASS_B = 10
} AluOp;

typedef enum {
    FORWARD_NONE = 0,
    FORWARD_EX_MEM,
    FORWARD_MEM_WB
} ForwardSrc;

typedef struct {
    bool     valid;
    uint32_t pc;
    uint32_t inst;
} IF_ID_Reg;

typedef struct {
    bool     valid;
    uint32_t pc;
    uint32_t inst;
    uint32_t rs1_data;
    uint32_t rs2_data;
    uint32_t imm;
    uint8_t  rs1;
    uint8_t  rs2;
    uint8_t  rd;
    Opcode   opcode;
    InstType type;
    uint8_t  funct3;
    uint8_t  funct7;
    AluOp    alu_op;
    bool     is_branch;
    bool     is_jal;
    bool     is_jalr;
    bool     is_load;
    bool     is_store;
    bool     is_ecall;
    bool     is_ebreak;
    bool     is_mret;
    bool     is_auipc;
    bool     is_lui;
    bool     wb_en;
    uint32_t predicted_pc;
} ID_EX_Reg;

typedef struct {
    bool     valid;
    uint32_t pc;
    uint32_t alu_result;
    uint32_t store_data;
    uint8_t  rd;
    uint8_t  funct3;
    bool     is_load;
    bool     is_store;
    bool     is_jal;
    bool     is_jalr;
    bool     is_ecall;
    bool     is_ebreak;
    bool     is_mret;
    bool     wb_en;
    uint32_t pc_plus_4;
} EX_MEM_Reg;

typedef struct {
    bool     valid;
    uint32_t pc;
    uint32_t alu_result;
    uint32_t mem_data;
    uint8_t  rd;
    uint8_t  funct3;
    bool     is_load;
    bool     is_jal;
    bool     is_jalr;
    bool     is_ecall;
    bool     is_ebreak;
    bool     is_mret;
    bool     is_auipc;
    bool     is_lui;
    bool     wb_en;
    uint32_t pc_plus_4;
} MEM_WB_Reg;

typedef struct {
    uint32_t regs[NUM_REGS];
} RegisterFile;

typedef struct {
    uint8_t data[MEM_SIZE];
} Memory;

typedef struct {
    uint32_t pc;
    RegisterFile rf;
    Memory     imem;
    Memory     dmem;

    IF_ID_Reg  if_id;
    ID_EX_Reg  id_ex;
    EX_MEM_Reg ex_mem;
    MEM_WB_Reg mem_wb;

    bool   stall_if;
    bool   stall_id;
    bool   flush_ex;

    bool   branch_mispredict;
    uint32_t correct_pc;

    uint32_t mstatus;
    uint32_t mepc;
    uint32_t mtvec;
    uint32_t mcause;

    uint32_t cycle_count;
    uint32_t inst_count;
    bool     timer_interrupt;
    uint64_t mtime;
    uint64_t mtimecmp;
} RiscvCore;

void  core_init(RiscvCore *core);
void  core_reset(RiscvCore *core);
void  core_tick(RiscvCore *core);
void  core_step(RiscvCore *core, uint32_t steps);

void  core_load_program(RiscvCore *core, const uint32_t *program, uint32_t len);

void  regfile_init(RegisterFile *rf);
void  regfile_write(RegisterFile *rf, uint8_t rd, uint32_t val);
uint32_t regfile_read(const RegisterFile *rf, uint8_t rs);

void  mem_init(Memory *mem);
void  mem_write_byte(Memory *mem, uint32_t addr, uint8_t val);
void  mem_write_half(Memory *mem, uint32_t addr, uint16_t val);
void  mem_write_word(Memory *mem, uint32_t addr, uint32_t val);
uint8_t  mem_read_byte(const Memory *mem, uint32_t addr);
uint16_t mem_read_half(const Memory *mem, uint32_t addr);
uint32_t mem_read_word(const Memory *mem, uint32_t addr);

uint8_t  inst_opcode(uint32_t inst);
uint8_t  inst_rd(uint32_t inst);
uint8_t  inst_rs1(uint32_t inst);
uint8_t  inst_rs2(uint32_t inst);
uint8_t  inst_funct3(uint32_t inst);
uint8_t  inst_funct7(uint32_t inst);

InstType decode_type(uint8_t opcode);
AluOp    decode_alu_op(uint8_t opcode, uint8_t funct3, uint8_t funct7);

int32_t  imm_i(uint32_t inst);
int32_t  imm_s(uint32_t inst);
int32_t  imm_b(uint32_t inst);
int32_t  imm_u(uint32_t inst);
int32_t  imm_j(uint32_t inst);

uint32_t alu_compute(AluOp op, uint32_t a, uint32_t b);
bool     branch_taken(uint8_t funct3, uint32_t rs1, uint32_t rs2);

void pipeline_if(RiscvCore *core);
void pipeline_id(RiscvCore *core);
void pipeline_ex(RiscvCore *core);
void pipeline_mem(RiscvCore *core);
void pipeline_wb(RiscvCore *core);

void csr_write(RiscvCore *core, uint32_t addr, uint32_t val);
uint32_t csr_read(const RiscvCore *core, uint32_t addr);
void csr_take_trap(RiscvCore *core, uint32_t cause, uint32_t epc);
void csr_mret(RiscvCore *core);

void core_dump_regs(const RiscvCore *core);
void core_dump_pipeline(const RiscvCore *core);
void core_dump_csr(const RiscvCore *core);

#endif
