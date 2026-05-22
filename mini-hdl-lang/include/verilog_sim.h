#ifndef VERILOG_SIM_H
#define VERILOG_SIM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define VS_MAX_NAME_LEN     256
#define VS_MAX_PORTS        64
#define VS_MAX_NETS         256
#define VS_MAX_ASSIGNS      128
#define VS_MAX_ALWAYS        64
#define VS_MAX_STMTS        256
#define VS_MAX_EVENTS      1024
#define VS_MAX_MODULES       32
#define VS_MAX_SENSITIVITY   16
#define VS_MAX_VCD_SIGNALS  128

typedef enum {
    VS_NET_WIRE,
    VS_NET_REG,
    VS_NET_WAND,
    VS_NET_WOR,
    VS_NET_TRI,
    VS_NET_TRI0,
    VS_NET_TRI1,
    VS_NET_SUPPLY0,
    VS_NET_SUPPLY1
} VerilogNetKind;

typedef enum {
    VS_PORT_INPUT,
    VS_PORT_OUTPUT,
    VS_PORT_INOUT
} VerilogPortDir;

typedef enum {
    VS_VAL_0,
    VS_VAL_1,
    VS_VAL_X,
    VS_VAL_Z
} VerilogValue;

typedef enum {
    VS_EVT_POSEDGE,
    VS_EVT_NEGEDGE,
    VS_EVT_ANYEDGE,
    VS_EVT_LEVEL
} VerilogEventType;

typedef enum {
    VS_STMT_BLOCKING_ASSIGN,
    VS_STMT_NONBLOCKING_ASSIGN,
    VS_STMT_IF,
    VS_STMT_CASE,
    VS_STMT_DISPLAY,
    VS_STMT_FOR_LOOP,
    VS_STMT_WHILE_LOOP
} VerilogStmtKind;

typedef enum {
    VS_SENS_POSEDGE,
    VS_SENS_NEGEDGE,
    VS_SENS_LEVEL
} VerilogSensitivityKind;

typedef uint64_t VerilogTime;

typedef struct {
    char                    name[VS_MAX_NAME_LEN];
    VerilogNetKind          kind;
    VerilogValue            value;
    int                     width;
    bool                    driven;
    char                    vcd_identifier;
} VerilogNet;

typedef struct {
    char                    name[VS_MAX_NAME_LEN];
    VerilogPortDir          direction;
    int                     net_index;
    int                     width;
} VerilogPort;

typedef struct {
    int                     lhs_net;
    VerilogValue           *rhs_values;
    int                     rhs_width;
} VerilogAssign;

typedef struct {
    VerilogSensitivityKind  kind;
    int                     signal_index;
} VerilogSensitivity;

typedef struct {
    VerilogStmtKind         kind;
    int                     lhs_net;
    VerilogValue           *rhs_values;
    int                     rhs_width;
    int                     cond_net;
    int                     true_next;
    int                     false_next;
    char                    display_msg[VS_MAX_NAME_LEN];
    int                     delay;
} VerilogStmt;

typedef struct {
    int                     sensitivity_count;
    VerilogSensitivity      sensitivity[VS_MAX_SENSITIVITY];
    int                     stmt_count;
    VerilogStmt             stmts[VS_MAX_STMTS];
    int                     entry_stmt;
} VerilogAlwaysBlock;

typedef struct {
    char                    name[VS_MAX_NAME_LEN];
    int                     port_count;
    VerilogPort             ports[VS_MAX_PORTS];
    int                     net_count;
    VerilogNet              nets[VS_MAX_NETS];
    int                     assign_count;
    VerilogAssign           assigns[VS_MAX_ASSIGNS];
    int                     always_count;
    VerilogAlwaysBlock      always_blocks[VS_MAX_ALWAYS];
} VerilogModule;

typedef struct {
    VerilogTime             time;
    VerilogEventType        type;
    int                     signal_index;
    int                     module_index;
    void                   *callback_data;
} VerilogEvent;

typedef struct {
    VerilogModule           modules[VS_MAX_MODULES];
    int                     module_count;
    VerilogTime             current_time;
    int                     event_count;
    VerilogEvent            events[VS_MAX_EVENTS];
    bool                    running;
    FILE                   *vcd_file;
    VerilogTime             vcd_last_time;
    int                     vcd_signal_count;
    int                     vcd_signal_ids[VS_MAX_VCD_SIGNALS];
} VerilogSimulator;

void          vs_init(VerilogSimulator *sim);
int           vs_add_module(VerilogSimulator *sim, const char *name);
VerilogModule *vs_get_module(VerilogSimulator *sim, int idx);
int           vs_add_port(VerilogModule *mod, const char *name, VerilogPortDir dir, int width);
int           vs_add_net(VerilogModule *mod, const char *name, VerilogNetKind kind, int width);
int           vs_add_always(VerilogModule *mod);
void          vs_add_sensitivity(VerilogAlwaysBlock *blk, VerilogSensitivityKind kind, int signal);
int           vs_add_stmt(VerilogAlwaysBlock *blk, VerilogStmtKind kind);
void          vs_add_blocking_assign(VerilogAlwaysBlock *blk, int stmt, int lhs, const VerilogValue *rhs, int width);
void          vs_add_nonblocking_assign(VerilogAlwaysBlock *blk, int stmt, int lhs, const VerilogValue *rhs, int width, int delay);
void          vs_add_assign(VerilogModule *mod, int lhs_net, const VerilogValue *rhs, int width);
void          vs_schedule_event(VerilogSimulator *sim, VerilogTime t, VerilogEventType type, int signal, int module);
void          vs_evaluate_continuous_assigns(VerilogModule *mod);
void          vs_evaluate_always_block(VerilogSimulator *sim, VerilogModule *mod, VerilogAlwaysBlock *blk);
void          vs_run(VerilogSimulator *sim, VerilogTime end_time);
void          vs_set_net_value(VerilogModule *mod, int net, VerilogValue val);
VerilogValue  vs_get_net_value(const VerilogModule *mod, int net);
void          vs_display_signals(const VerilogModule *mod, VerilogTime t);
void          vs_vcd_open(VerilogSimulator *sim, const char *filename);
void          vs_vcd_dump_signals(VerilogSimulator *sim);
void          vs_vcd_print_time(VerilogSimulator *sim, VerilogTime t);
void          vs_vcd_close(VerilogSimulator *sim);
void          vs_free(VerilogSimulator *sim);

#endif
