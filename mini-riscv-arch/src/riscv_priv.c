#include "riscv_priv.h"
#include <stdio.h>

void priv_init(PrivState *p) {
    p->current_mode = PRIV_M;
    p->trap_pending = false;
    p->trap_cause = 0;
    p->trap_value = 0;
    p->trap_pc = 0;
    p->return_pc = 0;
}

const char *priv_mode_name(PrivMode m) {
    switch (m) { case PRIV_U: return "User"; case PRIV_S: return "Supervisor"; case PRIV_M: return "Machine"; default: return "?"; }
}

bool priv_switch_to(PrivState *p, PrivMode target) {
    if (target > p->current_mode && target != PRIV_M) return false;
    if (target < p->current_mode) return false; /* can only trap upward */
    p->current_mode = target;
    return true;
}

bool priv_can_access(PrivState *p, uint16_t csr_addr) {
    uint8_t min_priv = (csr_addr >> 8) & 0x3;
    return p->current_mode >= (PrivMode)min_priv;
}

void priv_trap(PrivState *p, uint32_t cause, uint32_t tval, uint32_t pc) {
    p->trap_pending = true;
    p->trap_cause = cause;
    p->trap_value = tval;
    p->trap_pc = pc;
    p->return_pc = pc + 4;
    /* Elevate to machine mode on trap */
    if (p->current_mode < PRIV_M) p->current_mode = PRIV_M;
}

bool priv_mret(PrivState *p) {
    if (p->current_mode != PRIV_M) return false;
    if (!p->trap_pending) return false;
    p->trap_pending = false;
    p->current_mode = PRIV_U; /* simplified: return to user mode */
    return true;
}

bool priv_sret(PrivState *p) {
    if (p->current_mode < PRIV_S) return false;
    if (!p->trap_pending) return false;
    p->trap_pending = false;
    return true;
}

void priv_print(PrivState *p) {
    printf("=== Privilege State ===\n");
    printf("  Mode: %s\n", priv_mode_name(p->current_mode));
    if (p->trap_pending) printf("  Trap: cause=0x%X tval=0x%X pc=0x%X\n", p->trap_cause, p->trap_value, p->trap_pc);
    else printf("  No pending trap\n");
}
