#ifndef RISCV_DEBUG_H
#define RISCV_DEBUG_H
#include <stdint.h>
#include <stdbool.h>

#define DTM_DMI_ADDR    0x11
#define DTM_DMI_DATA    0x12
#define DTM_DMI_OP_NOP  0
#define DTM_DMI_OP_READ 1
#define DTM_DMI_OP_WRITE 2

#define DM_DMSTATUS      0x11
#define DM_DMCONTROL     0x10
#define DM_HARTINFO      0x12
#define DM_ABSTRACTCS    0x16
#define DM_COMMAND       0x17
#define DM_ABSTRACTAUTO  0x18
#define DM_SBCS          0x38
#define DM_SBADDRESS0    0x39

#define CMD_ACCESS_REG   0
#define CMD_QUICK_ACCESS 1

#define HALTREQ_BIT  31
#define RESUMEREQ_BIT 30

typedef enum { DBG_RUNNING, DBG_HALTED, DBG_STEPPING } DebugState;

typedef struct { uint32_t addr; bool enabled; bool is_instr; } HwBreakpoint;

typedef struct {
    DebugState state;
    HwBreakpoint breakpoints[8];
    int bp_count;
    uint32_t pc;
    uint32_t regs[32];
    bool halted;
    bool resume_ack;
} DebugModule;

void debug_init(DebugModule *dm);
bool debug_halt(DebugModule *dm, uint32_t current_pc);
bool debug_resume(DebugModule *dm);
bool debug_step(DebugModule *dm, uint32_t current_pc);
int  debug_set_breakpoint(DebugModule *dm, uint32_t addr, bool is_instr);
bool debug_clear_breakpoint(DebugModule *dm, int idx);
bool debug_hit_breakpoint(DebugModule *dm, uint32_t addr, bool is_instr);
bool debug_abstract_command(DebugModule *dm, uint32_t cmd, uint32_t arg, uint32_t *result);
void debug_dtm_transfer(uint32_t addr, uint32_t *data, bool is_write);
void debug_print(DebugModule *dm);
#endif
