#ifndef RISCV_CSR_H
#define RISCV_CSR_H
#include <stdint.h>
#include <stdbool.h>

#define CSR_MVENDORID  0xF11
#define CSR_MARCHID    0xF12
#define CSR_MIMPID     0xF13
#define CSR_MHARTID    0xF14
#define CSR_MSTATUS    0x300
#define CSR_MISA       0x301
#define CSR_MIE        0x304
#define CSR_MTVEC      0x305
#define CSR_MSCRATCH   0x340
#define CSR_MEPC       0x341
#define CSR_MCAUSE     0x342
#define CSR_MTVAL      0x343
#define CSR_MIP        0x344
#define CSR_SSTATUS    0x100
#define CSR_SIE        0x104
#define CSR_STVEC      0x105
#define CSR_SSCRATCH   0x140
#define CSR_SEPC       0x141
#define CSR_SCAUSE     0x142
#define CSR_STVAL      0x143
#define CSR_SATP       0x180

#define MSTATUS_MIE   0x00000008
#define MSTATUS_MPIE  0x00000080
#define MSTATUS_MPP   0x00001800
#define MSTATUS_MPP_M 3
#define MSTATUS_MPP_S 1
#define MSTATUS_MPP_U 0

#define MCAUSE_IRQ_FLAG 0x80000000
#define MCAUSE_CODE_MASK 0x7FFFFFFF

typedef struct {
    uint32_t regs[1024];
} CsrFile;

void csr_init(CsrFile *csr);
uint32_t csr_read(CsrFile *csr, uint16_t addr);
void csr_write(CsrFile *csr, uint16_t addr, uint32_t value);
void csr_set_bits(CsrFile *csr, uint16_t addr, uint32_t mask);
void csr_clear_bits(CsrFile *csr, uint16_t addr, uint32_t mask);
void csr_print(CsrFile *csr);
const char *csr_name(uint16_t addr);
#endif
