#include <stdio.h>
#include <stdlib.h>
#include "riscv_core.h"
#include "riscv_decode.h"
#include "pipeline_hazards.h"

#define PROGRAM_SIZE (MEM_SIZE / 4)

static void run_branch_test(const char *name, const uint32_t *program,
                            uint32_t len, uint32_t cycles) {
    RiscvCore core;
    core_init(&core);
    core_load_program(&core, program, len);

    printf("=== %s ===\n", name);
    printf("Program:\n");
    for (uint32_t i = 0; i < len; i++) {
        DecodedInst d = decode_instruction(program[i]);
        printf("  ");
        print_instruction(&d, i * 4);
    }

    printf("\nExecuting %u cycles...\n", cycles);
    for (uint32_t c = 0; c < cycles; c++) {
        HazardInfo hz;
        hazard_unit_init(&hz);
        hazard_detect(&core, &hz);

        if (hz.type != HAZ_NONE) {
            printf("  [cycle %2u] Hazard: ", c);
            switch (hz.type) {
            case HAZ_RAW_RS1:  printf("RAW on RS1\n"); break;
            case HAZ_RAW_RS2:  printf("RAW on RS2\n"); break;
            case HAZ_LOAD_USE: printf("LOAD-USE (stall)\n"); break;
            case HAZ_CONTROL:  printf("CONTROL (%s)\n",
                                      hz.branch_actual_taken ? "taken" : "not taken"); break;
            default: printf("other\n"); break;
            }
            hazard_resolve(&core, &hz);
        }

        core_tick(&core);
    }

    printf("\nResults:\n");
    for (int i = 1; i <= 5; i++)
        printf("  x%d = 0x%08X\n", i, regfile_read(&core.rf, i));
    printf("\n");
}

int main(void) {
    uint32_t prog[PROGRAM_SIZE];
    uint32_t n;

    n = 0;
    prog[n++] = 0x00100113; // addi x2, x0, 1
    prog[n++] = 0x00200193; // addi x3, x0, 2
    prog[n++] = 0x00300213; // addi x4, x0, 3
    prog[n++] = 0x00218A63; // beq  x3, x2, +20 (skip next)
    prog[n++] = 0x00120213; // addi x4, x4, 1
    prog[n++] = 0x00500213; // addi x4, x0, 5
    prog[n++] = 0x00000013; // nop
    run_branch_test("BEQ (equal, should jump)", prog, n, 20);

    n = 0;
    prog[n++] = 0x00100113; // addi x2, x0, 1
    prog[n++] = 0x00200193; // addi x3, x0, 2
    prog[n++] = 0x00318C63; // beq  x3, x3, +24 (skip 2)
    prog[n++] = 0x00A00213; // addi x4, x0, 10
    prog[n++] = 0x00B00293; // addi x5, x0, 11
    prog[n++] = 0x00C00313; // addi x6, x0, 12
    prog[n++] = 0x00D00393; // addi x7, x0, 13
    prog[n++] = 0x00000013; // nop
    run_branch_test("BEQ (not equal, no jump)", prog, n, 20);

    n = 0;
    prog[n++] = 0x00100113; // addi x2, x0, 1
    prog[n++] = 0x00500193; // addi x3, x0, 5
    prog[n++] = 0x00314A63; // blt  x2, x3, +20 (skip next)
    prog[n++] = 0x00120213; // addi x4, x4, 1
    prog[n++] = 0x00A00213; // addi x4, x0, 10
    prog[n++] = 0x00000013; // nop
    run_branch_test("BLT (less than, should branch)", prog, n, 20);

    n = 0;
    prog[n++] = 0x00100113; // addi x2, x0, 1
    prog[n++] = 0x00200193; // addi x3, x0, 2
    prog[n++] = 0x00000217; // auipc x4, 0
    prog[n++] = 0x010000EF; // jal  x1, +16
    prog[n++] = 0x00120213; // addi x4, x4, 1
    prog[n++] = 0x00300213; // addi x4, x0, 3
    prog[n++] = 0x00000013; // nop
    run_branch_test("JAL (unconditional jump)", prog, n, 20);

    n = 0;
    uint32_t start_addr = 0x100 / 4;
    for (uint32_t i = 0; i < start_addr; i++) prog[i] = 0x00000013;
    n = start_addr;
    prog[n++] = 0x00A00113; // addi x2, x0, 10
    prog[n++] = 0xFFF10113; // addi x2, x2, -1
    prog[n++] = 0x00211863; // bne  x2, x0, -16
    prog[n++] = 0x00500193; // addi x3, x0, 5
    prog[n++] = 0x00000013; // nop
    run_branch_test("Loop (countdown 10)", prog, n, 80);

    return 0;
}
