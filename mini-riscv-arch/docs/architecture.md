# RISC-V Processor Architecture

## Overview

The mini-riscv-arch implements a classic 5-stage in-order RISC pipeline
targeting the RV32I base integer instruction set. The design is written in
C99 for simulation and verification purposes, with an optional Verilog RTL
generator for synthesis.

## Pipeline Stages

```
                 +--------+       +--------+       +--------+
    PC -------->|   IF   |------>|   ID   |------>|   EX   |
                 +--------+       +--------+       +--------+
                     |                |                |
                     v                v                v
              Instruction      Register Read    ALU / Branch
                Fetch           + Decode        Resolution
                                                     |
                 +--------+       +--------+          |
                 |   WB   |<------|  MEM   |<---------+
                 +--------+       +--------+
                     |                |
                     v                v
              Write Back to    Data Memory
              Register File    Access
```

### 1. Instruction Fetch (IF)
- PC register holds the address of the next instruction
- Instruction is read from instruction memory (IMEM)
- PC is incremented by 4 (for 32-bit instructions) unless a branch/trap occurs
- The fetched instruction and PC are latched in the IF/ID pipeline register

### 2. Instruction Decode (ID)
- The 32-bit instruction word is decoded:
  - opcode (bits [6:0]) - determines instruction format
  - funct3 (bits [14:12]) - sub-opcode for branches, loads, ALU ops
  - funct7 (bits [31:25]) - distinguishes ADD/SUB, SRL/SRA
  - rs1, rs2 (source registers), rd (destination register)
- Immediate value is generated based on instruction type (I/S/B/U/J)
- Source operands are read from the register file
- Control signals are generated for subsequent stages
- Decoded instruction is latched in the ID/EX pipeline register

### 3. Execute (EX)
- ALU performs the operation specified by ALU control
- For branch instructions, the branch condition is evaluated
- For load/store, the effective address is computed (rs1 + immediate)
- Forwarding multiplexers select operands from EX/MEM or MEM/WB if needed
- Branch mispredict detection and correction occurs here
- Results are latched in the EX/MEM pipeline register

### 4. Memory Access (MEM)
- Load instructions read from data memory (DMEM)
- Store instructions write to data memory
- Load data is sign/zero-extended based on funct3
- Results are latched in the MEM/WB pipeline register

### 5. Writeback (WB)
- Results are written back to the register file
- Sources include ALU result, memory data, or PC+4 (for JAL/JALR)
- x0 is hardwired to zero

## Register File

| Register | ABI Name | Description |
|----------|----------|-------------|
| x0       | zero     | Hardwired to zero |
| x1       | ra       | Return address |
| x2       | sp       | Stack pointer |
| x3       | gp       | Global pointer |
| x4       | tp       | Thread pointer |
| x5-x7    | t0-t2    | Temporaries |
| x8       | s0/fp    | Saved register / Frame pointer |
| x9       | s1       | Saved register |
| x10-x17  | a0-a7    | Function arguments |
| x18-x27  | s2-s11   | Saved registers |
| x28-x31  | t3-t6    | Temporaries |

## Pipeline Hazards

### Data Hazards (RAW)

**Case 1: EX/MEM -> EX Forwarding**
```
add x1, x2, x3    IF  ID  EX  MEM  WB
sub x4, x1, x5         IF  ID  EX  MEM  WB
                              ^
                              +--- forwarded from EX/MEM
```
The result of `add` is available at the end of EX stage. The `sub` instruction
needs x1 in its EX stage. A forwarding path from EX/MEM to EX provides the
value without stalling.

**Case 2: MEM/WB -> EX Forwarding**
```
add x1, x2, x3    IF  ID  EX  MEM  WB
nop                    IF  ID  EX  MEM  WB
sub x4, x1, x5              IF  ID  EX  MEM  WB
                                   ^
                                   +--- forwarded from MEM/WB
```

**Case 3: Load-Use Hazard (Requires Stall)**
```
lw  x1, 0(x2)     IF  ID  EX  MEM  WB
add x3, x1, x4         IF  ID  ID  EX  MEM  WB    <-- ID repeated (stall)
```
The load produces its result at the end of MEM. The dependent instruction
needs it in EX. One cycle stall is inserted.

### Control Hazards

Branch prediction: static not-taken.
- On a taken branch, instructions fetched after the branch are flushed
- Branch is resolved in the EX stage
- Mispredict penalty: 2 cycles (instructions in IF and ID are flushed)

## CSR (Control and Status Registers)

M-mode CSRs implemented:
- **mstatus** (0x300): Machine status (MIE, MPIE, MPP)
- **mtvec** (0x305): Machine trap vector base address
- **mepc** (0x341): Machine exception program counter
- **mcause** (0x342): Machine trap cause
- **mie/mip** (0x304/0x344): Machine interrupt enable/pending

### Trap Flow
1. Exception/interrupt occurs
2. mcause set to trap cause
3. mepc set to current PC (or next PC for interrupts)
4. mstatus.MPIE = mstatus.MIE, mstatus.MIE = 0
5. PC = mtvec (base address of trap handler)
6. Handler executes, ends with MRET
7. MRET: mstatus.MIE = mstatus.MPIE, PC = mepc

## Memory Map

| Address Range        | Region       |
|---------------------|--------------|
| 0x00000000-0x00000FFF | Instruction Memory (IMEM) |
| 0x00000000-0x00000FFF | Data Memory (DMEM)        |

## Data Types

### Instruction Formats (RV32I)

```
R-type: funct7[31:25] rs2[24:20] rs1[19:15] funct3[14:12] rd[11:7] opcode[6:0]
I-type: imm[31:20]    rs1[19:15] funct3[14:12] rd[11:7] opcode[6:0]
S-type: imm[31:25]    rs2[24:20] rs1[19:15] funct3[14:12] imm[11:7] opcode[6:0]
B-type: imm[12|10:5]  rs2[24:20] rs1[19:15] funct3[14:12] imm[4:1|11] opcode[6:0]
U-type: imm[31:12]    rd[11:7]   opcode[6:0]
J-type: imm[20|10:1|11|19:12]   rd[11:7]   opcode[6:0]
```

## Performance Model

- Single-issue, in-order execution
- 1 cycle per pipeline stage (ideal case)
- Stalls for load-use hazards (1 cycle)
- Flushes for branch mispredicts (2 cycle penalty)
- CPI ideal: 1.0 (ignoring hazards)
- IPC theoretical max: 1.0

## Verilog RTL Generation

The riscv_verilog module generates synthesizable Verilog-2001 RTL from the
processor model. The generated code includes:

- 5-stage pipeline datapath
- Hardwired control unit (or FSM option)
- Hazard detection with forwarding and stalling
- Register file with x0 hardwired to 0
- ALU with all RV32I operations
- Instruction and data memories
- CSR registers (M-mode)
- Timer (mtime/mtimecmp)
- Writeback multiplexer
