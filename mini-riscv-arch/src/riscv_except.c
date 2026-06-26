#include "riscv_except.h"
#include "riscv_csr.h"
#include <stdio.h>

const char *except_name(uint32_t cause) {
    bool is_irq = cause & MCAUSE_IRQ_FLAG;
    uint32_t code = cause & MCAUSE_CODE_MASK;
    if (is_irq) {
        switch (code) { case 1: return "Supervisor Software IRQ"; case 3: return "Machine Software IRQ"; case 5: return "Supervisor Timer IRQ"; case 7: return "Machine Timer IRQ"; case 9: return "Supervisor External IRQ"; case 11: return "Machine External IRQ"; default: return "Unknown IRQ"; }
    }
    switch (code) { case 0: return "Instruction Address Misaligned"; case 1: return "Instruction Access Fault"; case 2: return "Illegal Instruction"; case 3: return "Breakpoint"; case 4: return "Load Address Misaligned"; case 5: return "Load Access Fault"; case 6: return "Store Address Misaligned"; case 7: return "Store Access Fault"; case 8: return "ECALL from U-mode"; case 9: return "ECALL from S-mode"; case 11: return "ECALL from M-mode"; case 12: return "Instruction Page Fault"; case 13: return "Load Page Fault"; case 15: return "Store Page Fault"; default: return "Unknown Exception"; }
}

bool except_raise(uint32_t cause, uint32_t tval, uint32_t pc, void (*handler)(uint32_t, uint32_t, uint32_t)) {
    if (handler) { handler(cause, tval, pc); return true; }
    return false;
}

void interrupt_init(InterruptCtrl *ic) {
    ic->pending_irqs = 0;
    ic->enabled_irqs = 0;
    ic->global_enable = false;
}

bool interrupt_enable(InterruptCtrl *ic, uint32_t irq_id) {
    if (irq_id > 31) return false;
    ic->enabled_irqs |= (1U << irq_id);
    return true;
}

bool interrupt_disable(InterruptCtrl *ic, uint32_t irq_id) {
    if (irq_id > 31) return false;
    ic->enabled_irqs &= ~(1U << irq_id);
    return true;
}

bool interrupt_pending(InterruptCtrl *ic, uint32_t irq_id) {
    return (ic->pending_irqs & (1U << irq_id)) != 0;
}

void interrupt_assert(InterruptCtrl *ic, uint32_t irq_id) {
    if (irq_id <= 31) ic->pending_irqs |= (1U << irq_id);
}

void interrupt_clear(InterruptCtrl *ic, uint32_t irq_id) {
    if (irq_id <= 31) ic->pending_irqs &= ~(1U << irq_id);
}

uint32_t interrupt_take(InterruptCtrl *ic) {
    if (!ic->global_enable) return 32;
    uint32_t candidates = ic->pending_irqs & ic->enabled_irqs;
    if (candidates == 0) return 32;
    for (uint32_t i = 0; i < 32; i++)
        if (candidates & (1U << i)) { ic->pending_irqs &= ~(1U << i); return i; }
    return 32;
}

void interrupt_print(InterruptCtrl *ic) {
    printf("=== Interrupt Controller ===\n");
    printf("  Global: %s\n", ic->global_enable ? "enabled" : "disabled");
    printf("  Pending: 0x%08X  Enabled: 0x%08X\n", ic->pending_irqs, ic->enabled_irqs);
}
