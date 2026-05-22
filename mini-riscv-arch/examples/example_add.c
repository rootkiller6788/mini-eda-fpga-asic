#include <stdio.h>
#include <stdlib.h>
#include "riscv_core.h"
#include "riscv_decode.h"

#define PROGRAM_SIZE (MEM_SIZE / 4)

int main(void) {
    RiscvCore core;
    core_init(&core);

    uint32_t program[PROGRAM_SIZE];
    uint32_t n = 0;

    program[n++] = 0x00100093; // addi x1, x0, 1
    program[n++] = 0x00200113; // addi x2, x0, 2
    program[n++] = 0x00300193; // addi x3, x0, 3
    program[n++] = 0x002081B3; // add  x3, x1, x2
    program[n++] = 0x402081B3; // sub  x3, x1, x2
    program[n++] = 0x0000A137; // lui  x2, 10
    program[n++] = 0x0000A197; // auipc x3, 10
    program[n++] = 0x00312023; // sw   x3, 0(x2)
    program[n++] = 0x00012183; // lw   x3, 0(x2)
    program[n++] = 0x001100B3; // add  x1, x2, x1
    program[n++] = 0x00208133; // add  x2, x1, x2
    program[n++] = 0x00000013; // nop

    core_load_program(&core, program, n);

    printf("=== RISC-V Example: Basic ADD/SUB/LUI ===\n");
    printf("Program loaded: %u instructions\n\n", n);

    for (uint32_t i = 0; i < n; i++) {
        DecodedInst d = decode_instruction(program[i]);
        printf("[%2u] ", i);
        print_instruction(&d, i * 4);
    }
    printf("\n");

    printf("Running 20 cycles...\n\n");
    for (uint32_t i = 0; i < 20; i++) {
        printf("--- Cycle %u ---\n", core.cycle_count);
        core_tick(&core);

        if (core.mem_wb.valid && core.mem_wb.wb_en && core.mem_wb.rd != 0) {
            printf("WB: x%u <- 0x%08X\n",
                   core.mem_wb.rd, core.mem_wb.alu_result);
        }
    }

    printf("\n");
    core_dump_regs(&core);
    printf("\n");
    core_dump_pipeline(&core);

    printf("\nFinal register values:\n");
    printf("x1 (ra) = 0x%08X\n", regfile_read(&core.rf, 1));
    printf("x2 (sp) = 0x%08X\n", regfile_read(&core.rf, 2));
    printf("x3 (gp) = 0x%08X\n", regfile_read(&core.rf, 3));
    printf("\nCycles: %u, Instructions: %u\n",
           core.cycle_count, core.inst_count);

    return 0;
}
