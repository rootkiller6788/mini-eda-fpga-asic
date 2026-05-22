#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "riscv_core.h"
#include "riscv_decode.h"
#include "csr_privilege.h"

static void test_csr_basics(void);
static void test_exception_trap(void);
static void test_timer_interrupt(void);
static void test_ecall_mret(void);

int main(void) {
    printf("========================================\n");
    printf("  RISC-V CSR & Privilege Mode Tests\n");
    printf("========================================\n\n");

    test_csr_basics();
    test_exception_trap();
    test_timer_interrupt();
    test_ecall_mret();

    printf("All CSR tests passed.\n");
    return 0;
}

static void test_csr_basics(void) {
    printf("--- Test 1: CSR Basics ---\n");

    CsrFile csr;
    csr_init(&csr);

    printf("Initial state:\n");
    csr_dump(&csr);
    printf("\n");

    csr_write32(&csr, CSR_MSCRATCH, 0xDEADBEEF);
    uint32_t val = csr_read32(&csr, CSR_MSCRATCH);
    printf("mscratch write 0xDEADBEEF -> read 0x%08X %s\n", val,
           val == 0xDEADBEEF ? "OK" : "FAIL");

    csr_write32(&csr, CSR_MSTATUS, 0x00001808);
    printf("mstatus set MIE=1: %s\n",
           csr.mstatus_mie ? "OK" : "FAIL");

    csr_write32(&csr, CSR_MTVEC, 0x80000004);
    printf("mtvec set vectored 0x80000004: mode=%s %s\n",
           csr.mtvec_mode_vectored ? "vectored" : "direct",
           csr.mtvec_mode_vectored ? "OK" : "FAIL");

    csr_write32(&csr, CSR_MEPC, 0x10001000);
    printf("mepc write 0x10001000: %s\n",
           csr.mepc == 0x10001000 ? "OK" : "FAIL");

    csr_write32(&csr, CSR_MCAUSE, 0x80000007);
    printf("mcause write 0x80000007: %s\n",
           csr.mcause == 0x80000007 ? "OK" : "FAIL");

    printf("\nFinal state:\n");
    csr_dump(&csr);
    printf("\n");
}

static void test_exception_trap(void) {
    printf("--- Test 2: Exception Trap ---\n");

    CsrFile csr;
    csr_init(&csr);
    csr.mstatus_mie = true;
    csr.mstatus |= MSTATUS_MIE;
    csr.mtvec = 0x80000000;

    csr_take_exception(&csr, TRAP_ILLEGAL_INSTRUCTION, 0x00001004, 0xDEADBEEF);

    printf("After illegal instruction trap:\n");
    printf("  mepc   = 0x%08X (expected 0x00001004) %s\n",
           csr.mepc,
           csr.mepc == 0x00001004 ? "OK" : "FAIL");
    printf("  mtval  = 0x%08X (expected 0xDEADBEEF) %s\n",
           csr.mtval,
           csr.mtval == 0xDEADBEEF ? "OK" : "FAIL");
    printf("  mcause = 0x%08X (expected 0x%08X) %s\n",
           csr.mcause,
           (uint32_t)TRAP_ILLEGAL_INSTRUCTION,
           csr.mcause == (uint32_t)TRAP_ILLEGAL_INSTRUCTION ? "OK" : "FAIL");
    printf("  MIE = 0 (was 1) %s\n",
           csr.mstatus_mie ? "FAIL" : "OK");
    printf("  MPIE = 1 (saved MIE) %s\n",
           csr.mstatus_mpie ? "OK" : "FAIL");
    printf("\n");
}

static bool timer_fired = false;

static bool mock_timer_cb(void) {
    return timer_fired;
}

static void test_timer_interrupt(void) {
    printf("--- Test 3: Timer Interrupt ---\n");

    CsrFile csr;
    csr_init(&csr);
    csr.mstatus_mie = true;
    csr.mstatus |= MSTATUS_MIE;
    csr.mie = 0x80;
    csr.mip = 0x80;

    csr_set_timer_callback(&csr, mock_timer_cb);

    timer_fired = false;
    bool pending = csr_timer_interrupt_pending(&csr);
    printf("Timer interrupt (callback=false, MIE=1, MIP=1, MIE_EN=1): %s %s\n",
           pending ? "pending" : "not pending",
           pending ? "OK" : "FAIL - should be pending via MIP");

    timer_fired = true;
    pending = csr_timer_interrupt_pending(&csr);
    printf("Timer interrupt (callback=true): %s %s\n",
           pending ? "pending" : "not pending",
           pending ? "OK" : "FAIL");

    csr_tick_timer(&csr);
    csr_tick_timer(&csr);
    printf("mtime after 2 ticks: %llu %s\n",
           (unsigned long long)csr.mtime,
           csr.mtime == 2 ? "OK" : "FAIL");
    printf("mcycle after 2 ticks: %llu %s\n",
           (unsigned long long)csr.mcycle,
           csr.mcycle == 2 ? "OK" : "FAIL");

    csr_take_interrupt(&csr, (InterruptCode)0x80000007, 0x00001000);
    printf("After timer interrupt trap:\n");
    printf("  mepc   = 0x%08X %s\n", csr.mepc,
           csr.mepc == 0x00001000 ? "OK" : "FAIL");
    printf("  mcause = 0x%08X %s\n", csr.mcause,
           csr.mcause == 0x80000007 ? "OK" : "FAIL");
    printf("\n");
}

static void test_ecall_mret(void) {
    printf("--- Test 4: ECALL / MRET ---\n");

    CsrFile csr;
    csr_init(&csr);
    csr.mstatus_mie = true;
    csr.mstatus |= MSTATUS_MIE;
    csr.priv = PRIV_MACHINE;

    csr_handle_ecall(&csr, 0x00002000);

    printf("After ECALL from M-mode:\n");
    printf("  mcause = 0x%08X (expected ECALL_M=11) %s\n",
           csr.mcause,
           csr.mcause == (uint32_t)TRAP_ECALL_M ? "OK" : "FAIL");
    printf("  mepc   = 0x%08X %s\n", csr.mepc,
           csr.mepc == 0x00002000 ? "OK" : "FAIL");
    printf("  MIE   = %d (should be 0) %s\n", csr.mstatus_mie,
           csr.mstatus_mie ? "FAIL" : "OK");

    csr.mepc = 0x10000000;
    csr_mret(&csr);

    printf("After MRET:\n");
    printf("  MIE   = %d (should be 1, restored from MPIE) %s\n",
           csr.mstatus_mie,
           csr.mstatus_mie ? "OK" : "FAIL");
    printf("  MPIE  = %d (should be 1) %s\n",
           csr.mstatus_mpie,
           csr.mstatus_mpie ? "OK" : "FAIL");

    printf("\nCSR Names:\n");
    uint16_t csr_addrs[] = {
        CSR_MSTATUS, CSR_MISA, CSR_MIE, CSR_MTVEC,
        CSR_MEPC, CSR_MCAUSE, CSR_MTVAL, CSR_MSCRATCH
    };
    for (int i = 0; i < 8; i++) {
        printf("  0x%03X -> %s\n", csr_addrs[i], csr_name(csr_addrs[i]));
    }

    printf("\nTrap Names:\n");
    TrapCause traps[] = {
        TRAP_ILLEGAL_INSTRUCTION, TRAP_ECALL_M,
        TRAP_BREAKPOINT, TRAP_LOAD_ACCESS_FAULT, TRAP_STORE_ACCESS_FAULT
    };
    for (int i = 0; i < 5; i++) {
        printf("  %d -> %s\n", traps[i], trap_name(traps[i]));
    }
    printf("\n");
}
