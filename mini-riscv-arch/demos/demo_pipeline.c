#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "riscv_core.h"
#include "riscv_decode.h"
#include "pipeline_hazards.h"
#include "csr_privilege.h"

#define MAX_PROGRAM  256
#define MAX_CYCLES   200

static void print_banner(void);
static void demo_forwarding(void);
static void demo_load_use_stall(void);
static void demo_branch_mispredict(void);
static void demo_pipeline_visual(bool show_forwarding);
static void demo_pipeline_timeline(void);
static void program_disasm(const uint32_t *prog, uint32_t len);
static void run_cycles(RiscvCore *core, uint32_t cycles, bool verbose);

int main(void) {
    print_banner();

    printf("\n=== Demo 1: Data Forwarding (EX/MEM -> EX) ===\n");
    demo_forwarding();

    printf("\n=== Demo 2: Load-Use Hazard (Stall) ===\n");
    demo_load_use_stall();

    printf("\n=== Demo 3: Branch Mispredict ===\n");
    demo_branch_mispredict();

    printf("\n=== Demo 4: Pipeline Timeline Visualization ===\n");
    demo_pipeline_timeline();

    printf("\n=== Demo 5: Full Pipeline Visual ===\n");
    demo_pipeline_visual(true);

    printf("\nPipeline hazard demo complete.\n");
    return 0;
}

static void print_banner(void) {
    printf("=================================================\n");
    printf("  RISC-V 5-Stage Pipeline Hazard Demo\n");
    printf("  Forwarding | Stalling | Flushing | Prediction\n");
    printf("=================================================\n");
}

static void program_disasm(const uint32_t *prog, uint32_t len) {
    printf("Program disassembly:\n");
    for (uint32_t i = 0; i < len; i++) {
        DecodedInst d = decode_instruction(prog[i]);
        printf("  PC=%03X:  ", i * 4);
        print_instruction(&d, i * 4);
    }
    printf("\n");
}

static void run_cycles(RiscvCore *core, uint32_t cycles, bool verbose) {
    for (uint32_t c = 0; c < cycles; c++) {
        if (verbose) {
            printf("[%3u] ", core->cycle_count);
            if (core->if_id.valid) printf("IF:%08X ", core->if_id.pc);
            else printf("IF:-------- ");
            if (core->id_ex.valid) printf("ID:%s ",
                instr_name(decode_instruction(core->id_ex.inst).instr));
            else printf("ID:-------- ");
            if (core->ex_mem.valid) printf("EX:%08X ", core->ex_mem.alu_result);
            else printf("EX:-------- ");
            if (core->mem_wb.valid) printf("MEM/WB:WB(x%d=%08X)",
                core->mem_wb.rd, core->mem_wb.alu_result);
            printf("\n");
        }
        core_tick(core);
    }
}

static void demo_forwarding(void) {
    RiscvCore core;
    core_init(&core);

    uint32_t prog[MAX_PROGRAM];
    uint32_t n = 0;

    prog[n++] = 0x00100093; // addi x1, x0, 1
    prog[n++] = 0x00208133; // add  x2, x1, x2
    prog[n++] = 0x402081B3; // sub  x3, x1, x2
    prog[n++] = 0x00500093; // addi x1, x0, 5
    prog[n++] = 0x000081B3; // add  x3, x1, x0
    prog[n++] = 0x00000013; // nop

    core_load_program(&core, prog, n);
    program_disasm(prog, n);

    printf("Running with forwarding (RAW hazards resolved via EX/MEM->EX):\n");
    for (uint32_t c = 0; c < 30; c++) {
        HazardInfo hz;
        hazard_unit_init(&hz);
        hazard_detect(&core, &hz);

        if (hz.type != HAZ_NONE) {
            printf("  [cycle %2u] ", c);
            switch (hz.type) {
            case HAZ_RAW_RS1:
                printf("RAW on RS1 -> forwarded from %s\n",
                    hz.rs1_fwd == FWD_SRC_EX_MEM ? "EX/MEM" :
                    hz.rs1_fwd == FWD_SRC_MEM_WB ? "MEM/WB" : "none");
                break;
            case HAZ_RAW_RS2:
                printf("RAW on RS2 -> forwarded from %s\n",
                    hz.rs2_fwd == FWD_SRC_EX_MEM ? "EX/MEM" :
                    hz.rs2_fwd == FWD_SRC_MEM_WB ? "MEM/WB" : "none");
                break;
            default: printf("Other hazard\n"); break;
            }
            hazard_resolve(&core, &hz);
        }
        core_tick(&core);
    }

    printf("\nFinal registers:\n");
    for (int i = 0; i <= 3; i++)
        printf("  x%d = 0x%08X (%d)\n", i, regfile_read(&core.rf, i),
               regfile_read(&core.rf, i));
}

static void demo_load_use_stall(void) {
    RiscvCore core;
    core_init(&core);

    uint32_t prog[MAX_PROGRAM];
    uint32_t n = 0;

    prog[n++] = 0x00100093; // addi x1, x0, 1
    prog[n++] = 0x0010A023; // sw   x1, 0(x1)
    prog[n++] = 0x0000A083; // lw   x1, 0(x1)
    prog[n++] = 0x00108133; // add  x2, x1, x1 (RAW on x1 from lw)
    prog[n++] = 0x00000013; // nop

    core_load_program(&core, prog, n);
    program_disasm(prog, n);

    printf("Running with load-use detection (stall expected after lw):\n");
    uint32_t stall_count = 0;
    for (uint32_t c = 0; c < 30; c++) {
        HazardInfo hz;
        hazard_unit_init(&hz);
        hazard_detect(&core, &hz);

        if (hz.type == HAZ_LOAD_USE) {
            printf("  [cycle %2u] LOAD-USE hazard detected! Stalling...\n", c);
            stall_count++;
            hazard_resolve(&core, &hz);
        }
        core_tick(&core);
    }

    printf("Load-use stalls occurred: %u\n", stall_count);
    printf("Final registers:\n");
    printf("  x1 = 0x%08X\n", regfile_read(&core.rf, 1));
    printf("  x2 = 0x%08X\n", regfile_read(&core.rf, 2));
}

static void demo_branch_mispredict(void) {
    RiscvCore core;
    core_init(&core);

    BranchPredictor bp;
    bpred_init(&bp);

    uint32_t prog[MAX_PROGRAM];
    uint32_t n = 0;

    prog[n++] = 0x00100093; // addi x1, x0, 1
    prog[n++] = 0x00100113; // addi x2, x0, 1
    prog[n++] = 0x00209A63; // bne  x1, x2, +20 (should NOT be taken)
    prog[n++] = 0x00A00213; // addi x4, x0, 10
    prog[n++] = 0x00B00293; // addi x5, x0, 11
    prog[n++] = 0x00309A63; // bne  x1, x1, +20 (should NOT be taken - same reg)
    prog[n++] = 0x00C00313; // addi x6, x0, 12
    prog[n++] = 0x00000013; // nop

    core_load_program(&core, prog, n);
    program_disasm(prog, n);

    printf("Branch predictor: static not-taken\n");
    printf("  Prediction counts: total=%u mispredicts=%u\n",
           bp.pred_count, bp.mispred_count);

    uint32_t mispredict_corrects = 0;
    for (uint32_t c = 0; c < 30; c++) {
        HazardInfo hz;
        hazard_unit_init(&hz);
        hazard_detect(&core, &hz);

        if (hz.type == HAZ_CONTROL) {
            printf("  [cycle %2u] Branch resolved: %s (predicted not-taken)\n",
                   c, hz.branch_actual_taken ? "TAKEN" : "NOT TAKEN");
            if (hz.branch_actual_taken) mispredict_corrects++;
            hazard_resolve(&core, &hz);
        }
        core_tick(&core);
    }

    printf("Branch mispredict corrections: %u\n", mispredict_corrects);
}

static void demo_pipeline_visual(bool show_forwarding) {
    RiscvCore core;
    core_init(&core);

    uint32_t prog[MAX_PROGRAM];
    uint32_t n = 0;

    prog[n++] = 0x00500093; // addi x1, x0, 5
    prog[n++] = 0x00A00113; // addi x2, x0, 10
    prog[n++] = 0x002081B3; // add  x3, x1, x2
    prog[n++] = 0x40210233; // sub  x4, x2, x1
    prog[n++] = 0x0010A023; // sw   x1, 0(x1)
    prog[n++] = 0x0000A183; // lw   x3, 0(x1) (load-use: x3 used next)
    prog[n++] = 0x003201B3; // add  x3, x4, x3
    prog[n++] = 0x00000013; // nop

    core_load_program(&core, prog, n);

    printf("Pipeline stage utilization (F=IF,D=ID,X=EX,M=MEM,W=WB,_=bubble):\n");
    printf("Cycle  F D X M W  | Comment\n");
    printf("------ - - - - -  + -------\n");

    for (uint32_t c = 0; c < 20; c++) {
        char stages[] = {'_', '_', '_', '_', '_', '\0'};

        if (core.if_id.valid) stages[0] = 'F';
        if (core.id_ex.valid) stages[1] = 'D';
        if (core.ex_mem.valid)stages[2] = 'X';
        if (core.mem_wb.valid)stages[3] = 'M';

        static uint32_t last_wb = 0xDEAD0000;
        static uint8_t  last_rd = 255;
        if (core.mem_wb.valid && core.mem_wb.wb_en &&
            core.mem_wb.alu_result != last_wb) {
            stages[4] = 'W';
            last_wb = core.mem_wb.alu_result;
            last_rd = core.mem_wb.rd;
        }

        printf(" %3u    %c %c %c %c %c  ",
               c, stages[0], stages[1], stages[2], stages[3], stages[4]);

        if (stages[2] == 'X' && core.ex_mem.valid) {
            DecodedInst d = decode_instruction(core.id_ex.inst);
            printf("| EX: %s alu=0x%08X", d.mnemonic, core.ex_mem.alu_result);
        }
        if (stages[4] == 'W' && last_rd != 255) {
            printf("| WB: x%d=0x%08X", last_rd, last_wb);
        }
        printf("\n");

        HazardInfo hz;
        hazard_unit_init(&hz);
        if (show_forwarding) {
            hazard_detect(&core, &hz);
            if (hz.type != HAZ_NONE) {
                hazard_resolve(&core, &hz);
            }
        }
        core_tick(&core);
    }

    printf("\nFinal register state:\n");
    for (int i = 0; i < 4; i++)
        printf("  x%d = %d (0x%08X)\n", i,
               regfile_read(&core.rf, i), regfile_read(&core.rf, i));
}

static void demo_pipeline_timeline(void) {
    RiscvCore core;
    core_init(&core);

    uint32_t prog[MAX_PROGRAM];
    uint32_t n = 0;

    prog[n++] = 0x00300093; // addi x1, x0, 3
    prog[n++] = 0x00400113; // addi x2, x0, 4
    prog[n++] = 0x002081B3; // add  x3, x1, x2
    prog[n++] = 0x40110233; // sub  x4, x2, x1
    prog[n++] = 0x0030A023; // sw   x3, 0(x1)
    prog[n++] = 0x0000A303; // lw   x6, 0(x1)
    prog[n++] = 0x001321B3; // slt  x3, x6, x1
    prog[n++] = 0x00000013; // nop

    core_load_program(&core, prog, n);

    printf("Timeline: Instruction flow through pipeline stages\n\n");
    printf("Inst#  Mnemonic     IF         ID         EX         MEM        WB\n");
    printf("-----  ---------    --------- --------- --------- --------- ---------\n");

    typedef struct {
        int32_t if_cycle, id_cycle, ex_cycle, mem_cycle, wb_cycle;
    } InstTiming;

    InstTiming timing[8] = {0};
    for (int i = 0; i < 8; i++) {
        timing[i].if_cycle = timing[i].id_cycle = timing[i].ex_cycle = -1;
        timing[i].mem_cycle = timing[i].wb_cycle = -1;
    }

    for (uint32_t c = 0; c < 25; c++) {
        uint32_t if_pc = core.if_id.valid ? core.if_id.pc / 4 : (uint32_t)-1;
        uint32_t id_pc = core.id_ex.valid ? core.id_ex.pc / 4 : (uint32_t)-1;
        uint32_t ex_pc = core.ex_mem.valid ? core.ex_mem.pc / 4 : (uint32_t)-1;
        uint32_t mem_pc = core.mem_wb.valid ? core.mem_wb.pc / 4 : (uint32_t)-1;

        if (if_pc < 8 && timing[if_pc].if_cycle == -1) timing[if_pc].if_cycle = c;
        if (id_pc < 8 && timing[id_pc].id_cycle == -1) timing[id_pc].id_cycle = c;
        if (ex_pc < 8 && timing[ex_pc].ex_cycle == -1) timing[ex_pc].ex_cycle = c;
        if (mem_pc < 8 && timing[mem_pc].mem_cycle == -1) timing[mem_pc].mem_cycle = c;
        if (core.mem_wb.valid && core.mem_wb.wb_en && core.mem_wb.pc / 4 < 8) {
            uint32_t wb_pc = core.mem_wb.pc / 4;
            if (timing[wb_pc].wb_cycle == -1) timing[wb_pc].wb_cycle = c;
        }

        core_tick(&core);
    }

    for (int i = 0; i < 8; i++) {
        DecodedInst d = decode_instruction(prog[i]);
        printf("  %-2d    %-10s ", i, d.mnemonic);

        for (int stage = 0; stage < 5; stage++) {
            int32_t t = -1;
            switch (stage) {
            case 0: t = timing[i].if_cycle; break;
            case 1: t = timing[i].id_cycle; break;
            case 2: t = timing[i].ex_cycle; break;
            case 3: t = timing[i].mem_cycle; break;
            case 4: t = timing[i].wb_cycle; break;
            }
            if (t >= 0) printf("%-10d", t);
            else        printf("---       ");
            printf(" ");
        }
        printf("\n");
    }
}

static void demo_pipeline_diagram(RiscvCore *core) {
    const char *headers[] = {"IF", "ID", "EX", "MEM", "WB"};

    for (int stage = 0; stage < 5; stage++) {
        printf("[%s] ", headers[stage]);
        bool active = false;
        switch (stage) {
        case 0: active = core->if_id.valid; break;
        case 1: active = core->id_ex.valid; break;
        case 2: active = core->ex_mem.valid; break;
        case 3: active = core->mem_wb.valid; break;
        case 4: active = core->mem_wb.valid; break;
        }
        printf("%s ", active ? "ACTIVE" : "EMPTY");
        if (active) {
            switch (stage) {
            case 0: printf("pc=0x%08X", core->if_id.pc); break;
            case 1: printf("inst=%s", instr_name(decode_instruction(core->id_ex.inst).instr)); break;
            case 2: printf("alu=0x%08X", core->ex_mem.alu_result); break;
            case 3: printf("addr=0x%08X", core->mem_wb.alu_result); break;
            case 4: printf("x%d=0x%08X", core->mem_wb.rd, core->mem_wb.alu_result); break;
            }
        }
        printf("\n");
    }
}
