#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "riscv_core.h"
#include "riscv_decode.h"
#include "pipeline_hazards.h"
#include "csr_privilege.h"
#include "riscv_verilog.h"

#define MAX_PROG  512
#define MAX_STEPS 1000

static void demo_program_load_and_run(void);
static void demo_verilog_generation(void);
static void demo_memory_operations(void);
static void demo_full_alu_test(void);
static void demo_csr_integration(void);
static void demo_branch_predictor_stats(void);
static void demo_small_bubble_sort(void);
static void print_program_stats(const RiscvCore *core);

int main(void) {
    printf("=========================================================\n");
    printf("  RISC-V RV32I Full Core Demo\n");
    printf("  Complete processor simulation with memory and I/O\n");
    printf("=========================================================\n");

    demo_program_load_and_run();
    demo_memory_operations();
    demo_full_alu_test();
    demo_csr_integration();
    demo_branch_predictor_stats();
    demo_small_bubble_sort();
    demo_verilog_generation();

    printf("\nAll demos complete.\n");
    return 0;
}

static void demo_program_load_and_run(void) {
    printf("\n=== Demo 1: Program Load and Execution ===\n");

    RiscvCore core;
    core_init(&core);

    uint32_t prog[MAX_PROG];
    uint32_t n = 0;

    prog[n++] = 0x00100093; // addi x1, x0, 1
    prog[n++] = 0x00200113; // addi x2, x0, 2
    prog[n++] = 0x00400193; // addi x3, x0, 4
    prog[n++] = 0x002081B3; // add  x3, x1, x2
    prog[n++] = 0x40110233; // sub  x4, x2, x1
    prog[n++] = 0x0010A023; // sw   x1, 0(x1)
    prog[n++] = 0x0000A283; // lw   x5, 0(x1)
    prog[n++] = 0x00000013; // nop

    core_load_program(&core, prog, n);

    printf("Program (%u instructions):\n", n);
    for (uint32_t i = 0; i < n; i++) {
        DecodedInst d = decode_instruction(prog[i]);
        printf("  [%3u] ", i);
        print_instruction(&d, i * 4);
    }

    printf("\nExecuting...\n");
    core_step(&core, 30);

    printf("\nRegister file:\n");
    for (int i = 0; i <= 5; i++) {
        printf("  x%02d (%s) = 0x%08X (%d)\n",
               i,
               i == 0 ? "zero" : i == 1 ? "ra  " : i == 2 ? "sp  " :
               i == 3 ? "gp  " : i == 4 ? "tp  " : "t0  ",
               regfile_read(&core.rf, i),
               regfile_read(&core.rf, i));
    }
    print_program_stats(&core);
}

static void demo_memory_operations(void) {
    printf("\n=== Demo 2: Memory Read/Write ===\n");

    RiscvCore core;
    core_init(&core);

    uint32_t prog[MAX_PROG];
    uint32_t n = 0;

    prog[n++] = 0x06400093; // addi x1, x0, 100
    prog[n++] = 0x0010A023; // sw   x1, 0(x1)
    prog[n++] = 0x0020A223; // sw   x2, 4(x1)
    prog[n++] = 0x0030A423; // sw   x3, 8(x1)
    prog[n++] = 0x0000A103; // lw   x2, 0(x1)
    prog[n++] = 0x0040A183; // lw   x3, 4(x1)
    prog[n++] = 0x0080A203; // lw   x4, 8(x1)
    prog[n++] = 0x0010A023; // sw   x1, 0(x1)
    prog[n++] = 0x00000013; // nop

    core_load_program(&core, prog, n);
    core_step(&core, 40);

    printf("Memory dump (data memory, first 64 bytes):\n");
    for (uint32_t addr = 0; addr < 64; addr += 16) {
        printf("  0x%04X: ", addr);
        for (uint32_t off = 0; off < 16; off++) {
            printf("%02X ", core.dmem.data[addr + off]);
        }
        printf("\n");
    }
    printf("\n");
    printf("x2 = %d, x3 = %d, x4 = %d\n",
           regfile_read(&core.rf, 2),
           regfile_read(&core.rf, 3),
           regfile_read(&core.rf, 4));
}

static void demo_full_alu_test(void) {
    printf("\n=== Demo 3: ALU Operation Test ===\n");

    typedef struct {
        AluOp  op;
        uint32_t a;
        uint32_t b;
        uint32_t expected;
        const char *name;
    } AluTest;

    AluTest tests[] = {
        { ALU_ADD,  10,  20, 30,     "ADD"   },
        { ALU_SUB,  50,  20, 30,     "SUB"   },
        { ALU_SLL,  1,   4,  16,     "SLL"   },
        { ALU_SLT,  5,   10, 1,      "SLT"   },
        { ALU_SLT,  10,  5,  0,      "SLT"   },
        { ALU_SLTU, 5,   10, 1,      "SLTU"  },
        { ALU_XOR,  0xFF,0x0F,0xF0,  "XOR"   },
        { ALU_SRL,  16,  2,  4,      "SRL"   },
        { ALU_SRA,  -16, 2,  -4,     "SRA"   },
        { ALU_OR,   0xF0,0x0F,0xFF,  "OR"    },
        { ALU_AND,  0xFF,0x0F,0x0F,  "AND"   },
        { ALU_PASS_B, 0, 42, 42,     "PASS_B"},
    };

    int passed = 0, failed = 0;
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        uint32_t result = alu_compute(tests[i].op, tests[i].a, tests[i].b);
        int ok = (result == tests[i].expected);
        if (ok) passed++; else failed++;

        printf("  %-7s  a=%-6u b=%-6u  result=%-6u expected=%-6u  %s\n",
               tests[i].name, tests[i].a, tests[i].b,
               result, tests[i].expected,
               ok ? "OK" : "FAIL");
    }
    printf("  ALU tests: %d passed, %d failed\n", passed, failed);
}

static void demo_csr_integration(void) {
    printf("\n=== Demo 4: CSR Integration ===\n");

    RiscvCore core;
    core_init(&core);

    core_dump_csr(&core);

    core.csr_write(&core, 0x300, 0x00001808);
    core.csr_write(&core, 0x305, 0x80000100);
    core.csr_write(&core, 0x341, 0x10000000);

    printf("\nAfter setting CSRs:\n");
    core_dump_csr(&core);

    uint32_t mstatus = core.csr_read(&core, 0x300);
    printf("mstatus readback: 0x%08X %s\n", mstatus,
           mstatus == 0x00001808 ? "OK" : "FAIL");

    core.csr_take_trap(&core, 2, 0x00001000);
    printf("\nAfter taking trap (illegal instruction):\n");
    printf("mcause = 0x%08X (expected 2) %s\n",
           core.mcause,
           core.mcause == 2 ? "OK" : "FAIL");
    printf("mepc   = 0x%08X (expected 0x1000) %s\n",
           core.mepc,
           core.mepc == 0x00001000 ? "OK" : "FAIL");

    core.mepc = 0x20000000;
    core.csr_mret(&core);
    printf("\nAfter MRET:\n");
    printf("PC = 0x%08X (expected 0x20000000) %s\n",
           core.pc,
           core.pc == 0x20000000 ? "OK" : "FAIL");
}

static void demo_branch_predictor_stats(void) {
    printf("\n=== Demo 5: Branch Predictor Statistics ===\n");

    BranchPredictor bp;
    bpred_init(&bp);

    uint32_t target;

    bp.strategy = BPRED_NOT_TAKEN;
    for (int i = 0; i < 10; i++) {
        bpred_predict(&bp, i * 4, &target);
        bpred_update(&bp, i * 4, (i % 2 == 0) ? (i + 1) * 4 : i * 4 + 4,
                     i % 2 == 0);
    }
    printf("Static not-taken: preds=%u mispreds=%u\n",
           bp.pred_count, bp.mispred_count);

    bpred_init(&bp);
    bp.strategy = BPRED_BTB;
    for (int i = 0; i < 20; i++) {
        bpred_predict(&bp, 0x100 + i * 4, &target);
        bpred_update(&bp, 0x100 + i * 4, 0x200 + i * 8, true);
    }
    printf("BTB predictor: preds=%u mispreds=%u hits=%u misses=%u entries=%u\n",
           bp.pred_count, bp.mispred_count,
           bp.btb_hits, bp.btb_misses, bp.btb_entries);
}

static void demo_small_bubble_sort(void) {
    printf("\n=== Demo 6: Bubble Sort on RISC-V ===\n");

    RiscvCore core;
    core_init(&core);

    core.dmem.data[0x100 + 0] = 5;
    core.dmem.data[0x100 + 1] = 0;
    core.dmem.data[0x100 + 4] = 3;
    core.dmem.data[0x100 + 5] = 0;
    core.dmem.data[0x100 + 8] = 1;
    core.dmem.data[0x100 + 9] = 0;
    core.dmem.data[0x100 + 12] = 4;
    core.dmem.data[0x100 + 13] = 0;
    core.dmem.data[0x100 + 16] = 2;
    core.dmem.data[0x100 + 17] = 0;

    printf("Initial array at 0x100: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", core.dmem.data[0x100 + i * 4]);
    }
    printf("\n");

    uint32_t prog[MAX_PROG];
    uint32_t n = 0;

    prog[n++] = 0x00500293; // addi x5, x0, 5
    prog[n++] = 0x10000213; // addi x4, x0, 0x100
    prog[n++] = 0x000281B3; // add  x3, x5, x0

    prog[n++] = 0xFFF18193; // outer: addi x3, x3, -1
    prog[n++] = 0x02019463; // bne  x3, x0, +40

    prog[n++] = 0x00020133; // inner: add  x2, x4, x0
    prog[n++] = 0x000205B3; // add  x1, x4, x0

    prog[n++] = 0x00002283; // lw   x5, 0(x4)
    prog[n++] = 0x00402303; // lw   x6, 4(x4)
    prog[n++] = 0x0062D463; // bge  x5, x6, +8

    prog[n++] = 0x00622023; // sw   x6, 0(x4)
    prog[n++] = 0x00522223; // sw   x5, 4(x4)
    prog[n++] = 0x00420213; // addi x4, x4, 4
    prog[n++] = 0xFE311AE3; // bne  x2, x3, -16
    prog[n++] = 0xFC000013; // nop

    core_load_program(&core, prog, n);
    core_step(&core, 200);

    printf("Sorted array at 0x100: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", core.dmem.data[0x100 + i * 4]);
    }
    printf("\n");
}

static void demo_verilog_generation(void) {
    printf("\n=== Demo 7: Verilog RTL Generation ===\n");

    VerilogConfig cfg;
    verilog_config_default(&cfg);

    verilog_generate_processor("build/riscv_core.v", &cfg);
    verilog_generate_testbench("build/tb_riscv_top.v", &cfg);

    printf("Verilog files written to build/\n");
}

static void print_program_stats(const RiscvCore *core) {
    printf("\nExecution Statistics:\n");
    printf("  Total cycles:    %u\n", core->cycle_count);
    printf("  Instructions:    %u\n", core->inst_count);
    if (core->cycle_count > 0)
        printf("  IPC:             %.2f\n",
               (float)core->inst_count / core->cycle_count);
    printf("  PC:              0x%08X\n", core->pc);
    printf("  Pipeline valid:  IF=%d ID=%d EX=%d MEM=%d WB=%d\n",
           core->if_id.valid,
           core->id_ex.valid,
           core->ex_mem.valid,
           false,
           core->mem_wb.valid);
}
