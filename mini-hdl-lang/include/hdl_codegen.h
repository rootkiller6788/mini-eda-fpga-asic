#ifndef HDL_CODEGEN_H
#define HDL_CODEGEN_H

#include "hdl_ast.h"

/**
 * hdl_codegen.h — HDL Code Generator (Backend)
 *
 * L1: Target code emission targets (Verilog, VHDL, netlist, C-model)
 * L2: AST-to-text lowering; expression-to-string conversion
 * L3: Double-buffered output with indentation management
 *     Pretty-printer with configurable formatting
 * L4: IEEE Std 1364-2001 §4 (Source text) output compliance
 *     Backus-Naur Form preservation: round-trip AST→text→AST
 * L5: Constant folding during codegen (L5: compiler optimization)
 *     Dead code elimination for unconnected net pruning
 */

#define CODEGEN_MAX_BUF (256 * 1024)
#define CODEGEN_MAX_INDENT 32

typedef enum {
    CODEGEN_VERILOG,
    CODEGEN_VHDL,
    CODEGEN_NETLIST,
    CODEGEN_C_MODEL,
    CODEGEN_JSON
} CodegenTarget;

typedef struct {
    CodegenTarget   target;
    char            buf[CODEGEN_MAX_BUF];
    int             buf_len;
    int             indent;
    int             error_count;
    bool            fold_constants;
    bool            prune_dead_code;
} Codegen;

/* --- Lifecycle --- */
void codegen_init(Codegen *cg, CodegenTarget target);
void codegen_generate(Codegen *cg, AstNode *root);
const char *codegen_get_buffer(const Codegen *cg);

/* --- Target-specific emitters --- */
void codegen_emit_verilog(Codegen *cg, AstNode *root);
void codegen_emit_vhdl(Codegen *cg, AstNode *root);
void codegen_emit_netlist(Codegen *cg, AstNode *root);
void codegen_emit_c_model(Codegen *cg, AstNode *root);

/* --- Expression codegen (L2: lowering to target syntax) --- */
void codegen_emit_expr(Codegen *cg, AstNode *expr);
void codegen_emit_binary(Codegen *cg, AstNode *expr);
void codegen_emit_unary(Codegen *cg, AstNode *expr);
void codegen_emit_ident(Codegen *cg, AstNode *expr);
void codegen_emit_number(Codegen *cg, AstNode *expr);

/* --- Statement codegen --- */
void codegen_emit_stmt(Codegen *cg, AstNode *stmt);
void codegen_emit_assign(Codegen *cg, AstNode *assign);
void codegen_emit_if(Codegen *cg, AstNode *if_stmt);
void codegen_emit_case(Codegen *cg, AstNode *case_stmt);
void codegen_emit_always(Codegen *cg, AstNode *always);
void codegen_emit_block(Codegen *cg, AstNode *block);

/* --- Utility --- */
void codegen_write(Codegen *cg, const char *text);
void codegen_write_line(Codegen *cg, const char *text);
void codegen_write_fmt(Codegen *cg, const char *fmt, ...);
void codegen_indent(Codegen *cg);
void codegen_dedent(Codegen *cg);
void codegen_newline(Codegen *cg);

#endif
