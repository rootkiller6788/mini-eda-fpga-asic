#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "riscv_core.h"

void regfile_init(RegisterFile *rf) {
    memset(rf->regs, 0, sizeof(rf->regs));
}

void regfile_write(RegisterFile *rf, uint8_t rd, uint32_t val) {
    if (rd == 0) return;
    if (rd < NUM_REGS) rf->regs[rd] = val;
}

uint32_t regfile_read(const RegisterFile *rf, uint8_t rs) {
    if (rs < NUM_REGS) return rf->regs[rs];
    return 0;
}

void mem_init(Memory *mem) {
    memset(mem->data, 0, MEM_SIZE);
}

void mem_write_byte(Memory *mem, uint32_t addr, uint8_t val) {
    if (addr < MEM_SIZE) mem->data[addr] = val;
}

void mem_write_half(Memory *mem, uint32_t addr, uint16_t val) {
    if (addr + 1 < MEM_SIZE) {
        mem->data[addr]     = (uint8_t)(val & 0xFF);
        mem->data[addr + 1] = (uint8_t)((val >> 8) & 0xFF);
    }
}

void mem_write_word(Memory *mem, uint32_t addr, uint32_t val) {
    if (addr + 3 < MEM_SIZE) {
        mem->data[addr]     = (uint8_t)(val & 0xFF);
        mem->data[addr + 1] = (uint8_t)((val >> 8) & 0xFF);
        mem->data[addr + 2] = (uint8_t)((val >> 16) & 0xFF);
        mem->data[addr + 3] = (uint8_t)((val >> 24) & 0xFF);
    }
}

uint8_t mem_read_byte(const Memory *mem, uint32_t addr) {
    if (addr < MEM_SIZE) return mem->data[addr];
    return 0;
}

uint16_t mem_read_half(const Memory *mem, uint32_t addr) {
    if (addr + 1 < MEM_SIZE)
        return (uint16_t)mem->data[addr] | ((uint16_t)mem->data[addr + 1] << 8);
    return 0;
}

uint32_t mem_read_word(const Memory *mem, uint32_t addr) {
    if (addr + 3 < MEM_SIZE)
        return (uint32_t)mem->data[addr]
             | ((uint32_t)mem->data[addr + 1] << 8)
             | ((uint32_t)mem->data[addr + 2] << 16)
             | ((uint32_t)mem->data[addr + 3] << 24);
    return 0;
}

uint8_t inst_opcode(uint32_t inst)    { return (uint8_t)(inst & 0x7F); }
uint8_t inst_rd(uint32_t inst)        { return (uint8_t)((inst >> 7) & 0x1F); }
uint8_t inst_rs1(uint32_t inst)       { return (uint8_t)((inst >> 15) & 0x1F); }
uint8_t inst_rs2(uint32_t inst)       { return (uint8_t)((inst >> 20) & 0x1F); }
uint8_t inst_funct3(uint32_t inst)    { return (uint8_t)((inst >> 12) & 0x7); }
uint8_t inst_funct7(uint32_t inst)    { return (uint8_t)((inst >> 25) & 0x7F); }

InstType decode_type(uint8_t opcode) {
    switch (opcode) {
    case OPCODE_ALU:  return TYPE_R;
    case OPCODE_ALUI:
    case OPCODE_LOAD:
    case OPCODE_JALR: return TYPE_I;
    case OPCODE_STORE:return TYPE_S;
    case OPCODE_BRANCH:return TYPE_B;
    case OPCODE_LUI:
    case OPCODE_AUIPC:return TYPE_U;
    case OPCODE_JAL:  return TYPE_J;
    default:          return TYPE_UNKNOWN;
    }
}

AluOp decode_alu_op(uint8_t opcode, uint8_t funct3, uint8_t funct7) {
    if (opcode == OPCODE_ALU) {
        switch (funct3) {
        case 0x0: return (funct7 == 0x20) ? ALU_SUB : ALU_ADD;
        case 0x1: return ALU_SLL;
        case 0x2: return ALU_SLT;
        case 0x3: return ALU_SLTU;
        case 0x4: return ALU_XOR;
        case 0x5: return (funct7 == 0x20) ? ALU_SRA : ALU_SRL;
        case 0x6: return ALU_OR;
        case 0x7: return ALU_AND;
        default:  return ALU_ADD;
        }
    }
    if (opcode == OPCODE_ALUI) {
        switch (funct3) {
        case 0x0: return ALU_ADD;
        case 0x1: return ALU_SLL;
        case 0x2: return ALU_SLT;
        case 0x3: return ALU_SLTU;
        case 0x4: return ALU_XOR;
        case 0x5: return (funct7 == 0x20) ? ALU_SRA : ALU_SRL;
        case 0x6: return ALU_OR;
        case 0x7: return ALU_AND;
        default:  return ALU_ADD;
        }
    }
    if (opcode == OPCODE_LOAD || opcode == OPCODE_STORE)
        return ALU_ADD;
    if (opcode == OPCODE_BRANCH)
        return ALU_PASS_B;
    if (opcode == OPCODE_LUI || opcode == OPCODE_AUIPC)
        return ALU_ADD;
    if (opcode == OPCODE_JAL || opcode == OPCODE_JALR)
        return ALU_ADD;
    return ALU_ADD;
}

int32_t imm_i(uint32_t inst) {
    return (int32_t)(inst) >> 20;
}

int32_t imm_s(uint32_t inst) {
    int32_t imm = (int32_t)((inst >> 7) & 0x1F);
    imm |= (int32_t)((inst >> 25) & 0x7F) << 5;
    return (imm << 20) >> 20;
}

int32_t imm_b(uint32_t inst) {
    int32_t imm = (int32_t)((inst >> 8) & 0xF) << 1;
    imm |= (int32_t)((inst >> 25) & 0x3F) << 5;
    imm |= (int32_t)((inst >> 7) & 0x1) << 11;
    imm |= (int32_t)((inst >> 31) & 0x1) << 12;
    return (imm << 19) >> 19;
}

int32_t imm_u(uint32_t inst) {
    return (int32_t)(inst & 0xFFFFF000);
}

int32_t imm_j(uint32_t inst) {
    int32_t imm = (int32_t)((inst >> 21) & 0x3FF) << 1;
    imm |= (int32_t)((inst >> 20) & 0x1) << 11;
    imm |= (int32_t)((inst >> 12) & 0xFF) << 12;
    imm |= (int32_t)((inst >> 31) & 0x1) << 20;
    return (imm << 11) >> 11;
}

uint32_t alu_compute(AluOp op, uint32_t a, uint32_t b) {
    switch (op) {
    case ALU_ADD:   return a + b;
    case ALU_SUB:   return a - b;
    case ALU_SLL:   return a << (b & 0x1F);
    case ALU_SLT:   return ((int32_t)a < (int32_t)b) ? 1 : 0;
    case ALU_SLTU:  return (a < b) ? 1 : 0;
    case ALU_XOR:   return a ^ b;
    case ALU_SRL:   return a >> (b & 0x1F);
    case ALU_SRA:   return (uint32_t)((int32_t)a >> (b & 0x1F));
    case ALU_OR:    return a | b;
    case ALU_AND:   return a & b;
    case ALU_PASS_B:return b;
    default:        return 0;
    }
}

bool branch_taken(uint8_t funct3, uint32_t rs1, uint32_t rs2) {
    switch (funct3) {
    case 0x0: return rs1 == rs2;
    case 0x1: return rs1 != rs2;
    case 0x4: return (int32_t)rs1 < (int32_t)rs2;
    case 0x5: return (int32_t)rs1 >= (int32_t)rs2;
    case 0x6: return rs1 < rs2;
    case 0x7: return rs1 >= rs2;
    default:  return false;
    }
}

static void stage_regs_reset(RiscvCore *core) {
    memset(&core->if_id, 0, sizeof(IF_ID_Reg));
    memset(&core->id_ex, 0, sizeof(ID_EX_Reg));
    memset(&core->ex_mem, 0, sizeof(EX_MEM_Reg));
    memset(&core->mem_wb, 0, sizeof(MEM_WB_Reg));
}

void core_init(RiscvCore *core) {
    memset(core, 0, sizeof(RiscvCore));
    regfile_init(&core->rf);
    mem_init(&core->imem);
    mem_init(&core->dmem);
    core->pc = RESET_PC;
    stage_regs_reset(core);
    core->mstatus  = 0x00001800;
    core->mtvec    = 0x00000000;
    core->mepc     = 0x00000000;
    core->mcause   = 0x00000000;
    core->cycle_count = 0;
    core->inst_count  = 0;
}

void core_reset(RiscvCore *core) {
    uint32_t saved_status = core->mstatus & 0x00001800;
    core_init(core);
    core->mstatus = saved_status;
}

void core_load_program(RiscvCore *core, const uint32_t *program, uint32_t len) {
    for (uint32_t i = 0; i < len && i < MEM_SIZE / 4; i++) {
        mem_write_word(&core->imem, i * 4, program[i]);
    }
}

void pipeline_if(RiscvCore *core) {
    if (core->stall_if) {
        core->stall_if = false;
        return;
    }
    uint32_t inst = mem_read_word(&core->imem, core->pc);
    core->if_id.valid = true;
    core->if_id.pc    = core->pc;
    core->if_id.inst  = inst;
}

void pipeline_id(RiscvCore *core) {
    if (!core->if_id.valid) {
        core->id_ex.valid = false;
        return;
    }

    if (core->stall_id) {
        core->stall_id = false;
        return;
    }

    uint32_t inst = core->if_id.inst;
    uint32_t pc   = core->if_id.pc;

    uint8_t opcode = inst_opcode(inst);

    core->id_ex.valid   = true;
    core->id_ex.pc       = pc;
    core->id_ex.inst     = inst;
    core->id_ex.rs1      = inst_rs1(inst);
    core->id_ex.rs2      = inst_rs2(inst);
    core->id_ex.rd       = inst_rd(inst);
    core->id_ex.opcode   = opcode;
    core->id_ex.type     = decode_type(opcode);
    core->id_ex.funct3   = inst_funct3(inst);
    core->id_ex.funct7   = inst_funct7(inst);

    core->id_ex.rs1_data = regfile_read(&core->rf, core->id_ex.rs1);
    core->id_ex.rs2_data = regfile_read(&core->rf, core->id_ex.rs2);

    switch (core->id_ex.type) {
    case TYPE_I: core->id_ex.imm = imm_i(inst); break;
    case TYPE_S: core->id_ex.imm = imm_s(inst); break;
    case TYPE_B: core->id_ex.imm = imm_b(inst); break;
    case TYPE_U: core->id_ex.imm = imm_u(inst); break;
    case TYPE_J: core->id_ex.imm = imm_j(inst); break;
    default:     core->id_ex.imm = 0; break;
    }

    core->id_ex.alu_op  = decode_alu_op(opcode, core->id_ex.funct3,
                                        core->id_ex.funct7);
    core->id_ex.is_branch = (opcode == OPCODE_BRANCH);
    core->id_ex.is_jal    = (opcode == OPCODE_JAL);
    core->id_ex.is_jalr   = (opcode == OPCODE_JALR);
    core->id_ex.is_load   = (opcode == OPCODE_LOAD);
    core->id_ex.is_store  = (opcode == OPCODE_STORE);
    core->id_ex.is_auipc  = (opcode == OPCODE_AUIPC);
    core->id_ex.is_lui    = (opcode == OPCODE_LUI);

    core->id_ex.wb_en = !core->id_ex.is_branch && !core->id_ex.is_store
                      && opcode != OPCODE_SYSTEM;

    if (core->flush_ex) {
        core->flush_ex = false;
        core->id_ex.valid = false;
    }
}

static uint32_t get_operand_a(RiscvCore *core, uint32_t rs1_val, ForwardSrc fwd) {
    switch (fwd) {
    case FORWARD_EX_MEM:  return core->ex_mem.alu_result;
    case FORWARD_MEM_WB:  return core->mem_wb.alu_result;
    default:              return rs1_val;
    }
}

static uint32_t get_operand_b(RiscvCore *core, uint32_t rs2_val, ForwardSrc fwd) {
    switch (fwd) {
    case FORWARD_EX_MEM:  return core->ex_mem.alu_result;
    case FORWARD_MEM_WB:  return core->mem_wb.alu_result;
    default:              return rs2_val;
    }
}

void pipeline_ex(RiscvCore *core) {
    if (!core->id_ex.valid) {
        core->ex_mem.valid = false;
        return;
    }

    uint32_t op_a = core->id_ex.rs1_data;
    uint32_t op_b = core->id_ex.rs2_data;
    ForwardSrc fwd_a = FORWARD_NONE;
    ForwardSrc fwd_b = FORWARD_NONE;

    if (core->ex_mem.valid && core->ex_mem.wb_en && core->ex_mem.rd != 0) {
        if (core->ex_mem.rd == core->id_ex.rs1) fwd_a = FORWARD_EX_MEM;
        if (core->ex_mem.rd == core->id_ex.rs2) fwd_b = FORWARD_EX_MEM;
    }
    if (core->mem_wb.valid && core->mem_wb.wb_en && core->mem_wb.rd != 0) {
        if (core->mem_wb.rd == core->id_ex.rs1 && fwd_a == FORWARD_NONE)
            fwd_a = FORWARD_MEM_WB;
        if (core->mem_wb.rd == core->id_ex.rs2 && fwd_b == FORWARD_NONE)
            fwd_b = FORWARD_MEM_WB;
    }

    op_a = get_operand_a(core, op_a, fwd_a);
    op_b = get_operand_b(core, op_b, fwd_b);

    uint32_t alu_a = op_a;
    uint32_t alu_b = op_b;

    if (core->id_ex.type == TYPE_I || core->id_ex.type == TYPE_S)
        alu_b = (uint32_t)core->id_ex.imm;
    if (core->id_ex.is_auipc)
        alu_a = core->id_ex.pc;
    if (core->id_ex.is_jal || core->id_ex.is_jalr)
        core->ex_mem.pc_plus_4 = core->id_ex.pc + 4;

    core->ex_mem.alu_result = alu_compute(core->id_ex.alu_op, alu_a, alu_b);
    core->ex_mem.store_data = op_b;
    core->ex_mem.rd         = core->id_ex.rd;
    core->ex_mem.funct3     = core->id_ex.funct3;
    core->ex_mem.is_load    = core->id_ex.is_load;
    core->ex_mem.is_store   = core->id_ex.is_store;
    core->ex_mem.is_jal     = core->id_ex.is_jal;
    core->ex_mem.is_jalr    = core->id_ex.is_jalr;
    core->ex_mem.wb_en      = core->id_ex.wb_en;
    core->ex_mem.valid      = true;
    core->ex_mem.pc         = core->id_ex.pc;
    core->ex_mem.is_ecall   = core->id_ex.is_ecall;
    core->ex_mem.is_ebreak  = core->id_ex.is_ebreak;
    core->ex_mem.is_mret    = core->id_ex.is_mret;
    core->ex_mem.pc_plus_4  = core->id_ex.pc + 4;

    if (core->id_ex.is_branch) {
        bool taken = branch_taken(core->id_ex.funct3, op_a, op_b);
        if (taken) {
            uint32_t target = core->id_ex.pc + (uint32_t)core->id_ex.imm;
            core->correct_pc = target;
            core->branch_mispredict = true;
        } else {
            core->correct_pc = core->id_ex.pc + 4;
            core->branch_mispredict = false;
        }
        core->ex_mem.valid = false;
    }
}

void pipeline_mem(RiscvCore *core) {
    if (!core->ex_mem.valid) {
        core->mem_wb.valid = false;
        return;
    }

    if (core->ex_mem.is_load) {
        switch (core->ex_mem.funct3) {
        case 0x2: core->ex_mem.alu_result =
                  (int32_t)mem_read_word(&core->dmem, core->ex_mem.alu_result);
                  break;
        case 0x1: core->ex_mem.alu_result =
                  (int32_t)(int16_t)mem_read_half(&core->dmem, core->ex_mem.alu_result);
                  break;
        case 0x0: core->ex_mem.alu_result =
                  (int32_t)(int8_t)mem_read_byte(&core->dmem, core->ex_mem.alu_result);
                  break;
        case 0x5: core->ex_mem.alu_result =
                  (uint32_t)mem_read_half(&core->dmem, core->ex_mem.alu_result);
                  break;
        case 0x4: core->ex_mem.alu_result =
                  (uint32_t)mem_read_byte(&core->dmem, core->ex_mem.alu_result);
                  break;
        default: break;
        }
    }

    if (core->ex_mem.is_store) {
        uint32_t addr = core->ex_mem.alu_result;
        uint32_t data = core->ex_mem.store_data;
        switch (core->ex_mem.funct3) {
        case 0x2: mem_write_word(&core->dmem, addr, data); break;
        case 0x1: mem_write_half(&core->dmem, addr, (uint16_t)data); break;
        case 0x0: mem_write_byte(&core->dmem, addr, (uint8_t)data); break;
        default: break;
        }
    }

    core->mem_wb.valid      = core->ex_mem.valid;
    core->mem_wb.pc         = core->ex_mem.pc;
    core->mem_wb.alu_result = core->ex_mem.alu_result;
    core->mem_wb.rd         = core->ex_mem.rd;
    core->mem_wb.wb_en      = core->ex_mem.wb_en;
    core->mem_wb.is_jal     = core->ex_mem.is_jal;
    core->mem_wb.is_jalr    = core->ex_mem.is_jalr;
    core->mem_wb.pc_plus_4  = core->ex_mem.pc_plus_4;
    core->mem_wb.is_ecall   = core->ex_mem.is_ecall;
    core->mem_wb.is_ebreak  = core->ex_mem.is_ebreak;
    core->mem_wb.is_mret    = core->ex_mem.is_mret;
    core->mem_wb.is_auipc   = false;
    core->mem_wb.is_lui     = false;
    core->mem_wb.funct3     = core->ex_mem.funct3;
    core->mem_wb.is_load    = core->ex_mem.is_load;
}

void pipeline_wb(RiscvCore *core) {
    if (!core->mem_wb.valid) return;

    if (core->mem_wb.wb_en && core->mem_wb.rd != 0) {
        uint32_t wb_val = core->mem_wb.alu_result;
        if (core->mem_wb.is_jal || core->mem_wb.is_jalr)
            wb_val = core->mem_wb.pc_plus_4;
        regfile_write(&core->rf, core->mem_wb.rd, wb_val);
    }
}

void core_tick(RiscvCore *core) {
    pipeline_wb(core);
    pipeline_mem(core);
    pipeline_ex(core);
    pipeline_id(core);
    pipeline_if(core);

    if (core->branch_mispredict) {
        core->pc = core->correct_pc;
        core->branch_mispredict = false;
        stage_regs_reset(core);
    } else {
        core->pc += 4;
    }

    if (core->pc >= MEM_SIZE) core->pc = 0;

    core->cycle_count++;
    if (core->if_id.valid) core->inst_count++;
}

void core_step(RiscvCore *core, uint32_t steps) {
    for (uint32_t i = 0; i < steps; i++) core_tick(core);
}

void csr_write(RiscvCore *core, uint32_t addr, uint32_t val) {
    switch (addr) {
    case 0x300: core->mstatus = val; break;
    case 0x305: core->mtvec   = val; break;
    case 0x341: core->mepc    = val; break;
    case 0x342: core->mcause  = val; break;
    default: break;
    }
}

uint32_t csr_read(const RiscvCore *core, uint32_t addr) {
    switch (addr) {
    case 0x300: return core->mstatus;
    case 0x305: return core->mtvec;
    case 0x341: return core->mepc;
    case 0x342: return core->mcause;
    default:    return 0;
    }
}

void csr_take_trap(RiscvCore *core, uint32_t cause, uint32_t epc) {
    core->mcause = cause;
    core->mepc   = epc;
    uint32_t mie = (core->mstatus >> 3) & 1;
    core->mstatus = (core->mstatus & ~0x80) | (mie << 7);
    core->mstatus &= ~0x8;
    core->pc = core->mtvec;
}

void csr_mret(RiscvCore *core) {
    uint32_t mpie = (core->mstatus >> 7) & 1;
    core->mstatus = (core->mstatus & ~0x8) | (mpie << 3);
    core->mstatus |= 0x80;
    core->pc = core->mepc;
}

void core_dump_regs(const RiscvCore *core) {
    printf("=== Register File ===\n");
    for (int i = 0; i < 32; i++) {
        printf("x%-2d = 0x%08X", i, core->rf.regs[i]);
        if ((i + 1) % 4 == 0) printf("\n");
        else printf("  ");
    }
}

void core_dump_pipeline(const RiscvCore *core) {
    printf("=== Pipeline State (cycle %u) ===\n", core->cycle_count);
    printf("PC:   0x%08X\n", core->pc);
    printf("IF/ID: valid=%d pc=0x%08X inst=0x%08X\n",
           core->if_id.valid, core->if_id.pc, core->if_id.inst);
    printf("ID/EX: valid=%d pc=0x%08X inst=0x%08X\n",
           core->id_ex.valid, core->id_ex.pc, core->id_ex.inst);
    printf("EX/MEM:valid=%d pc=0x%08X alu=0x%08X\n",
           core->ex_mem.valid, core->ex_mem.pc, core->ex_mem.alu_result);
    printf("MEM/WB:valid=%d pc=0x%08X alu=0x%08X\n",
           core->mem_wb.valid, core->mem_wb.pc, core->mem_wb.alu_result);
}

void core_dump_csr(const RiscvCore *core) {
    printf("=== CSR State ===\n");
    printf("mstatus:  0x%08X\n", core->mstatus);
    printf("mtvec:    0x%08X\n", core->mtvec);
    printf("mepc:     0x%08X\n", core->mepc);
    printf("mcause:   0x%08X\n", core->mcause);
    printf("mtime:    %llu\n", (unsigned long long)core->mtime);
    printf("mtimecmp: %llu\n", (unsigned long long)core->mtimecmp);
}
