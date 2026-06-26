#ifndef SYSTEMVERILOG_SIM_H
#define SYSTEMVERILOG_SIM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define SV_MAX_NAME_LEN        256
#define SV_MAX_MODULES         32
#define SV_MAX_PORTS           64
#define SV_MAX_SIGNALS        128
#define SV_MAX_ALWAYS          64
#define SV_MAX_STMTS          256
#define SV_MAX_INTERFACES      16
#define SV_MAX_MODPORTS         8
#define SV_MAX_ENUMS           32
#define SV_MAX_STRUCTS         32
#define SV_MAX_PACKAGES        16
#define SV_MAX_ASSERTIONS      64
#define SV_MAX_ENUM_MEMBERS    32
#define SV_MAX_STRUCT_FIELDS   32
#define SV_MAX_PKG_ITEMS       64

typedef enum {
    SV_LOGIC_0 = 0,
    SV_LOGIC_1 = 1,
    SV_LOGIC_X = 2,
    SV_LOGIC_Z = 3
} SvLogicValue;

typedef enum {
    SV_ALWAYS_FF,
    SV_ALWAYS_COMB,
    SV_ALWAYS_LATCH
} SvAlwaysKind;

typedef enum {
    SV_PORT_INPUT,
    SV_PORT_OUTPUT,
    SV_PORT_INOUT,
    SV_PORT_REF
} SvPortDir;

typedef enum {
    SV_STMT_ASSIGN,
    SV_STMT_IF,
    SV_STMT_CASE,
    SV_STMT_FOR_LOOP,
    SV_STMT_DISPLAY,
    SV_STMT_ASSERT
} SvStmtKind;

typedef enum {
    SV_ASSERT_IMMEDIATE,
    SV_ASSERT_CONCURRENT
} SvAssertKind;

typedef struct {
    char                name[SV_MAX_NAME_LEN];
    SvLogicValue        value;
    int                 width;
    bool                is_four_state;
} SvSignal;

typedef struct {
    char                name[SV_MAX_NAME_LEN];
    SvPortDir           direction;
    int                 width;
    int                 signal_index;
} SvPort;

typedef struct {
    int                 signal_index;
} SvModport;

typedef struct {
    char                name[SV_MAX_NAME_LEN];
    int                 modport_count;
    SvModport           modports[SV_MAX_MODPORTS];
    int                 signal_count;
    SvSignal            signals[SV_MAX_SIGNALS];
} SvInterface;

typedef struct {
    char                name[SV_MAX_NAME_LEN];
    int                 member_count;
    struct {
        char name[SV_MAX_NAME_LEN];
        int  value;
    } members[SV_MAX_ENUM_MEMBERS];
} SvEnum;

typedef struct {
    char                name[SV_MAX_NAME_LEN];
    int                 field_count;
    struct {
        char name[SV_MAX_NAME_LEN];
        int  width;
        bool is_signed;
    } fields[SV_MAX_STRUCT_FIELDS];
} SvStruct;

typedef struct {
    char                name[SV_MAX_NAME_LEN];
    int                 item_count;
    struct {
        char name[SV_MAX_NAME_LEN];
        int  value;
        int  type;
    } items[SV_MAX_PKG_ITEMS];
} SvPackage;

typedef struct {
    SvStmtKind          kind;
    int                 lhs_signal;
    SvLogicValue       *rhs_value;
    int                 rhs_width;
    int                 cond_signal;
    int                 true_target;
    int                 false_target;
    int                 delay;
    char                display_msg[SV_MAX_NAME_LEN];
} SvStmt;

typedef struct {
    SvAlwaysKind        kind;
    int                 sensitivity_count;
    int                 sensitivity[SV_MAX_PORTS];
    bool                edge_sensitive;
    int                 stmt_count;
    SvStmt              stmts[SV_MAX_STMTS];
    int                 entry_stmt;
} SvAlwaysBlock;

typedef struct {
    char                name[SV_MAX_NAME_LEN];
    char                cond_text[SV_MAX_NAME_LEN];
    SvAssertKind        kind;
    bool                is_immediate;
    bool                passed;
} SvAssertion;

typedef struct {
    char                name[SV_MAX_NAME_LEN];
    int                 port_count;
    SvPort              ports[SV_MAX_PORTS];
    int                 signal_count;
    SvSignal            signals[SV_MAX_SIGNALS];
    int                 always_count;
    SvAlwaysBlock       always_blocks[SV_MAX_ALWAYS];
    int                 interface_count;
    SvInterface         interfaces[SV_MAX_INTERFACES];
    int                 enum_count;
    SvEnum              enums[SV_MAX_ENUMS];
    int                 struct_count;
    SvStruct            structs[SV_MAX_STRUCTS];
    int                 assertion_count;
    SvAssertion         assertions[SV_MAX_ASSERTIONS];
} SvModule;

typedef struct {
    SvModule            modules[SV_MAX_MODULES];
    int                 module_count;
    int                 package_count;
    SvPackage           packages[SV_MAX_PACKAGES];
    uint64_t            current_time;
    bool                running;
} SvSimulator;

void         sv_init(SvSimulator *sim);
int          sv_add_module(SvSimulator *sim, const char *name);
SvModule    *sv_get_module(SvSimulator *sim, int idx);
int          sv_add_port(SvModule *mod, const char *name, SvPortDir dir, int width);
int          sv_add_signal(SvModule *mod, const char *name, int width, bool four_state);
int          sv_add_always_ff(SvModule *mod, const char *clk_signal, bool posedge);
int          sv_add_always_comb(SvModule *mod);
int          sv_add_interface(SvModule *mod, const char *name);
int          sv_add_enum(SvModule *mod, const char *name);
void         sv_add_enum_member(SvEnum *e, const char *name, int value);
int          sv_add_struct(SvModule *mod, const char *name);
void         sv_add_struct_field(SvStruct *s, const char *name, int width, bool is_signed);
int          sv_add_package(SvSimulator *sim, const char *name);
void         sv_add_package_item(SvPackage *pkg, const char *name, int value, int type);
int          sv_add_assertion(SvModule *mod, const char *cond, SvAssertKind kind);
int          sv_add_stmt(SvAlwaysBlock *blk, SvStmtKind kind);
void         sv_add_assign(SvAlwaysBlock *blk, int stmt, int lhs, const SvLogicValue *rhs, int width);
void         sv_evaluate_always_ff(SvModule *mod, SvAlwaysBlock *blk);
void         sv_evaluate_always_comb(SvModule *mod, SvAlwaysBlock *blk);
void         sv_evaluate_assertions(SvModule *mod);
void         sv_run(SvSimulator *sim, uint64_t end_time);
void         sv_set_signal(SvModule *mod, int sig, SvLogicValue val);
SvLogicValue sv_get_signal(const SvModule *mod, int sig);
void         sv_display_module(const SvModule *mod);
void         sv_free(SvSimulator *sim);

#endif
