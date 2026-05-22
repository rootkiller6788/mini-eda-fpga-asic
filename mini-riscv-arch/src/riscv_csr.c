#include "riscv_csr.h"
#include <stdio.h>
#include <string.h>

void csr_init(CsrFile *csr) {
    memset(csr->regs, 0, sizeof(csr->regs));
    csr->regs[CSR_MVENDORID >> 2] = 0;
    csr->regs[CSR_MARCHID >> 2] = 1;
    csr->regs[CSR_MIMPID >> 2] = 0;
    csr->regs[CSR_MHARTID >> 2] = 0;
    csr_write(csr, CSR_MISA, (1U << 20) | (1U << 18) | (1U << 12) | (1U << 8) | (1U << 0)); /* I+M+A+C+RV32 */
    csr_write(csr, CSR_MSTATUS, MSTATUS_MPP & (MSTATUS_MPP_M << MSTATUS_MPP_M));
}

uint32_t csr_read(CsrFile *csr, uint16_t addr) {
    if (addr < 1024) { uint16_t idx = addr >> 2; return csr->regs[idx]; }
    return 0;
}

void csr_write(CsrFile *csr, uint16_t addr, uint32_t value) {
    uint16_t idx = addr >> 2;
    if (idx < 1024) {
        /* Apply WARL (Write Any Read Legal) constraints */
        switch (addr) {
            case CSR_MSTATUS:
                csr->regs[idx] = (csr->regs[idx] & 0x00001888) | (value & 0x00001888);
                break;
            case CSR_MTVEC:
                csr->regs[idx] = value & 0xFFFFFFFC;
                break;
            case CSR_MEPC:
                csr->regs[idx] = value & 0xFFFFFFFC;
                break;
            case CSR_SATP:
                if ((value >> 31) == 0) csr->regs[idx] = 0;
                else csr->regs[idx] = value;
                break;
            default:
                csr->regs[idx] = value;
        }
    }
}

void csr_set_bits(CsrFile *csr, uint16_t addr, uint32_t mask) {
    csr_write(csr, addr, csr_read(csr, addr) | mask);
}

void csr_clear_bits(CsrFile *csr, uint16_t addr, uint32_t mask) {
    csr_write(csr, addr, csr_read(csr, addr) & ~mask);
}

const char *csr_name(uint16_t addr) {
    switch (addr) {
        case CSR_MVENDORID: return "mvendorid"; case CSR_MARCHID: return "marchid";
        case CSR_MIMPID: return "mimpid"; case CSR_MHARTID: return "mhartid";
        case CSR_MSTATUS: return "mstatus"; case CSR_MISA: return "misa";
        case CSR_MIE: return "mie"; case CSR_MTVEC: return "mtvec";
        case CSR_MSCRATCH: return "mscratch"; case CSR_MEPC: return "mepc";
        case CSR_MCAUSE: return "mcause"; case CSR_MTVAL: return "mtval";
        case CSR_MIP: return "mip"; case CSR_SSTATUS: return "sstatus";
        case CSR_SIE: return "sie"; case CSR_STVEC: return "stvec";
        case CSR_SEPC: return "sepc"; case CSR_SCAUSE: return "scause";
        case CSR_STVAL: return "stval"; case CSR_SATP: return "satp";
        default: return "unknown";
    }
}

void csr_print(CsrFile *csr) {
    printf("=== CSR File ===\n");
    uint16_t addrs[] = {CSR_MSTATUS,CSR_MISA,CSR_MTVEC,CSR_MEPC,CSR_MCAUSE,CSR_SATP,0};
    for (int i = 0; addrs[i]; i++)
        printf("  %-12s (0x%03X): 0x%08X\n", csr_name(addrs[i]), addrs[i], csr_read(csr, addrs[i]));
}
