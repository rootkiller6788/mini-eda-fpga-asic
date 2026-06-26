#ifndef RISCV_PRIV_H
#define RISCV_PRIV_H
#include <stdint.h>
#include <stdbool.h>

typedef enum { PRIV_U = 0, PRIV_S = 1, PRIV_M = 3 } PrivMode;

typedef struct {
    PrivMode current_mode;
    bool trap_pending;
    uint32_t trap_cause;
    uint32_t trap_value;
    uint32_t trap_pc;
    uint32_t return_pc;
} PrivState;

void priv_init(PrivState *p);
bool priv_switch_to(PrivState *p, PrivMode target);
const char *priv_mode_name(PrivMode m);
bool priv_can_access(PrivState *p, uint16_t csr_addr);
void priv_trap(PrivState *p, uint32_t cause, uint32_t tval, uint32_t pc);
bool priv_mret(PrivState *p);
bool priv_sret(PrivState *p);
void priv_print(PrivState *p);
#endif
