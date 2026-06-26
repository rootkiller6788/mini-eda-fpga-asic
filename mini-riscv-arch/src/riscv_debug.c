#include "riscv_debug.h"
#include <stdio.h>
#include <string.h>

void debug_init(DebugModule *dm) {
    memset(dm, 0, sizeof(*dm));
    dm->state = DBG_RUNNING;
    dm->bp_count = 0;
    dm->halted = false;
    dm->resume_ack = false;
}

bool debug_halt(DebugModule *dm, uint32_t current_pc) {
    dm->state = DBG_HALTED;
    dm->halted = true;
    dm->pc = current_pc;
    return true;
}

bool debug_resume(DebugModule *dm) {
    dm->state = DBG_RUNNING;
    dm->halted = false;
    dm->resume_ack = true;
    return true;
}

bool debug_step(DebugModule *dm, uint32_t current_pc) {
    dm->state = DBG_STEPPING;
    dm->halted = true;
    dm->pc = current_pc;
    return true;
}

int debug_set_breakpoint(DebugModule *dm, uint32_t addr, bool is_instr) {
    if (dm->bp_count >= 8) return -1;
    int idx = dm->bp_count++;
    dm->breakpoints[idx].addr = addr;
    dm->breakpoints[idx].enabled = true;
    dm->breakpoints[idx].is_instr = is_instr;
    return idx;
}

bool debug_clear_breakpoint(DebugModule *dm, int idx) {
    if (idx < 0 || idx >= dm->bp_count) return false;
    dm->breakpoints[idx].enabled = false;
    return true;
}

bool debug_hit_breakpoint(DebugModule *dm, uint32_t addr, bool is_instr) {
    for (int i = 0; i < dm->bp_count; i++) {
        if (dm->breakpoints[i].enabled && dm->breakpoints[i].addr == addr) {
            if (dm->breakpoints[i].is_instr == is_instr) return true;
        }
    }
    return false;
}

bool debug_abstract_command(DebugModule *dm, uint32_t cmd, uint32_t arg, uint32_t *result) {
    if (cmd == CMD_ACCESS_REG) {
        uint8_t regno = arg & 0xFF;
        bool is_write = (arg >> 16) & 1;
        if (is_write) {
            uint32_t value = (arg >> 24);
            if (regno < 32) dm->regs[regno] = value;
        } else {
            if (result && regno < 32) *result = dm->regs[regno];
        }
        return true;
    }
    return false;
}

void debug_dtm_transfer(uint32_t addr, uint32_t *data, bool is_write) {
    (void)addr; (void)data; (void)is_write;
}

void debug_print(DebugModule *dm) {
    printf("=== Debug Module ===\n");
    printf("  State: %s\n", dm->state == DBG_RUNNING ? "Running" : dm->state == DBG_HALTED ? "Halted" : "Stepping");
    printf("  PC: 0x%08X\n", dm->pc);
    printf("  Breakpoints: %d\n", dm->bp_count);
    for (int i = 0; i < dm->bp_count; i++) {
        printf("    BP%d: 0x%08X %s %s\n", i, dm->breakpoints[i].addr,
               dm->breakpoints[i].is_instr ? "instr" : "data",
               dm->breakpoints[i].enabled ? "enabled" : "disabled");
    }
}
