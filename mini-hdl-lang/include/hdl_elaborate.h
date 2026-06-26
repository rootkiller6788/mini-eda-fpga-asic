#ifndef HDL_ELABORATE_H
#define HDL_ELABORATE_H

#include "hdl_ast.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * hdl_elaborate.h — HDL Design Elaborator
 *
 * L1: Module/entity definitions, port binding, parameter propagation
 * L2: Design hierarchy resolution (top-down elaboration)
 *     Symbol table construction for scoped name lookup
 * L3: Hash-table symbol storage with scope chaining
 *     Parameter overrides via defparam/generate
 * L4: IEEE Std 1364-2001 §12 (Elaboration and hierarchy)
 *     Two-pass elaboration: definition pass → instantiation pass
 * L5: Topological sort for dependency-ordered elaboration
 *     Kahn's algorithm for detecting cyclic module dependencies
 */

#define ELAB_MAX_MODULES       16
#define ELAB_MAX_PARAMS        32
#define ELAB_MAX_SYMBOLS      256
#define ELAB_MAX_NAME_LEN     128

typedef enum {
    ELAB_SYM_MODULE,
    ELAB_SYM_PORT,
    ELAB_SYM_NET,
    ELAB_SYM_REG,
    ELAB_SYM_PARAM,
    ELAB_SYM_GENERATE
} ElabSymbolKind;

typedef enum {
    ELAB_PORT_INPUT,
    ELAB_PORT_OUTPUT,
    ELAB_PORT_INOUT
} ElabPortDir;

typedef struct {
    char            name[ELAB_MAX_NAME_LEN];
    ElabSymbolKind  kind;
    void           *ast_node;
    int             scope_level;
    int             width;
    bool            is_signed;
    union {
        struct { ElabPortDir dir; int bind_index; } port;
        struct { int default_value; } param;
        struct { int msb; int lsb; } range;
    } info;
} ElabSymbol;

typedef struct {
    char            name[ELAB_MAX_NAME_LEN];
    int             port_count;
    struct {
        char name[ELAB_MAX_NAME_LEN];
        ElabPortDir dir;
        int  width;
    } ports[64];
    int             param_count;
    struct {
        char name[ELAB_MAX_NAME_LEN];
        int  value;
    } params[ELAB_MAX_PARAMS];
    int             instance_count;
    struct {
        char name[ELAB_MAX_NAME_LEN];
        char module_name[ELAB_MAX_NAME_LEN];
        int  port_map[32];
        int  port_count;
    } instances[16];
    AstNode        *ast;
    bool            is_elaborated;
} HdlModule;

typedef struct {
    HdlModule       modules[ELAB_MAX_MODULES];
    int             module_count;
    ElabSymbol      symbols[ELAB_MAX_SYMBOLS];
    int             symbol_count;
    int             current_scope;
    int             scope_stack[32];
    int             scope_depth;
} Elaborator;

/* --- Lifecycle --- */
void elaborator_init(Elaborator *e);

/* --- Module management --- */
bool        elaborator_add_module(Elaborator *e, AstNode *ast);
HdlModule  *elaborator_find_module(Elaborator *e, const char *name);
bool        elaborator_elaborate_module(Elaborator *e, int mod_idx);
bool        elaborator_elaborate_all(Elaborator *e);

/* --- Symbol table (L3: scoped name resolution) --- */
void        elaborator_push_scope(Elaborator *e);
void        elaborator_pop_scope(Elaborator *e);
int         elaborator_add_symbol(Elaborator *e, const char *name,
                                  ElabSymbolKind kind, int width);
ElabSymbol *elaborator_lookup(Elaborator *e, const char *name);
ElabSymbol *elaborator_lookup_scoped(Elaborator *e, const char *name, int scope);

/* --- Port binding --- */
bool        elaborator_bind_ports(Elaborator *e, HdlModule *mod, AstNode *port_list);
bool        elaborator_bind_instance(Elaborator *e, HdlModule *parent,
                                     AstNode *inst_node);

/* --- Parameter propagation (L4: defparam semantics) --- */
bool        elaborator_resolve_params(Elaborator *e, HdlModule *mod);

/* --- Dependency ordering (L5: topological sort) --- */
bool        elaborator_topological_sort(Elaborator *e, int *order, int *count);

/* --- Debug --- */
void        elaborator_dump_symbols(Elaborator *e);
void        elaborator_dump_hierarchy(Elaborator *e, int mod_idx, int depth);

#endif
