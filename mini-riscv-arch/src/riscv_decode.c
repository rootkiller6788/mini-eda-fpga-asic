#include <stdio.h>
#include <string.h>
#include "riscv_decode.h"

static const char *mnemonic_table[] = {
    [INSTR_ADD]    = "add",    [INSTR_SUB]    = "sub",
    [INSTR_SLL]    = "sll",    [INSTR_SLT]    = "slt",
    [INSTR_SLTU]   = "sltu",   [INSTR_XOR]    = "xor",
    [INSTR_SRL]    = "srl",    [INSTR_SRA]    = "sra",
    [INSTR_OR]     = "or",     [INSTR_AND]    = "and",
    [INSTR_ADDI]   = "addi",   [INSTR_SLTI]   = "slti",
    [INSTR_SLTIU]  = "sltiu",  [INSTR_XORI]   = "xori",
    [INSTR_ORI]    = "ori",    [INSTR_ANDI]   = "andi",
    [INSTR_SLLI]   = "slli",   [INSTR_SRLI]   = "srli",
    [INSTR_SRAI]   = "srai",   [INSTR_LB]     = "lb",
    [INSTR_LH]     = "lh",     [INSTR_LW]     = "lw",
    [INSTR_LBU]    = "lbu",    [INSTR_LHU]    = "lhu",
    [INSTR_SB]     = "sb",     [INSTR_SH]     = "sh",
    [INSTR_SW]     = "sw",     [INSTR_BEQ]    = "beq",
    [INSTR_BNE]    = "bne",    [INSTR_BLT]    = "blt",
    [INSTR_BGE]    = "bge",    [INSTR_BLTU]   = "bltu",
    [INSTR_BGEU]   = "bgeu",   [INSTR_JAL]    = "jal",
    [INSTR_JALR]   = "jalr",   [INSTR_LUI]    = "lui",
    [INSTR_AUIPC]  = "auipc",  [INSTR_ECALL]  = "ecall",
    [INSTR_EBREAK] = "ebreak", [INSTR_MRET]   = "mret",
    [INSTR_UNKNOWN]= "unknown"
};

const char *instr_name(RiscvInstr i) {
    if (i <= INSTR_UNKNOWN) return mnemonic_table[i];
    return "invalid";
}

uint8_t decode_opcode(uint32_t inst)    { return inst & 0x7F; }
uint8_t decode_rd(uint32_t inst)        { return (inst >> 7) & 0x1F; }
uint8_t decode_funct3(uint32_t inst)    { return (inst >> 12) & 0x7; }
uint8_t decode_rs1(uint32_t inst)       { return (inst >> 15) & 0x1F; }
uint8_t decode_rs2(uint32_t inst)       { return (inst >> 20) & 0x1F; }
uint8_t decode_funct7(uint32_t inst)    { return (inst >> 25) & 0x7F; }

InstrType decode_format(uint8_t opcode) {
    switch (opcode) {
    case OP_ALU:   return TYPE_R;
    case OP_ALUI:
    case OP_LOAD:
    case OP_JALR:  return TYPE_I;
    case OP_STORE: return TYPE_S;
    case OP_BRANCH:return TYPE_B;
    case OP_LUI:
    case OP_AUIPC: return TYPE_U;
    case OP_JAL:   return TYPE_J;
    default:       return TYPE_R;
    }
}

RiscvInstr decode_instr_type(const DecodedInst *d) {
    if (!d) return INSTR_UNKNOWN;

    switch (d->opcode) {
    case OP_ALU:
        if (d->funct3 == F3_ADD_SUB)
            return (d->funct7 == F7_SUB) ? INSTR_SUB : INSTR_ADD;
        if (d->funct3 == F3_SLL)     return INSTR_SLL;
        if (d->funct3 == F3_SLT)     return INSTR_SLT;
        if (d->funct3 == F3_SLTU)    return INSTR_SLTU;
        if (d->funct3 == F3_XOR)     return INSTR_XOR;
        if (d->funct3 == F3_SRL_SRA) return (d->funct7 == F7_SRA) ? INSTR_SRA : INSTR_SRL;
        if (d->funct3 == F3_OR)      return INSTR_OR;
        if (d->funct3 == F3_AND)     return INSTR_AND;
        return INSTR_UNKNOWN;

    case OP_ALUI:
        if (d->funct3 == F3_ADDI)   return INSTR_ADDI;
        if (d->funct3 == F3_SLTI)   return INSTR_SLTI;
        if (d->funct3 == F3_SLTIU)  return INSTR_SLTIU;
        if (d->funct3 == F3_XORI)   return INSTR_XORI;
        if (d->funct3 == F3_ORI)    return INSTR_ORI;
        if (d->funct3 == F3_ANDI)   return INSTR_ANDI;
        if (d->funct3 == F3_SLLI)   return INSTR_SLLI;
        if (d->funct3 == F3_SRLI_SRAI) return (d->funct7 == F7_SRA) ? INSTR_SRAI : INSTR_SRLI;
        return INSTR_UNKNOWN;

    case OP_LOAD:
        if (d->funct3 == F3_LB)   return INSTR_LB;
        if (d->funct3 == F3_LH)   return INSTR_LH;
        if (d->funct3 == F3_LW)   return INSTR_LW;
        if (d->funct3 == F3_LBU)  return INSTR_LBU;
        if (d->funct3 == F3_LHU)  return INSTR_LHU;
        return INSTR_UNKNOWN;

    case OP_STORE:
        if (d->funct3 == F3_SB) return INSTR_SB;
        if (d->funct3 == F3_SH) return INSTR_SH;
        if (d->funct3 == F3_SW) return INSTR_SW;
        return INSTR_UNKNOWN;

    case OP_BRANCH:
        if (d->funct3 == F3_BEQ)  return INSTR_BEQ;
        if (d->funct3 == F3_BNE)  return INSTR_BNE;
        if (d->funct3 == F3_BLT)  return INSTR_BLT;
        if (d->funct3 == F3_BGE)  return INSTR_BGE;
        if (d->funct3 == F3_BLTU) return INSTR_BLTU;
        if (d->funct3 == F3_BGEU) return INSTR_BGEU;
        return INSTR_UNKNOWN;

    case OP_JAL:   return INSTR_JAL;
    case OP_JALR:  return INSTR_JALR;
    case OP_LUI:   return INSTR_LUI;
    case OP_AUIPC: return INSTR_AUIPC;

    case OP_SYSTEM:
        if (d->funct3 == F3_ECALL_EBREAK) {
            if (d->imm == F12_ECALL)  return INSTR_ECALL;
            if (d->imm == F12_EBREAK) return INSTR_EBREAK;
            if (d->imm == F12_MRET)   return INSTR_MRET;
        }
        return INSTR_UNKNOWN;

    default: return INSTR_UNKNOWN;
    }
}

ALUOper decode_alu_operation(const DecodedInst *d) {
    if (!d) return ALU_ADD;

    if (d->opcode == OP_ALU) {
        switch (d->funct3) {
        case F3_ADD_SUB: return (d->funct7 == F7_SUB) ? ALU_SUB : ALU_ADD;
        case F3_SLL:     return ALU_SLL;
        case F3_SLT:     return ALU_SLT;
        case F3_SLTU:    return ALU_SLTU;
        case F3_XOR:     return ALU_XOR;
        case F3_SRL_SRA: return (d->funct7 == F7_SRA) ? ALU_SRA : ALU_SRL;
        case F3_OR:      return ALU_OR;
        case F3_AND:     return ALU_AND;
        default:         return ALU_ADD;
        }
    }
    if (d->opcode == OP_ALUI) {
        if (d->funct3 == F3_SLLI) return ALU_SLL;
        if (d->funct3 == F3_SRLI_SRAI)
            return (d->funct7 == F7_SRA) ? ALU_SRA : ALU_SRL;
        return ALU_ADD;
    }
    if (d->opcode == OP_LUI)    return ALU_COPY;
    if (d->opcode == OP_AUIPC)  return ALU_ADD_PC;
    if (d->opcode == OP_LOAD || d->opcode == OP_STORE)
        return ALU_ADD;
    if (d->opcode == OP_BRANCH) return ALU_SUB;
    return ALU_ADD;
}

int32_t imm_generate_i(uint32_t inst) {
    return (int32_t)(inst) >> 20;
}

int32_t imm_generate_s(uint32_t inst) {
    int32_t imm = (int32_t)((inst >> 7) & 0x1F);
    imm |= ((int32_t)((inst >> 25) & 0x7F)) << 5;
    return (imm << 20) >> 20;
}

int32_t imm_generate_b(uint32_t inst) {
    int32_t imm = (((int32_t)(inst >> 8)) & 0xF) << 1;
    imm |= (((int32_t)(inst >> 25)) & 0x3F) << 5;
    imm |= (((int32_t)(inst >> 7)) & 0x1) << 11;
    imm |= ((((int32_t)inst) >> 31) & 0x1) << 12;
    return (imm << 19) >> 19;
}

int32_t imm_generate_u(uint32_t inst) {
    return (int32_t)(inst & 0xFFFFF000);
}

int32_t imm_generate_j(uint32_t inst) {
    int32_t imm = (((int32_t)(inst >> 21)) & 0x3FF) << 1;
    imm |= ((((int32_t)(inst >> 20)) & 0x1)) << 11;
    imm |= ((((int32_t)(inst >> 12)) & 0xFF)) << 12;
    imm |= ((((int32_t)inst) >> 31) & 0x1) << 20;
    return (imm << 11) >> 11;
}

uint32_t alu_execute(ALUOper op, uint32_t a, uint32_t b) {
    switch (op) {
    case ALU_ADD:    return a + b;
    case ALU_SUB:    return a - b;
    case ALU_SLL:    return a << (b & 0x1F);
    case ALU_SLT:    return ((int32_t)a < (int32_t)b) ? 1 : 0;
    case ALU_SLTU:   return (a < b) ? 1 : 0;
    case ALU_XOR:    return a ^ b;
    case ALU_SRL:    return a >> (b & 0x1F);
    case ALU_SRA:    return (uint32_t)((int32_t)a >> (b & 0x1F));
    case ALU_OR:     return a | b;
    case ALU_AND:    return a & b;
    case ALU_COPY:   return b;
    case ALU_ADD_PC: return a + b;
    default:         return 0;
    }
}

bool is_branch_taken(uint8_t funct3, uint32_t rs1_val, uint32_t rs2_val) {
    switch (funct3) {
    case F3_BEQ:  return rs1_val == rs2_val;
    case F3_BNE:  return rs1_val != rs2_val;
    case F3_BLT:  return (int32_t)rs1_val < (int32_t)rs2_val;
    case F3_BGE:  return (int32_t)rs1_val >= (int32_t)rs2_val;
    case F3_BLTU: return rs1_val < rs2_val;
    case F3_BGEU: return rs1_val >= rs2_val;
    default:      return false;
    }
}

uint32_t load_mem(uint8_t funct3, uint32_t addr, const uint8_t *mem,
                  uint32_t mem_size) {
    if (addr >= mem_size) return 0;

    switch (funct3) {
    case F3_LB:  return (int32_t)(int8_t)mem[addr];
    case F3_LH:  return (int32_t)(int16_t)(mem[addr] | ((uint16_t)mem[addr + 1] << 8));
    case F3_LW:  return (uint32_t)mem[addr]
                    | ((uint32_t)mem[addr + 1] << 8)
                    | ((uint32_t)mem[addr + 2] << 16)
                    | ((uint32_t)mem[addr + 3] << 24);
    case F3_LBU: return (uint32_t)mem[addr];
    case F3_LHU: return (uint32_t)(mem[addr] | ((uint16_t)mem[addr + 1] << 8));
    default:     return 0;
    }
}

void store_mem(uint8_t funct3, uint32_t addr, uint32_t data, uint8_t *mem,
               uint32_t mem_size) {
    if (addr >= mem_size) return;

    switch (funct3) {
    case F3_SB:
        mem[addr] = (uint8_t)(data & 0xFF);
        break;
    case F3_SH:
        mem[addr]     = (uint8_t)(data & 0xFF);
        mem[addr + 1] = (uint8_t)((data >> 8) & 0xFF);
        break;
    case F3_SW:
        mem[addr]     = (uint8_t)(data & 0xFF);
        mem[addr + 1] = (uint8_t)((data >> 8) & 0xFF);
        mem[addr + 2] = (uint8_t)((data >> 16) & 0xFF);
        mem[addr + 3] = (uint8_t)((data >> 24) & 0xFF);
        break;
    default: break;
    }
}

DecodedInst decode_instruction(uint32_t inst) {
    DecodedInst d;
    memset(&d, 0, sizeof(DecodedInst));
    d.raw    = inst;
    d.opcode = decode_opcode(inst);
    d.rd     = decode_rd(inst);
    d.rs1    = decode_rs1(inst);
    d.rs2    = decode_rs2(inst);
    d.funct3 = decode_funct3(inst);
    d.funct7 = decode_funct7(inst);
    d.type   = decode_format(d.opcode);

    switch (d.type) {
    case TYPE_I: d.imm = imm_generate_i(inst); break;
    case TYPE_S: d.imm = imm_generate_s(inst); break;
    case TYPE_B: d.imm = imm_generate_b(inst); break;
    case TYPE_U: d.imm = imm_generate_u(inst); break;
    case TYPE_J: d.imm = imm_generate_j(inst); break;
    default:     d.imm = 0; break;
    }

    d.is_branch = (d.opcode == OP_BRANCH);
    d.is_jump   = (d.opcode == OP_JAL);
    d.is_jalr   = (d.opcode == OP_JALR);
    d.is_load   = (d.opcode == OP_LOAD);
    d.is_store  = (d.opcode == OP_STORE);
    d.is_alu_r  = (d.opcode == OP_ALU);
    d.is_alu_i  = (d.opcode == OP_ALUI);
    d.is_system = (d.opcode == OP_SYSTEM);
    d.has_rd    = !d.is_branch && !d.is_store && (d.opcode != OP_SYSTEM);
    d.has_rs1   = (d.type == TYPE_R || d.type == TYPE_I
                || d.type == TYPE_S || d.type == TYPE_B);
    d.has_rs2   = (d.type == TYPE_R || d.type == TYPE_S || d.type == TYPE_B);
    d.wb_enable = d.has_rd;

    d.instr = decode_instr_type(&d);
    d.mnemonic = instr_name(d.instr);
    d.alu_op = decode_alu_operation(&d);

    return d;
}

bool is_compressed(uint16_t half) {
    return (half & 0x3) != 0x3;
}

uint32_t decompress(uint16_t half) {
    uint8_t op = half & 0x3;
    uint8_t funct3 = (half >> 13) & 0x7;

    if (op == 0x0 && funct3 == 0x0) {
        uint8_t rd  = (half >> 7) & 0x7;
        uint8_t rs1 = (half >> 7) & 0x7;
        uint32_t nzimm = ((half >> 5) & 0x1) << 5
                       | ((half >> 2) & 0x7) << 1
                       | ((half >> 12) & 0x1) << 6;
        return (nzimm << 20)
             | (rs1 << 15)
             | (0x0 << 12)
             | ((rd + 8) << 7)
             | OP_ADDI;
    }

    if (op == 0x1) {
        uint8_t rd  = (half >> 7) & 0x7;
        uint32_t imm = ((half >> 2) & 0x1F) << 1
                     | ((half >> 12) & 0x1) << 5;
        return (imm << 20)
             | (0x0 << 15)
             | (0x0 << 12)
             | ((rd + 8) << 7)
             | OP_JALR;
    }

    if (op == 0x2) {
        uint8_t rd = (half >> 7) & 0x7;
        uint8_t rs1 = (half >> 7) & 0x7;
        uint32_t offset = ((half >> 5) & 0x1) << 6
                        | ((half >> 10) & 0x7) << 3
                        | ((half >> 6) & 0x1) << 2;
        return (offset << 20)
             | (rs1 << 15)
             | (0x2 << 12)
             | ((rd + 8) << 7)
             | OP_LOAD;
    }

    (void)funct3;
    return 0;
}

void print_instruction(const DecodedInst *d, uint32_t pc) {
    printf("0x%08X: ", pc);
    printf("%-8s ", d->mnemonic);

    switch (d->type) {
    case TYPE_R:
        printf("x%d, x%d, x%d", d->rd, d->rs1, d->rs2);
        break;
    case TYPE_I:
        if (d->is_load)
            printf("x%d, %d(x%d)", d->rd, d->imm, d->rs1);
        else if (d->is_jalr)
            printf("x%d, x%d, %d", d->rd, d->rs1, d->imm);
        else
            printf("x%d, x%d, %d", d->rd, d->rs1, d->imm);
        break;
    case TYPE_S:
        printf("x%d, %d(x%d)", d->rs2, d->imm, d->rs1);
        break;
    case TYPE_B:
        printf("x%d, x%d, %d", d->rs1, d->rs2, d->imm);
        break;
    case TYPE_U:
        printf("x%d, 0x%X", d->rd, d->imm >> 12);
        break;
    case TYPE_J:
        printf("x%d, %d", d->rd, d->imm);
        break;
    default:
        break;
    }
    printf("\n");
}
