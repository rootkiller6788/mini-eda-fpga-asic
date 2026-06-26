#ifndef CSR_PRIVILEGE_H
#define CSR_PRIVILEGE_H

#include <stdint.h>
#include <stdbool.h>

#define CSR_MSTATUS     0x300
#define CSR_MISA        0x301
#define CSR_MIE         0x304
#define CSR_MTVEC       0x305
#define CSR_MSCRATCH    0x340
#define CSR_MEPC        0x341
#define CSR_MCAUSE      0x342
#define CSR_MTVAL       0x343
#define CSR_MIP         0x344
#define CSR_MCYCLE      0xB00
#define CSR_MTIME       0xB01
#define CSR_MTIMECMP    0xB02

#define MSTATUS_MIE     0x00000008
#define MSTATUS_MPIE    0x00000080
#define MSTATUS_MPP_M   0x00001800
#define MSTATUS_MPP_S   0x00001000
#define MSTATUS_MPP_U   0x00000000

#define MCAUSE_INT_BIT  0x80000000

typedef enum {
    PRIV_USER       = 0,
    PRIV_SUPERVISOR = 1,
    PRIV_MACHINE    = 3
} PrivilegeMode;

typedef enum {
    TRAP_INSTR_ADDR_MISALIGNED  = 0,
    TRAP_INSTR_ACCESS_FAULT     = 1,
    TRAP_ILLEGAL_INSTRUCTION    = 2,
    TRAP_BREAKPOINT             = 3,
    TRAP_LOAD_ADDR_MISALIGNED   = 4,
    TRAP_LOAD_ACCESS_FAULT      = 5,
    TRAP_STORE_ADDR_MISALIGNED  = 6,
    TRAP_STORE_ACCESS_FAULT     = 7,
    TRAP_ECALL_U                = 8,
    TRAP_ECALL_S                = 9,
    TRAP_ECALL_M                = 11,
    TRAP_INSTR_PAGE_FAULT       = 12,
    TRAP_LOAD_PAGE_FAULT        = 13,
    TRAP_STORE_PAGE_FAULT       = 15
} TrapCause;

typedef enum {
    INT_TIMER_M  = 0x80000007,
    INT_SOFT_M   = 0x80000003,
    INT_EXT_M    = 0x8000000B
} InterruptCode;

typedef struct {
    uint32_t mstatus;
    uint32_t misa;
    uint32_t mie;
    uint32_t mtvec;
    uint32_t mscratch;
    uint32_t mepc;
    uint32_t mcause;
    uint32_t mtval;
    uint32_t mip;

    uint64_t mcycle;
    uint64_t mtime;
    uint64_t mtimecmp;

    PrivilegeMode priv;

    bool mstatus_mie;
    bool mstatus_mpie;
    uint8_t mstatus_mpp;

    bool mtvec_mode_direct;
    bool mtvec_mode_vectored;

    bool (*timer_callback)(void);
} CsrFile;

void csr_init(CsrFile *csr);
void csr_reset(CsrFile *csr);

uint32_t csr_read32(const CsrFile *csr, uint16_t addr);
void     csr_write32(CsrFile *csr, uint16_t addr, uint32_t val);

void csr_set_mstatus(CsrFile *csr, uint32_t val);
uint32_t csr_get_mstatus(const CsrFile *csr);

void csr_take_exception(CsrFile *csr, TrapCause cause, uint32_t epc,
                        uint32_t tval);
void csr_take_interrupt(CsrFile *csr, InterruptCode code, uint32_t epc);
void csr_mret(CsrFile *csr);
void csr_handle_ecall(CsrFile *csr, uint32_t epc);

uint32_t csr_trap_vector(const CsrFile *csr, bool is_interrupt);
void     csr_tick_timer(CsrFile *csr);
bool     csr_timer_interrupt_pending(const CsrFile *csr);
void     csr_set_timer_callback(CsrFile *csr, bool (*cb)(void));

void csr_dump(const CsrFile *csr);
const char *csr_name(uint16_t addr);
const char *trap_name(TrapCause cause);

#endif
