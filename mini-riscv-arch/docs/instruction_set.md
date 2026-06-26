# RV32I Base Integer Instruction Set

## Instruction Encoding

All RV32I instructions are 32 bits wide. The opcode is in bits [6:0].

### R-type (Register-Register)

| Bit Range  | Field   | Description           |
|------------|---------|-----------------------|
| [6:0]      | opcode  | 0110011               |
| [11:7]     | rd      | Destination register  |
| [14:12]    | funct3  | Operation sub-code    |
| [19:15]    | rs1     | Source register 1     |
| [24:20]    | rs2     | Source register 2     |
| [31:25]    | funct7  | Operation variant     |

#### R-type Instructions

| Instruction | funct3 | funct7  | Operation                  |
|-------------|--------|---------|----------------------------|
| ADD         | 0x0    | 0x00    | rd = rs1 + rs2             |
| SUB         | 0x0    | 0x20    | rd = rs1 - rs2             |
| SLL         | 0x1    | 0x00    | rd = rs1 << rs2[4:0]       |
| SLT         | 0x2    | 0x00    | rd = (signed rs1 < rs2)?1:0|
| SLTU        | 0x3    | 0x00    | rd = (rs1 < rs2) ? 1 : 0   |
| XOR         | 0x4    | 0x00    | rd = rs1 ^ rs2             |
| SRL         | 0x5    | 0x00    | rd = rs1 >> rs2[4:0]       |
| SRA         | 0x5    | 0x20    | rd = signed(rs1) >> rs2[4:0]|
| OR          | 0x6    | 0x00    | rd = rs1 | rs2             |
| AND         | 0x7    | 0x00    | rd = rs1 & rs2             |

### I-type (Register-Immediate)

| Bit Range  | Field   | Description             |
|------------|---------|-------------------------|
| [6:0]      | opcode  | varies by type          |
| [11:7]     | rd      | Destination register    |
| [14:12]    | funct3  | Operation sub-code      |
| [19:15]    | rs1     | Source register         |
| [31:20]    | imm[11:0]| 12-bit signed immediate |

#### I-type ALU Instructions (opcode 0010011)

| Instruction | funct3 | Operation                     |
|-------------|--------|-------------------------------|
| ADDI        | 0x0    | rd = rs1 + sext(imm[11:0])    |
| SLTI        | 0x2    | rd = (signed rs1 < sext(imm))?1:0 |
| SLTIU       | 0x3    | rd = (rs1 < sext(imm)) ? 1 : 0    |
| XORI        | 0x4    | rd = rs1 ^ sext(imm[11:0])    |
| ORI         | 0x6    | rd = rs1 | sext(imm[11:0])   |
| ANDI        | 0x7    | rd = rs1 & sext(imm[11:0])    |
| SLLI        | 0x1    | rd = rs1 << imm[4:0]          |
| SRLI        | 0x5    | rd = rs1 >> imm[4:0] (funct7=0x00) |
| SRAI        | 0x5    | rd = signed(rs1) >> imm[4:0] (funct7=0x20) |

#### I-type Load Instructions (opcode 0000011)

| Instruction | funct3 | Operation                            |
|-------------|--------|--------------------------------------|
| LB          | 0x0    | rd = sext(M[rs1 + sext(imm)][7:0])   |
| LH          | 0x1    | rd = sext(M[rs1 + sext(imm)][15:0])  |
| LW          | 0x2    | rd = M[rs1 + sext(imm)][31:0]        |
| LBU         | 0x4    | rd = M[rs1 + sext(imm)][7:0]         |
| LHU         | 0x5    | rd = M[rs1 + sext(imm)][15:0]        |

#### I-type JALR (opcode 1100111)

| Instruction | funct3 | Operation                          |
|-------------|--------|------------------------------------|
| JALR        | 0x0    | rd = PC+4; PC = (rs1 + sext(imm)) & ~1 |

### S-type (Store)

| Bit Range   | Field   | Description             |
|-------------|---------|-------------------------|
| [6:0]       | opcode  | 0100011                 |
| [11:7]      | imm[4:0]| 5-bit immediate (low)   |
| [14:12]     | funct3  | Store width             |
| [19:15]     | rs1     | Base address register   |
| [24:20]     | rs2     | Data source register    |
| [31:25]     | imm[11:5]| 7-bit immediate (high) |

| Instruction | funct3 | Operation                            |
|-------------|--------|--------------------------------------|
| SB          | 0x0    | M[rs1 + sext(imm)][7:0] = rs2[7:0]   |
| SH          | 0x1    | M[rs1 + sext(imm)][15:0] = rs2[15:0] |
| SW          | 0x2    | M[rs1 + sext(imm)][31:0] = rs2[31:0] |

### B-type (Branch)

| Bit Range   | Field      | Description                     |
|-------------|------------|---------------------------------|
| [6:0]       | opcode     | 1100011                         |
| [11:7]      | imm[4:1|11]| Immediate (bits 4:1, 11)       |
| [14:12]     | funct3     | Branch condition                |
| [19:15]     | rs1        | Source register 1               |
| [24:20]     | rs2        | Source register 2               |
| [31:25]     | imm[12|10:5]| Immediate (bits 12, 10:5)     |

Immediate encoding: {imm[12], imm[10:5], imm[4:1], imm[11]} → shifted left by 1.

| Instruction | funct3 | Condition taken                    |
|-------------|--------|------------------------------------|
| BEQ         | 0x0    | rs1 == rs2                         |
| BNE         | 0x1    | rs1 != rs2                         |
| BLT         | 0x4    | signed(rs1) < signed(rs2)          |
| BGE         | 0x5    | signed(rs1) >= signed(rs2)         |
| BLTU        | 0x6    | rs1 < rs2 (unsigned)               |
| BGEU        | 0x7    | rs1 >= rs2 (unsigned)              |

### U-type (Upper Immediate)

| Bit Range  | Field   | Description               |
|------------|---------|---------------------------|
| [6:0]      | opcode  | varies                    |
| [11:7]     | rd      | Destination register      |
| [31:12]    | imm[31:12]| 20-bit upper immediate |

| Instruction | opcode   | Operation                     |
|-------------|----------|-------------------------------|
| LUI         | 0110111  | rd = imm[31:12] << 12         |
| AUIPC       | 0010111  | rd = PC + (imm[31:12] << 12) |

### J-type (Jump)

| Bit Range   | Field                   | Description                  |
|-------------|------------------------|------------------------------|
| [6:0]       | opcode                 | 1101111                      |
| [11:7]      | rd                     | Destination register         |
| [19:12]     | imm[19:12]             | Immediate bits 19:12         |
| [20]        | imm[11]                | Immediate bit 11             |
| [30:21]     | imm[10:1]              | Immediate bits 10:1          |
| [31]        | imm[20]                | Immediate bit 20             |

| Instruction | opcode   | Operation                                |
|-------------|----------|------------------------------------------|
| JAL         | 1101111  | rd = PC+4; PC = PC + sext(imm[20|10:1|11|19:12]) |

### System Instructions (opcode 1110011)

| Instruction | funct3 | funct12  | Operation                |
|-------------|--------|----------|--------------------------|
| ECALL       | 0x0    | 0x000    | Environment call (trap)  |
| EBREAK      | 0x0    | 0x001    | Debugger breakpoint      |
| MRET        | 0x0    | 0x302    | Return from M-mode trap  |

## Pseudo-Instructions

Common RISC-V pseudo-instructions and their expansions:

| Pseudo     | Expansion        | Description               |
|------------|------------------|---------------------------|
| NOP        | addi x0, x0, 0   | No operation              |
| MV rd, rs  | addi rd, rs, 0   | Copy register             |
| NOT rd, rs | xori rd, rs, -1  | Bitwise NOT               |
| NEG rd, rs | sub  rd, x0, rs  | Negate                    |
| SEQZ rd,rs | sltiu rd, rs, 1  | Set if == 0               |
| SNEZ rd,rs | sltu  rd, x0, rs | Set if != 0               |
| J offset   | jal  x0, offset  | Unconditional jump        |
| JR rs      | jalr x0, rs, 0   | Jump register             |
| RET        | jalr x0, ra, 0   | Return from subroutine    |
| CALL offset| auipc ra, offset_hi; jalr ra, ra, offset_lo | Far call |
| LI rd, imm | addi rd, x0, imm | Load immediate (small)    |

## ALU Operation Codes

| Code | Name   | Operation               |
|------|--------|-------------------------|
| 0    | ADD    | a + b                   |
| 1    | SUB    | a - b                   |
| 2    | SLL    | a << (b & 0x1F)         |
| 3    | SLT    | (int32_t)a < (int32_t)b |
| 4    | SLTU   | a < b                   |
| 5    | XOR    | a ^ b                   |
| 6    | SRL    | a >> (b & 0x1F)         |
| 7    | SRA    | (int32_t)a >> (b & 0x1F)|
| 8    | OR     | a | b                   |
| 9    | AND    | a & b                   |
| 10   | COPY   | b (used for LUI)        |
| 11   | ADD_PC | a + b (used for AUIPC)  |

## CSR Address Map

| Address | Name     | Description                     |
|---------|----------|---------------------------------|
| 0x300   | mstatus  | Machine status register         |
| 0x301   | misa     | Machine ISA register            |
| 0x304   | mie      | Machine interrupt enable        |
| 0x305   | mtvec    | Machine trap vector             |
| 0x340   | mscratch | Machine scratch register        |
| 0x341   | mepc     | Machine exception PC            |
| 0x342   | mcause   | Machine trap cause              |
| 0x343   | mtval    | Machine trap value              |
| 0x344   | mip      | Machine interrupt pending       |
| 0xB00   | mcycle   | Machine cycle counter (low)     |
| 0xB80   | mcycleh  | Machine cycle counter (high)    |
| 0xB01   | mtime    | Machine timer (low)             |
| 0xB81   | mtimeh   | Machine timer (high)            |
| 0xB02   | mtimecmp | Machine timer compare (low)     |
| 0xB83   | mtimecmph| Machine timer compare (high)    |

## Trap Cause Codes

| Code | Description                      | Type        |
|------|----------------------------------|-------------|
| 0    | Instruction address misaligned   | Exception   |
| 1    | Instruction access fault         | Exception   |
| 2    | Illegal instruction              | Exception   |
| 3    | Breakpoint                       | Exception   |
| 4    | Load address misaligned          | Exception   |
| 5    | Load access fault                | Exception   |
| 6    | Store address misaligned         | Exception   |
| 7    | Store access fault               | Exception   |
| 8    | Environment call from U-mode     | Exception   |
| 9    | Environment call from S-mode     | Exception   |
| 11   | Environment call from M-mode     | Exception   |
| 12   | Instruction page fault           | Exception   |
| 13   | Load page fault                  | Exception   |
| 15   | Store page fault                 | Exception   |
| 3    | Machine software interrupt       | Interrupt   |
| 7    | Machine timer interrupt          | Interrupt   |
| 11   | Machine external interrupt       | Interrupt   |
