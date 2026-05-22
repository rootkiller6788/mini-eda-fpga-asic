#ifndef RISCV_EXCEPT_H
#define RISCV_EXCEPT_H
#include <stdint.h>
#include <stdbool.h>

#define EXC_INSTR_ADDR_MISALIGNED  0
#define EXC_INSTR_ACCESS_FAULT     1
#define EXC_ILLEGAL_INSTRUCTION    2
#define EXC_BREAKPOINT             3
#define EXC_LOAD_ADDR_MISALIGNED   4
#define EXC_LOAD_ACCESS_FAULT      5
#define EXC_STORE_ADDR_MISALIGNED  6
#define EXC_STORE_ACCESS_FAULT     7
#define EXC_ECALL_U                8
#define EXC_ECALL_S                9
#define EXC_ECALL_M                11
#define EXC_INSTR_PAGE_FAULT       12
#define EXC_LOAD_PAGE_FAULT        13
#define EXC_STORE_PAGE_FAULT       15

#define IRQ_SOFTWARE_S  1
#define IRQ_TIMER_S     5
#define IRQ_EXT_S       9
#define IRQ_SOFTWARE_M  3
#define IRQ_TIMER_M     7
#define IRQ_EXT_M       11

typedef struct {
    uint32_t pending_irqs;
    uint32_t enabled_irqs;
    bool global_enable;
} InterruptCtrl;

const char *except_name(uint32_t cause);
bool except_raise(uint32_t cause, uint32_t tval, uint32_t pc, void (*handler)(uint32_t, uint32_t, uint32_t));
void interrupt_init(InterruptCtrl *ic);
bool interrupt_enable(InterruptCtrl *ic, uint32_t irq_id);
bool interrupt_disable(InterruptCtrl *ic, uint32_t irq_id);
bool interrupt_pending(InterruptCtrl *ic, uint32_t irq_id);
void interrupt_assert(InterruptCtrl *ic, uint32_t irq_id);
void interrupt_clear(InterruptCtrl *ic, uint32_t irq_id);
uint32_t interrupt_take(InterruptCtrl *ic);
void interrupt_print(InterruptCtrl *ic);
#endif
