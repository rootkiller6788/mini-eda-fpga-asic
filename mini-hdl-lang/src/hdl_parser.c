#include "hdl_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ================================================================
 * L4: Verilog Grammar Fragment (IEEE 1364-2001 Annex A simplified)
 *
 * source_text ::= description*
 * description ::= module_declaration
 * module_declaration ::= 'module' identifier [port_list] ';' module_item* 'endmodule'
 * port_list ::= '(' port (',' port)* ')'
 * port ::= [direction] ['[' range ']'] identifier
 * module_item ::= net_decl | reg_decl | assign_stmt | always_block
 * net_decl ::= 'wire' ['[' range ']'] identifier ';'
 * assign_stmt ::= 'assign' identifier '=' expr ';'
 * always_block ::= 'always' '@' '(' sensitivity_list ')' stmt
 * sensitivity_list ::= sensitivity_item (',' sensitivity_item)*
 * stmt ::= block | if_stmt | blocking_assign | nonblocking_assign
 * block ::= 'begin' stmt* 'end'
 * ================================================================ */

static void parser_advance(Parser *p) {
    lexer_next(&p->lexer);
}

static TokenKind parser_current_kind(Parser *p) {
    return p->lexer.current.type;
}

static bool parser_check(Parser *p, TokenKind kind) {
    return parser_current_kind(p) == kind;
}

static bool parser_match(Parser *p, TokenKind kind) {
    if (parser_check(p, kind)) {
        parser_advance(p);
        return true;
    }
    return false;
}

static bool parser_expect(Parser *p, TokenKind kind, const char *expected) {
    if (parser_match(p, kind)) return true;
    char msg[256];
    snprintf(msg, sizeof(msg), "expected %s, got %s",
             expected, token_kind_name(parser_current_kind(p)));
    parser_error(p, msg);
    if (!p->panic_mode) {
        p->panic_mode = true;
    }
    return false;
}

/* ================================================================
 * L5: Pratt expression parser
 * Precedence climbing with inline null-denotation and left-denotation
 * ================================================================ */

AstNode *parser_parse_expr(Parser *p, int min_prec) {
    Token tok = p->lexer.current;
    parser_advance(p);

    /* Null denotation: prefix expressions */
    AstNode *left = NULL;
    switch (tok.type) {
        case TK_IDENT:
            left = ast_create_node(AST_EXPR_IDENT, tok.text);
            break;
        case TK_NUMBER:
            left = ast_create_node(AST_EXPR_NUMBER, tok.text);
            left->int_val  = tok.int_val;
            left->radix    = tok.radix;
            left->size_bits = tok.size_bits;
            break;
        case TK_MINUS:
            left = ast_create_node(AST_EXPR_UNARY, "-");
            AstNode *inner = parser_parse_expr(p, 110);
            if (inner) ast_add_child(left, inner);
            return left;
        case TK_BANG:
            left = ast_create_node(AST_EXPR_UNARY, "!");
            inner = parser_parse_expr(p, 110);
            if (inner) ast_add_child(left, inner);
            return left;
        case TK_TILDE:
            left = ast_create_node(AST_EXPR_UNARY, "~");
            inner = parser_parse_expr(p, 110);
            if (inner) ast_add_child(left, inner);
            return left;
        case TK_AMPER:
            left = ast_create_node(AST_EXPR_UNARY, "&");
            inner = parser_parse_expr(p, 110);
            if (inner) ast_add_child(left, inner);
            return left;
        case TK_LPAREN:
            left = parser_parse_expr(p, 0);
            parser_expect(p, TK_RPAREN, ")");
            break;
        case TK_LBRACE: {
            /* Concatenation {a, b, c} */
            AstNode *concat = ast_create_node(AST_EXPR_CONCAT, "{}");
            if (!parser_check(p, TK_RBRACE)) {
                do {
                    AstNode *elem = parser_parse_expr(p, 0);
                    if (elem) ast_add_child(concat, elem);
                } while (parser_match(p, TK_COMMA));
            }
            parser_expect(p, TK_RBRACE, "}");
            return concat;
        }
        case TK_QUESTION: {
            /* Ternary: ? expr : expr */
            AstNode *ternary = ast_create_node(AST_EXPR_TERNARY, "?:");
            AstNode *true_expr = parser_parse_expr(p, 0);
            if (true_expr) ast_add_child(ternary, true_expr);
            parser_expect(p, TK_COLON, ":");
            AstNode *false_expr = parser_parse_expr(p, 0);
            if (false_expr) ast_add_child(ternary, false_expr);
            return ternary;
        }
        default:
            parser_error(p, "unexpected token in expression");
            return ast_create_node(AST_ERROR, "expr_error");
    }

    /* Left denotation: infix / postfix operators */
    while (!parser_check(p, TK_EOF) && !parser_check(p, TK_SEMICOLON) &&
           !parser_check(p, TK_COMMA) && !parser_check(p, TK_RPAREN) &&
           !parser_check(p, TK_RBRACKET) && !parser_check(p, TK_RBRACE) &&
           !parser_check(p, TK_COLON)) {

        Token op = p->lexer.current;
        int prec = token_precedence(op.type);

        if (prec == 0 || prec < min_prec) break;

        parser_advance(p);

        if (op.type == TK_QUESTION) {
            /* Ternary continuation */
            AstNode *ternary = ast_create_node(AST_EXPR_TERNARY, "?:");
            ast_add_child(ternary, left);
            AstNode *true_expr = parser_parse_expr(p, 0);
            if (true_expr) ast_add_child(ternary, true_expr);
            parser_expect(p, TK_COLON, ":");
            AstNode *false_expr = parser_parse_expr(p, prec - 1);
            if (false_expr) ast_add_child(ternary, false_expr);
            left = ternary;
        } else {
            /* Binary operators */
            AstNode *binary = ast_create_node(AST_EXPR_BINARY, token_kind_name(op.type));
            binary->int_val = (int)op.type;
            ast_add_child(binary, left);
            AstNode *right = parser_parse_expr(p, prec);
            if (right) ast_add_child(binary, right);
            left = binary;
        }
    }

    return left;
}

/* ================================================================
 * L2: Module parsing
 * ================================================================ */

AstNode *parser_parse_module(Parser *p) {
    if (!parser_expect(p, TK_MODULE, "module")) return NULL;

    Token name_tok = p->lexer.current;
    parser_expect(p, TK_IDENT, "module name");
    AstNode *mod = ast_create_node(AST_MODULE, name_tok.text);

    /* Port list */
    if (parser_match(p, TK_LPAREN)) {
        AstNode *port_list = parser_parse_port_list(p);
        if (port_list) ast_add_child(mod, port_list);
        parser_expect(p, TK_RPAREN, ")");
    }

    parser_expect(p, TK_SEMICOLON, ";");

    /* Module items */
    while (!parser_check(p, TK_ENDMODULE) && !parser_check(p, TK_EOF)) {
        TokenKind k = parser_current_kind(p);
        AstNode *item = NULL;

        switch (k) {
            case TK_WIRE:
            case TK_REG:
                item = parser_parse_net_decl(p);
                break;
            case TK_ASSIGN:
                item = parser_parse_assign(p);
                break;
            case TK_ALWAYS:
                item = parser_parse_always_block(p);
                break;
            case TK_INPUT:
            case TK_OUTPUT:
            case TK_INOUT:
                /* Port declarations inside module body (Verilog-2001 style) */
                item = parser_parse_port_decl(p);
                break;
            case TK_PARAMETER:
            case TK_LOCALPARAM: {
                parser_advance(p);
                AstNode *param = ast_create_node(AST_PARAM_DECL, p->lexer.current.text);
                parser_expect(p, TK_IDENT, "parameter name");
                parser_expect(p, TK_EQ, "=");
                if (parser_check(p, TK_NUMBER)) {
                    param->int_val = p->lexer.current.int_val;
                }
                item = param;
                parser_advance(p);
                parser_expect(p, TK_SEMICOLON, ";");
                break;
            }
            default:
                /* Skip unknown item for error recovery */
                parser_advance(p);
                break;
        }

        if (item) ast_add_child(mod, item);
    }

    parser_expect(p, TK_ENDMODULE, "endmodule");
    return mod;
}

AstNode *parser_parse_port_list(Parser *p) {
    AstNode *pl = ast_create_node(AST_PORT_LIST, "ports");
    do {
        AstNode *port = parser_parse_port_decl(p);
        if (port) ast_add_child(pl, port);
    } while (parser_match(p, TK_COMMA));
    return pl;
}

AstNode *parser_parse_port_decl(Parser *p) {
    int port_dir = 0;
    TokenKind dir_kind = parser_current_kind(p);

    switch (dir_kind) {
        case TK_INPUT:  port_dir = (int)TK_INPUT;  parser_advance(p); break;
        case TK_OUTPUT: port_dir = (int)TK_OUTPUT; parser_advance(p); break;
        case TK_INOUT:  port_dir = (int)TK_INOUT;  parser_advance(p); break;
        default: break;
    }

    /* Optional range: [MSB:LSB] */
    if (parser_match(p, TK_LBRACKET)) {
        /* Skip range for now */
        int depth = 1;
        while (depth > 0 && !parser_check(p, TK_EOF)) {
            if (parser_check(p, TK_LBRACKET)) depth++;
            if (parser_check(p, TK_RBRACKET)) depth--;
            parser_advance(p);
        }
    }

    AstNode *port = NULL;
    if (parser_check(p, TK_IDENT)) {
        port = ast_create_node(AST_PORT, p->lexer.current.text);
        port->int_val = port_dir;
        parser_advance(p);
    } else if (parser_check(p, TK_REG) || parser_check(p, TK_WIRE) ||
               parser_check(p, TK_LOGIC)) {
        parser_advance(p);
        /* Optional range after type keyword: e.g., reg [7:0] name */
        if (parser_match(p, TK_LBRACKET)) {
            int depth = 1;
            while (depth > 0 && !parser_check(p, TK_EOF)) {
                if (parser_check(p, TK_LBRACKET)) depth++;
                if (parser_check(p, TK_RBRACKET)) depth--;
                parser_advance(p);
            }
        }
        if (parser_check(p, TK_IDENT)) {
            port = ast_create_node(AST_PORT, p->lexer.current.text);
            port->int_val = port_dir;
            parser_advance(p);
        }
    }

    return port;
}

AstNode *parser_parse_net_decl(Parser *p) {
    AstNodeType decl_type = AST_NET_DECL;
    TokenKind k = parser_current_kind(p);

    if (k == TK_WIRE) {
        decl_type = AST_NET_DECL;
    } else if (k == TK_REG) {
        decl_type = AST_REG_DECL;
    } else if (k == TK_LOGIC) {
        decl_type = AST_NET_DECL;
    }
    parser_advance(p);

    /* Optional range */
    if (parser_match(p, TK_LBRACKET)) {
        /* Skip range */
        int depth = 1;
        while (depth > 0 && !parser_check(p, TK_EOF)) {
            if (parser_check(p, TK_LBRACKET)) depth++;
            if (parser_check(p, TK_RBRACKET)) depth--;
            parser_advance(p);
        }
    }

    AstNode *decl = NULL;
    if (parser_check(p, TK_IDENT)) {
        decl = ast_create_node(decl_type, p->lexer.current.text);
        parser_advance(p);
    }
    parser_expect(p, TK_SEMICOLON, ";");
    return decl;
}

AstNode *parser_parse_assign(Parser *p) {
    parser_expect(p, TK_ASSIGN, "assign");
    AstNode *assign = ast_create_node(AST_ASSIGN, "");

    /* LHS */
    if (parser_check(p, TK_IDENT)) {
        AstNode *lhs = ast_create_node(AST_EXPR_IDENT, p->lexer.current.text);
        ast_add_child(assign, lhs);
        parser_advance(p);
    }

    parser_expect(p, TK_EQ, "=");

    /* RHS expression */
    AstNode *rhs = parser_parse_expr(p, 0);
    if (rhs) ast_add_child(assign, rhs);

    parser_expect(p, TK_SEMICOLON, ";");
    return assign;
}

AstNode *parser_parse_sensitivity_list(Parser *p) {
    AstNode *sens = ast_create_node(AST_SENSITIVITY, "sensitivity");

    /* Handle posedge/negedge qualifiers */
    while (!parser_check(p, TK_RPAREN) && !parser_check(p, TK_EOF)) {
        AstNode *item = NULL;
        if (parser_match(p, TK_POSEDGE) || parser_match(p, TK_NEGEDGE)) {
            /* edge-triggered signal */
            if (parser_check(p, TK_IDENT)) {
                item = ast_create_node(AST_EXPR_IDENT, p->lexer.current.text);
                parser_advance(p);
            }
        } else if (parser_check(p, TK_IDENT)) {
            item = ast_create_node(AST_EXPR_IDENT, p->lexer.current.text);
            parser_advance(p);
        } else if (parser_match(p, TK_STAR)) {
            /* always @(*) — wildcard sensitivity */
            item = ast_create_node(AST_EXPR_IDENT, "*");
        }

        if (item) ast_add_child(sens, item);

        /* or or , are both valid separators in different Verilog styles */
        if (parser_match(p, TK_OR) || parser_match(p, TK_COMMA)) {
            continue;
        } else {
            break;
        }
    }

    return sens;
}

AstNode *parser_parse_always_block(Parser *p) {
    parser_expect(p, TK_ALWAYS, "always");
    parser_expect(p, TK_AT, "@");
    parser_expect(p, TK_LPAREN, "(");

    AstNode *always = ast_create_node(AST_ALWAYS, "always");
    AstNode *sens = parser_parse_sensitivity_list(p);
    if (sens) ast_add_child(always, sens);

    parser_expect(p, TK_RPAREN, ")");

    /* Statement body */
    AstNode *body = parser_parse_stmt(p);
    if (body) ast_add_child(always, body);

    return always;
}

AstNode *parser_parse_stmt(Parser *p) {
    if (parser_match(p, TK_BEGIN)) {
        AstNode *block = ast_create_node(AST_STMT_BLOCK, "begin_end");
        while (!parser_check(p, TK_END) && !parser_check(p, TK_EOF)) {
            AstNode *stmt = parser_parse_stmt(p);
            if (stmt) ast_add_child(block, stmt);
        }
        parser_expect(p, TK_END, "end");
        return block;
    }

    if (parser_check(p, TK_IF)) {
        parser_advance(p);
        parser_expect(p, TK_LPAREN, "(");
        AstNode *cond = parser_parse_expr(p, 0);
        parser_expect(p, TK_RPAREN, ")");

        AstNode *if_node = ast_create_node(AST_STMT_IF, "if");
        if (cond) ast_add_child(if_node, cond);

        AstNode *then_body = parser_parse_stmt(p);
        if (then_body) ast_add_child(if_node, then_body);

        if (parser_match(p, TK_ELSE)) {
            AstNode *else_body = parser_parse_stmt(p);
            if (else_body) ast_add_child(if_node, else_body);
        }

        return if_node;
    }

    if (parser_check(p, TK_CASE)) {
        parser_advance(p);
        parser_expect(p, TK_LPAREN, "(");
        AstNode *sel = parser_parse_expr(p, 0);
        parser_expect(p, TK_RPAREN, ")");

        AstNode *case_node = ast_create_node(AST_STMT_CASE, "case");
        if (sel) ast_add_child(case_node, sel);

        while (!parser_check(p, TK_ENDCASE) && !parser_check(p, TK_EOF)) {
            if (parser_match(p, TK_DEFAULT)) {
                parser_expect(p, TK_COLON, ":");
            } else {
                AstNode *val = parser_parse_expr(p, 0);
                parser_expect(p, TK_COLON, ":");
                if (val) ast_add_child(case_node, val);
            }
            AstNode *case_stmt = parser_parse_stmt(p);
            if (case_stmt) ast_add_child(case_node, case_stmt);
        }

        parser_expect(p, TK_ENDCASE, "endcase");
        return case_node;
    }

    /* Simple assignment: lhs = rhs; or lhs <= rhs; */
    if (parser_check(p, TK_IDENT)) {
        Token id_tok = p->lexer.current;
        parser_advance(p);

        if (parser_match(p, TK_EQ)) {
            AstNode *ba = ast_create_node(AST_STMT_BA, "=");
            AstNode *lhs = ast_create_node(AST_EXPR_IDENT, id_tok.text);
            ast_add_child(ba, lhs);
            AstNode *rhs = parser_parse_expr(p, 0);
            if (rhs) ast_add_child(ba, rhs);
            parser_expect(p, TK_SEMICOLON, ";");
            return ba;
        }

        if (parser_match(p, TK_LE)) {
            AstNode *nba = ast_create_node(AST_STMT_NBA, "<=");
            AstNode *lhs = ast_create_node(AST_EXPR_IDENT, id_tok.text);
            ast_add_child(nba, lhs);
            AstNode *rhs = parser_parse_expr(p, 0);
            if (rhs) ast_add_child(nba, rhs);
            parser_expect(p, TK_SEMICOLON, ";");
            return nba;
        }

        /* Not an assignment — skip */
        return NULL;
    }

    /* Skip unknown statement */
    parser_advance(p);
    return NULL;
}

/* ================================================================
 * Parser lifecycle
 * ================================================================ */

void parser_init(Parser *p, const char *source) {
    memset(p, 0, sizeof(*p));
    lexer_init(&p->lexer, source);
    p->root = ast_create_node(AST_ROOT, "root");
    p->panic_mode = false;
}

bool parser_parse(Parser *p) {
    p->panic_mode = false;

    /* Parse modules one by one until EOF */
    while (!parser_check(p, TK_EOF)) {
        if (parser_check(p, TK_MODULE)) {
            AstNode *mod = parser_parse_module(p);
            if (mod) {
                ast_add_child(p->root, mod);
            }
        } else {
            /* Skip tokens until next module or EOF */
            parser_advance(p);
        }
    }

    return p->error_count == 0;
}

AstNode *parser_get_root(Parser *p) {
    return p->root;
}

/* ================================================================
 * Error handling — L3: Panic-mode recovery
 * ================================================================ */

void parser_error(Parser *p, const char *msg) {
    if (p->error_count < PARSER_MAX_ERRORS) {
        int i = p->error_count++;
        strncpy(p->errors[i].msg, msg, 255);
        p->errors[i].pos = p->lexer.loc;
    }
}

void parser_print_errors(Parser *p) {
    for (int i = 0; i < p->error_count; i++) {
        fprintf(stderr, "Parser error at %u:%u: %s\n",
                p->errors[i].pos.line, p->errors[i].pos.col, p->errors[i].msg);
    }
}
