#include "riscv_csr.h"
#include "riscv_priv.h"
#include "riscv_except.h"
#include "riscv_mmu.h"
#include "riscv_debug.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int test_csr(void) { CsrFile c; csr_init(&c); csr_write(&c, CSR_MCAUSE, 7); assert(csr_read(&c, CSR_MCAUSE) == 7); printf("  PASS: CSR r/w\n"); return 0; }
static int test_priv(void) { PrivState p; priv_init(&p); assert(p.current_mode == PRIV_M); priv_trap(&p, 8, 0, 0x1000); assert(p.trap_pending); assert(priv_mret(&p)); printf("  PASS: privilege trap+mret\n"); return 0; }
static int test_except(void) { const char *n = except_name(2); assert(strstr(n, "Illegal")); printf("  PASS: exception naming\n"); return 0; }
static int test_irq(void) { InterruptCtrl ic; interrupt_init(&ic); interrupt_enable(&ic, 7); interrupt_assert(&ic, 7); ic.global_enable = true; assert(interrupt_pending(&ic, 7)); uint32_t t = interrupt_take(&ic); assert(t == 7); printf("  PASS: interrupt handling\n"); return 0; }
static int test_mmu(void) { MmuState m; mmu_init(&m); uint32_t pa = mmu_translate(&m, 0x1000, false, false); assert(pa == 0x1000); printf("  PASS: MMU bare mode\n"); return 0; }
static int test_debug(void) { DebugModule d; debug_init(&d); debug_halt(&d, 0x8000); assert(d.state == DBG_HALTED); debug_resume(&d); assert(d.state == DBG_RUNNING); printf("  PASS: debug halt/resume\n"); return 0; }

int main(void) {
    printf("=== RISC-V Architecture Tests ===\n");
    int p = 0, f = 0;
    #define R(t) do { if (t() == 0) p++; else { printf("  FAIL: %s\n", #t); f++; } } while(0)
    R(test_csr); R(test_priv); R(test_except); R(test_irq); R(test_mmu); R(test_debug);
    printf("\nResult: %d passed, %d failed\n", p, f);
    return f > 0 ? 1 : 0;
}
