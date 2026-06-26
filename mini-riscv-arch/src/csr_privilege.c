#include <stdio.h>
#include <string.h>
#include "csr_privilege.h"

void csr_init(CsrFile *csr) {
    memset(csr, 0, sizeof(CsrFile));
    csr->mstatus = MSTATUS_MPP_M;
    csr->misa    = 0x40001100;
    csr->mtvec   = 0x00000000;
    csr->mie     = 0x00000000;
    csr->mip     = 0x00000000;
    csr->priv    = PRIV_MACHINE;
    csr->mstatus_mie  = false;
    csr->mstatus_mpie = true;
    csr->mstatus_mpp  = PRIV_MACHINE;
    csr->mtvec_mode_direct = true;
    csr->mtvec_mode_vectored = false;
    csr->mcycle = 0;
    csr->mtime  = 0;
    csr->mtimecmp = 0;
    csr->timer_callback = NULL;
}

void csr_reset(CsrFile *csr) {
    csr_init(csr);
}

uint32_t csr_read32(const CsrFile *csr, uint16_t addr) {
    switch (addr) {
    case CSR_MSTATUS:  return csr->mstatus;
    case CSR_MISA:     return csr->misa;
    case CSR_MIE:      return csr->mie;
    case CSR_MTVEC:    return csr->mtvec;
    case CSR_MSCRATCH: return csr->mscratch;
    case CSR_MEPC:     return csr->mepc;
    case CSR_MCAUSE:   return csr->mcause;
    case CSR_MTVAL:    return csr->mtval;
    case CSR_MIP:      return csr->mip;
    case CSR_MCYCLE:   return (uint32_t)(csr->mcycle & 0xFFFFFFFF);
    case 0xB80:        return (uint32_t)(csr->mcycle >> 32);
    case CSR_MTIME:    return (uint32_t)(csr->mtime & 0xFFFFFFFF);
    case 0xB81:        return (uint32_t)(csr->mtime >> 32);
    case CSR_MTIMECMP: return (uint32_t)(csr->mtimecmp & 0xFFFFFFFF);
    case 0xB83:        return (uint32_t)(csr->mtimecmp >> 32);
    default:           return 0;
    }
}

void csr_write32(CsrFile *csr, uint16_t addr, uint32_t val) {
    switch (addr) {
    case CSR_MSTATUS:
        csr_set_mstatus(csr, val);
        break;
    case CSR_MIE:
        csr->mie = val;
        break;
    case CSR_MTVEC:
        csr->mtvec = val & 0xFFFFFFFC;
        csr->mtvec_mode_direct   = (val & 0x2) == 0;
        csr->mtvec_mode_vectored = (val & 0x2) != 0;
        break;
    case CSR_MEPC:
        csr->mepc = val;
        break;
    case CSR_MCAUSE:
        csr->mcause = val;
        break;
    case CSR_MTVAL:
        csr->mtval = val;
        break;
    case CSR_MSCRATCH:
        csr->mscratch = val;
        break;
    case CSR_MCYCLE:
        csr->mcycle = (csr->mcycle & 0xFFFFFFFF00000000ULL) | val;
        break;
    case 0xB80:
        csr->mcycle = (csr->mcycle & 0xFFFFFFFFULL) | ((uint64_t)val << 32);
        break;
    case CSR_MTIME:
        csr->mtime = (csr->mtime & 0xFFFFFFFF00000000ULL) | val;
        break;
    case 0xB81:
        csr->mtime = (csr->mtime & 0xFFFFFFFFULL) | ((uint64_t)val << 32);
        break;
    case CSR_MTIMECMP:
        csr->mtimecmp = (csr->mtimecmp & 0xFFFFFFFF00000000ULL) | val;
        break;
    case 0xB83:
        csr->mtimecmp = (csr->mtimecmp & 0xFFFFFFFFULL) | ((uint64_t)val << 32);
        break;
    default:
        break;
    }
}

void csr_set_mstatus(CsrFile *csr, uint32_t val) {
    csr->mstatus = val;
    csr->mstatus_mie  = (val & MSTATUS_MIE) != 0;
    csr->mstatus_mpie = (val & MSTATUS_MPIE) != 0;
    csr->mstatus_mpp  = (val >> 11) & 0x3;
}

uint32_t csr_get_mstatus(const CsrFile *csr) {
    return csr->mstatus;
}

void csr_take_exception(CsrFile *csr, TrapCause cause, uint32_t epc,
                        uint32_t tval) {
    csr->mcause = (uint32_t)cause;
    csr->mepc   = epc;
    csr->mtval  = tval;

    csr->mstatus_mpie = csr->mstatus_mie;
    csr->mstatus_mie  = false;
    csr->mstatus_mpp  = (uint8_t)csr->priv;
    csr->priv = PRIV_MACHINE;

    csr->mstatus &= ~MSTATUS_MIE;
    csr->mstatus &= ~MSTATUS_MPIE;
    if (csr->mstatus_mpie) csr->mstatus |= MSTATUS_MPIE;
    csr->mstatus = (csr->mstatus & ~MSTATUS_MPP_M)
                 | ((uint32_t)csr->mstatus_mpp << 11);
}

void csr_take_interrupt(CsrFile *csr, InterruptCode code, uint32_t epc) {
    if (!csr->mstatus_mie) return;

    csr->mcause = (uint32_t)code;
    csr->mepc   = epc;

    csr->mstatus_mpie = csr->mstatus_mie;
    csr->mstatus_mie  = false;
    csr->mstatus_mpp  = (uint8_t)csr->priv;
    csr->priv = PRIV_MACHINE;

    csr->mstatus &= ~MSTATUS_MIE;
    if (csr->mstatus_mpie) csr->mstatus |= MSTATUS_MPIE;
    csr->mstatus = (csr->mstatus & ~MSTATUS_MPP_M)
                 | ((uint32_t)csr->mstatus_mpp << 11);
}

void csr_mret(CsrFile *csr) {
    csr->mstatus_mie  = csr->mstatus_mpie;
    csr->mstatus_mpie = true;
    csr->priv = (PrivilegeMode)csr->mstatus_mpp;
    csr->mstatus_mpp = PRIV_USER;

    csr->mstatus &= ~MSTATUS_MPP_M;
    csr->mstatus |= MSTATUS_MPIE;
    if (csr->mstatus_mie) csr->mstatus |= MSTATUS_MIE;
}

void csr_handle_ecall(CsrFile *csr, uint32_t epc) {
    TrapCause cause;
    switch (csr->priv) {
    case PRIV_USER:       cause = TRAP_ECALL_U; break;
    case PRIV_SUPERVISOR: cause = TRAP_ECALL_S; break;
    default:              cause = TRAP_ECALL_M; break;
    }
    csr_take_exception(csr, cause, epc, 0);
}

uint32_t csr_trap_vector(const CsrFile *csr, bool is_interrupt) {
    if (csr->mtvec_mode_vectored && is_interrupt) {
        uint32_t code = csr->mcause & 0x7FFFFFFF;
        return (csr->mtvec & 0xFFFFFFFC) + (code * 4);
    }
    return csr->mtvec & 0xFFFFFFFC;
}

void csr_tick_timer(CsrFile *csr) {
    csr->mtime++;
    csr->mcycle++;
}

bool csr_timer_interrupt_pending(const CsrFile *csr) {
    if (csr->timer_callback && csr->timer_callback())
        return true;
    return (csr->mie & 0x80) && (csr->mip & 0x80) && csr->mstatus_mie;
}

void csr_set_timer_callback(CsrFile *csr, bool (*cb)(void)) {
    csr->timer_callback = cb;
}

const char *csr_name(uint16_t addr) {
    switch (addr) {
    case CSR_MSTATUS:  return "mstatus";
    case CSR_MISA:     return "misa";
    case CSR_MIE:      return "mie";
    case CSR_MTVEC:    return "mtvec";
    case CSR_MSCRATCH: return "mscratch";
    case CSR_MEPC:     return "mepc";
    case CSR_MCAUSE:   return "mcause";
    case CSR_MTVAL:    return "mtval";
    case CSR_MIP:      return "mip";
    case CSR_MCYCLE:   return "mcycle";
    case CSR_MTIME:    return "mtime";
    case CSR_MTIMECMP: return "mtimecmp";
    default:           return "unknown";
    }
}

const char *trap_name(TrapCause cause) {
    switch (cause) {
    case TRAP_INSTR_ADDR_MISALIGNED: return "instruction address misaligned";
    case TRAP_INSTR_ACCESS_FAULT:    return "instruction access fault";
    case TRAP_ILLEGAL_INSTRUCTION:   return "illegal instruction";
    case TRAP_BREAKPOINT:            return "breakpoint";
    case TRAP_LOAD_ADDR_MISALIGNED:  return "load address misaligned";
    case TRAP_LOAD_ACCESS_FAULT:     return "load access fault";
    case TRAP_STORE_ADDR_MISALIGNED: return "store address misaligned";
    case TRAP_STORE_ACCESS_FAULT:    return "store access fault";
    case TRAP_ECALL_U:               return "environment call from U-mode";
    case TRAP_ECALL_S:               return "environment call from S-mode";
    case TRAP_ECALL_M:               return "environment call from M-mode";
    case TRAP_INSTR_PAGE_FAULT:      return "instruction page fault";
    case TRAP_LOAD_PAGE_FAULT:       return "load page fault";
    case TRAP_STORE_PAGE_FAULT:      return "store page fault";
    default:                         return "unknown trap";
    }
}

void csr_dump(const CsrFile *csr) {
    printf("=== CSR State ===\n");
    printf("mstatus:   0x%08X  (MIE=%d MPIE=%d MPP=%d)\n",
           csr->mstatus, csr->mstatus_mie,
           csr->mstatus_mpie, csr->mstatus_mpp);
    printf("mtvec:     0x%08X  (%s)\n",
           csr->mtvec,
           csr->mtvec_mode_vectored ? "vectored" : "direct");
    printf("mepc:      0x%08X\n", csr->mepc);
    printf("mcause:    0x%08X\n", csr->mcause);
    printf("mtval:     0x%08X\n", csr->mtval);
    printf("mie:       0x%08X  mip: 0x%08X\n", csr->mie, csr->mip);
    printf("mcycle:    %llu  mtime: %llu  mtimecmp: %llu\n",
           (unsigned long long)csr->mcycle,
           (unsigned long long)csr->mtime,
           (unsigned long long)csr->mtimecmp);
    printf("priv:      %s\n",
           csr->priv == PRIV_MACHINE ? "M" :
           csr->priv == PRIV_SUPERVISOR ? "S" : "U");
}
