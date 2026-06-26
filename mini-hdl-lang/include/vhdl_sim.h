#ifndef VHDL_SIM_H
#define VHDL_SIM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define VHDL_MAX_NAME_LEN       256
#define VHDL_MAX_PORTS           64
#define VHDL_MAX_SIGNALS        128
#define VHDL_MAX_VARIABLES       64
#define VHDL_MAX_PROCESSES       32
#define VHDL_MAX_STMTS          256
#define VHDL_MAX_GENERICS        16
#define VHDL_MAX_CONCURRENT      64
#define VHDL_MAX_DRIVERS          8
#define VHDL_MAX_ENTITIES        16
#define VHDL_MAX_PROJECTED      128

typedef enum {
    VHDL_STD_U  = 0,
    VHDL_STD_X  = 1,
    VHDL_STD_0  = 2,
    VHDL_STD_1  = 3,
    VHDL_STD_Z  = 4,
    VHDL_STD_W  = 5,
    VHDL_STD_L  = 6,
    VHDL_STD_H  = 7,
    VHDL_STD_DC = 8
} VhdlStdLogic;

typedef enum {
    VHDL_PORT_IN,
    VHDL_PORT_OUT,
    VHDL_PORT_INOUT,
    VHDL_PORT_BUFFER
} VhdlPortMode;

typedef enum {
    VHDL_SIG_STD_LOGIC,
    VHDL_SIG_STD_LOGIC_VECTOR,
    VHDL_SIG_BIT,
    VHDL_SIG_BIT_VECTOR,
    VHDL_SIG_INTEGER,
    VHDL_SIG_BOOLEAN
} VhdlSignalType;

typedef enum {
    VHDL_STMT_SIG_ASSIGN,
    VHDL_STMT_VAR_ASSIGN,
    VHDL_STMT_IF,
    VHDL_STMT_CASE,
    VHDL_STMT_WAIT,
    VHDL_STMT_REPORT,
    VHDL_STMT_FOR_LOOP
} VhdlStmtKind;

typedef struct {
    VhdlStdLogic        value;
    VhdlStdLogic        drivers[VHDL_MAX_DRIVERS];
    int                 driver_count;
} VhdlResolvedSignal;

typedef struct {
    char                name[VHDL_MAX_NAME_LEN];
    VhdlSignalType      type;
    int                 width;
    VhdlResolvedSignal  resolved;
    VhdlStdLogic        projected[VHDL_MAX_PROJECTED];
    int                 projected_count;
    bool                is_resolved;
    bool                has_event;
} VhdlSignal;

typedef struct {
    char                name[VHDL_MAX_NAME_LEN];
    VhdlSignalType      type;
    int                 width;
    VhdlStdLogic       *value;
    bool                is_shared;
} VhdlVariable;

typedef struct {
    char                name[VHDL_MAX_NAME_LEN];
    VhdlPortMode        mode;
    VhdlSignalType      type;
    int                 width;
    int                 signal_index;
} VhdlPort;

typedef struct {
    char                name[VHDL_MAX_NAME_LEN];
    int                 port_count;
    VhdlPort            ports[VHDL_MAX_PORTS];
    int                 generic_count;
    struct {
        char name[VHDL_MAX_NAME_LEN];
        int  value;
    } generics[VHDL_MAX_GENERICS];
} VhdlEntity;

typedef struct {
    VhdlStmtKind        kind;
    int                 lhs_signal;
    VhdlStdLogic       *rhs_value;
    int                 rhs_width;
    int                 delay;
    int                 true_target;
    int                 false_target;
    int                 cond_signal;
    char                report_msg[VHDL_MAX_NAME_LEN];
} VhdlStmt;

typedef struct {
    char                name[VHDL_MAX_NAME_LEN];
    int                 sensitivity_count;
    int                 sensitivity[VHDL_MAX_PROCESSES * 2];
    int                 var_count;
    VhdlVariable        variables[VHDL_MAX_VARIABLES];
    int                 stmt_count;
    VhdlStmt            stmts[VHDL_MAX_STMTS];
    int                 entry_stmt;
    bool                is_active;
} VhdlProcess;

typedef enum {
    VHDL_CC_SIG_ASSIGN,
    VHDL_CC_COND_ASSIGN,
    VHDL_CC_COMPONENT_INST
} VhdlConcurrentKind;

typedef struct {
    VhdlConcurrentKind  kind;
    int                 target_signal;
    VhdlStdLogic       *rhs_value;
    int                 rhs_width;
    int                 cond_signal;
} VhdlConcurrentStmt;

typedef struct {
    char                name[VHDL_MAX_NAME_LEN];
    int                 entity_index;
    int                 signal_count;
    VhdlSignal          signals[VHDL_MAX_SIGNALS];
    int                 process_count;
    VhdlProcess         processes[VHDL_MAX_PROCESSES];
    int                 concurrent_count;
    VhdlConcurrentStmt  concurrent_stmts[VHDL_MAX_CONCURRENT];
} VhdlArchitecture;

typedef struct {
    VhdlEntity          entities[VHDL_MAX_ENTITIES];
    int                 entity_count;
    VhdlArchitecture    architectures[VHDL_MAX_ENTITIES];
    int                 arch_count;
    uint64_t            current_time;
    uint64_t            delta_count;
    bool                running;
} VhdlSimulator;

VhdlStdLogic  vhdl_resolve_std_logic(const VhdlStdLogic *drivers, int count);
const char   *vhdl_std_logic_to_char(VhdlStdLogic val);
VhdlStdLogic  vhdl_char_to_std_logic(char c);

void          vhdl_init(VhdlSimulator *sim);
int           vhdl_add_entity(VhdlSimulator *sim, const char *name);
VhdlEntity   *vhdl_get_entity(VhdlSimulator *sim, int idx);
int           vhdl_add_port(VhdlEntity *ent, const char *name, VhdlPortMode mode, VhdlSignalType type, int width);
int           vhdl_add_architecture(VhdlSimulator *sim, int entity_idx, const char *name);
VhdlArchitecture *vhdl_get_architecture(VhdlSimulator *sim, int idx);
int           vhdl_add_signal(VhdlArchitecture *arch, const char *name, VhdlSignalType type, int width, bool resolved);
int           vhdl_add_variable(VhdlProcess *proc, const char *name, VhdlSignalType type, int width);
int           vhdl_add_process(VhdlArchitecture *arch, const char *name);
void          vhdl_add_process_sensitivity(VhdlProcess *proc, int signal_idx);
int           vhdl_add_process_stmt(VhdlProcess *proc, VhdlStmtKind kind);
void          vhdl_add_signal_assign(VhdlProcess *proc, int stmt, int lhs, const VhdlStdLogic *rhs, int width, int delay);
int           vhdl_add_concurrent(VhdlArchitecture *arch, VhdlConcurrentKind kind);
void          vhdl_set_concurrent_assign(int stmt, int target, const VhdlStdLogic *rhs, int width);
void          vhdl_run_delta_cycle(VhdlSimulator *sim, VhdlArchitecture *arch);
void          vhdl_evaluate_process(VhdlSimulator *sim, VhdlArchitecture *arch, VhdlProcess *proc);
void          vhdl_evaluate_concurrent(VhdlArchitecture *arch);
void          vhdl_resolve_signals(VhdlArchitecture *arch);
void          vhdl_run(VhdlSimulator *sim, uint64_t end_time);
void          vhdl_display_signals(const VhdlArchitecture *arch);
void          vhdl_free(VhdlSimulator *sim);

#endif
