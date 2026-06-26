#ifndef HDL_PARSER_H
#define HDL_PARSER_H

#include "hdl_lexer.h"
#include "hdl_ast.h"
#include <stdbool.h>

/**
 * hdl_parser.h — Recursive-Descent HDL Parser
 *
 * L1: Production rule definitions for Verilog/SystemVerilog subset
 * L2: Module declaration parsing, port list parsing, always block parsing
 * L3: Recursive descent parser with error recovery (panic-mode)
 *     Grammar: IEEE 1364-2001 Annex A
 * L4: IEEE Std 1364-2001 §12 (Elaboration)
 *     First/Follow sets for Verilog grammar fragment
 * L5: Pratt parser for operator precedence expressions
 */

#define PARSER_MAX_ERRORS 64

typedef struct {
    Lexer       lexer;
    AstNode    *root;
    int         error_count;
    struct {
        char        msg[256];
        LexerPos    pos;
    } errors[PARSER_MAX_ERRORS];
    bool        panic_mode;
} Parser;

/* Lifecycle */
void     parser_init(Parser *p, const char *source);
bool     parser_parse(Parser *p);
AstNode *parser_get_root(Parser *p);

/* Expression parsing (Pratt-style, L5) */
AstNode *parser_parse_expr(Parser *p, int min_prec);

/* Declaration parsing */
AstNode *parser_parse_module(Parser *p);
AstNode *parser_parse_port_list(Parser *p);
AstNode *parser_parse_port_decl(Parser *p);
AstNode *parser_parse_net_decl(Parser *p);
AstNode *parser_parse_always_block(Parser *p);
AstNode *parser_parse_stmt(Parser *p);
AstNode *parser_parse_assign(Parser *p);
AstNode *parser_parse_sensitivity_list(Parser *p);

/* Error handling */
void parser_error(Parser *p, const char *msg);
void parser_print_errors(Parser *p);

#endif
