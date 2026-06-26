#include "riscv_csr.h"
#include "riscv_priv.h"
#include "riscv_except.h"
#include "riscv_mmu.h"
#include "riscv_debug.h"
#include <stdio.h>
#include <stdint.h>

int main(void) {
    printf("=== RISC-V Privileged Architecture Demo ===\n\n");
    /* CSR demo */
    CsrFile csr; csr_init(&csr);
    printf("mstatus: 0x%08X\n", csr_read(&csr, CSR_MSTATUS));
    printf("misa: 0x%08X (RV32IMAC=%s)\n", csr_read(&csr, CSR_MISA), (csr_read(&csr, CSR_MISA) & 0x1F0000) ? "yes" : "no");
    /* Privilege modes */
    PrivState priv; priv_init(&priv);
    printf("\nCurrent mode: %s\n", priv_mode_name(priv.current_mode));
    priv_trap(&priv, EXC_ECALL_U, 0x1000, 0x80000000);
    printf("After trap: cause=%s\n", except_name(priv.trap_cause | MCAUSE_IRQ_FLAG));
    priv_print(&priv);
    /* Interrupt controller */
    InterruptCtrl ic; interrupt_init(&ic);
    interrupt_enable(&ic, IRQ_TIMER_M);
    interrupt_assert(&ic, IRQ_TIMER_M);
    ic.global_enable = true;
    uint32_t taken = interrupt_take(&ic);
    printf("\nTaken IRQ: %u (MTI=%u)\n", taken, IRQ_TIMER_M);
    /* MMU */
    MmuState mmu; mmu_init(&mmu);
    printf("\nMMU mode: %s\n", mmu.mode == SATP_MODE_BARE ? "Bare (no translation)" : "With paging");
    uint32_t pa = mmu_translate(&mmu, 0x80001000, false, false);
    printf("VA 0x80001000 -> PA 0x%08X\n", pa);
    /* Debug module */
    DebugModule dm; debug_init(&dm);
    debug_halt(&dm, 0x80000000);
    debug_set_breakpoint(&dm, 0x80000100, true);
    printf("\nDebug: %s at PC=0x%08X, BP hit=%s\n", dm.state == DBG_HALTED ? "Halted" : "Running", dm.pc, debug_hit_breakpoint(&dm, 0x80000100, true) ? "yes" : "no");
    csr_print(&csr);
    return 0;
}
