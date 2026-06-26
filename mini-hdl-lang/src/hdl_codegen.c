#include "hdl_codegen.h"
#include "hdl_lexer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ================================================================
 * L4: IEEE Std 1364-2001 §4 — Source text emission
 *
 * Generated Verilog must conform to the lexical and syntactical rules
 * defined in §2-§4 of the standard. This includes proper keyword
 * capitalization, semicolon placement, and expression formatting.
 *
 * VHDL output follows IEEE Std 1076-2008 syntax.
 *
 * Netlist format is a custom human-readable representation for
 * debugging and tool integration.
 * ================================================================ */

/* ================================================================
 * L3: Output buffer management
 * ================================================================ */

static void buf_append(Codegen *cg, const char *s) {
    if (!s) return;
    int len = (int)strlen(s);
    if (cg->buf_len + len >= CODEGEN_MAX_BUF - 1) return;
    memcpy(cg->buf + cg->buf_len, s, (size_t)len);
    cg->buf_len += len;
    cg->buf[cg->buf_len] = '\0';
}

static void buf_append_char(Codegen *cg, char c) {
    if (cg->buf_len >= CODEGEN_MAX_BUF - 2) return;
    cg->buf[cg->buf_len++] = c;
    cg->buf[cg->buf_len] = '\0';
}

void codegen_write(Codegen *cg, const char *text) {
    buf_append(cg, text);
}

void codegen_write_line(Codegen *cg, const char *text) {
    for (int i = 0; i < cg->indent; i++) buf_append(cg, "  ");
    buf_append(cg, text);
    buf_append(cg, "\n");
}

void codegen_write_fmt(Codegen *cg, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char tmp[1024];
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    buf_append(cg, tmp);
}

void codegen_indent(Codegen *cg) {
    if (cg->indent < CODEGEN_MAX_INDENT) cg->indent++;
}

void codegen_dedent(Codegen *cg) {
    if (cg->indent > 0) cg->indent--;
}

void codegen_newline(Codegen *cg) {
    buf_append_char(cg, '\n');
}

/* ================================================================
 * Lifecycle
 * ================================================================ */

void codegen_init(Codegen *cg, CodegenTarget target) {
    memset(cg, 0, sizeof(*cg));
    cg->target = target;
    cg->fold_constants = true;
    cg->prune_dead_code = false;
}

const char *codegen_get_buffer(const Codegen *cg) {
    return cg->buf;
}

void codegen_generate(Codegen *cg, AstNode *root) {
    switch (cg->target) {
        case CODEGEN_VERILOG:  codegen_emit_verilog(cg, root);  break;
        case CODEGEN_VHDL:     codegen_emit_vhdl(cg, root);     break;
        case CODEGEN_NETLIST:  codegen_emit_netlist(cg, root);  break;
        case CODEGEN_C_MODEL:  codegen_emit_c_model(cg, root);  break;
        case CODEGEN_JSON:     /* fall through for now */        break;
    }
}

/* ================================================================
 * Expression codegen — L2: Lower AST expressions to text
 *
 * Handles operator symbol mapping for each target language:
 *   Verilog: ~ & | ^  && || << >>
 *   VHDL:     not and or xor  and or sll srl
 * ================================================================ */

static const char *verilog_op_name(int kind) {
    switch ((TokenKind)kind) {
        case TK_PLUS:  return "+";   case TK_MINUS: return "-";
        case TK_STAR:  return "*";   case TK_SLASH:  return "/";
        case TK_PERCENT: return "%";
        case TK_LSHIFT: return "<<"; case TK_RSHIFT: return ">>";
        case TK_LT:    return "<";   case TK_GT:     return ">";
        case TK_LE:    return "<=";  case TK_GE:     return ">=";
        case TK_EQEQ:  return "==";  case TK_NEQ:    return "!=";
        case TK_AMPER: return "&";   case TK_PIPE:   return "|";
        case TK_CARET: return "^";   case TK_TILDE:  return "~";
        case TK_ANDAND: return "&&"; case TK_PIPEPIPE: return "||";
        case TK_BANG:  return "!";   case TK_SINGLEQUOTE: return "'";
        default: return "?";
    }
}

static const char *vhdl_op_name(int kind) {
    switch ((TokenKind)kind) {
        case TK_PLUS:  return "+";   case TK_MINUS: return "-";
        case TK_STAR:  return "*";   case TK_SLASH:  return "/";
        case TK_ANDAND: case TK_AMPER: return "and";
        case TK_PIPEPIPE: case TK_PIPE: return "or";
        case TK_CARET: return "xor";
        case TK_BANG:  case TK_TILDE: return "not";
        case TK_EQEQ:  return "=";
        case TK_NEQ:   return "/=";
        case TK_LT:    return "<";   case TK_GT:     return ">";
        case TK_LE:    return "<=";  case TK_GE:     return ">=";
        default: return "?";
    }
}

void codegen_emit_expr(Codegen *cg, AstNode *expr) {
    if (!expr) {
        buf_append(cg, "?");
        return;
    }

    switch (expr->type) {
        case AST_EXPR_IDENT:
            buf_append(cg, expr->name);
            break;

        case AST_EXPR_NUMBER:
            if (cg->target == CODEGEN_VHDL && expr->radix == 2) {
                codegen_write_fmt(cg, "\"%s\"", expr->name);
            } else if (expr->radix != 10 && cg->target == CODEGEN_VERILOG) {
                const char *base = (expr->radix == 16) ? "h" :
                                   (expr->radix == 8)  ? "o" :
                                   (expr->radix == 2)  ? "b" : "d";
                codegen_write_fmt(cg, "%d'%s%s",
                                  expr->size_bits, base, expr->name);
            } else {
                codegen_write_fmt(cg, "%d", expr->int_val);
            }
            break;

        case AST_EXPR_BINARY: {
            bool need_paren = (expr->child_count >= 2);
            if (need_paren) buf_append_char(cg, '(');
            codegen_emit_expr(cg, expr->children[0]);
            buf_append_char(cg, ' ');
            const char *op = (cg->target == CODEGEN_VHDL)
                ? vhdl_op_name(expr->int_val)
                : verilog_op_name(expr->int_val);
            buf_append(cg, op);
            buf_append_char(cg, ' ');
            codegen_emit_expr(cg, expr->children[1]);
            if (need_paren) buf_append_char(cg, ')');
            break;
        }

        case AST_EXPR_UNARY: {
            const char *op = (cg->target == CODEGEN_VHDL)
                ? vhdl_op_name(expr->int_val)
                : verilog_op_name(expr->int_val);
            buf_append(cg, op);
            if (expr->child_count > 0) {
                codegen_emit_expr(cg, expr->children[0]);
            }
            break;
        }

        case AST_EXPR_TERNARY: {
            buf_append_char(cg, '(');
            if (expr->child_count > 0) codegen_emit_expr(cg, expr->children[0]);
            buf_append(cg, " ? ");
            if (expr->child_count > 1) codegen_emit_expr(cg, expr->children[1]);
            buf_append(cg, " : ");
            if (expr->child_count > 2) codegen_emit_expr(cg, expr->children[2]);
            buf_append_char(cg, ')');
            break;
        }

        case AST_EXPR_CONCAT: {
            if (cg->target == CODEGEN_VERILOG) {
                buf_append_char(cg, '{');
                for (int i = 0; i < expr->child_count; i++) {
                    if (i > 0) buf_append(cg, ", ");
                    codegen_emit_expr(cg, expr->children[i]);
                }
                buf_append_char(cg, '}');
            } else if (cg->target == CODEGEN_VHDL) {
                for (int i = 0; i < expr->child_count; i++) {
                    if (i > 0) buf_append(cg, " & ");
                    codegen_emit_expr(cg, expr->children[i]);
                }
            }
            break;
        }

        default:
            buf_append(cg, expr->name[0] ? expr->name : "?");
            break;
    }
}

/* ================================================================
 * Statement codegen
 * ================================================================ */

void codegen_emit_stmt(Codegen *cg, AstNode *stmt) {
    if (!stmt) return;

    for (int i = 0; i < cg->indent; i++) buf_append(cg, "  ");

    switch (stmt->type) {
        case AST_STMT_BLOCK:
            codegen_write_line(cg, "begin");
            codegen_indent(cg);
            for (int i = 0; i < stmt->child_count; i++) {
                codegen_emit_stmt(cg, stmt->children[i]);
            }
            codegen_dedent(cg);
            codegen_write_line(cg, "end");
            break;

        case AST_STMT_IF:
            buf_append(cg, "if (");
            if (stmt->child_count > 0) codegen_emit_expr(cg, stmt->children[0]);
            buf_append(cg, ")");
            codegen_newline(cg);
            if (stmt->child_count > 1) {
                codegen_indent(cg);
                codegen_emit_stmt(cg, stmt->children[1]);
                codegen_dedent(cg);
            }
            if (stmt->child_count > 2) {
                codegen_write_line(cg, "else");
                codegen_indent(cg);
                codegen_emit_stmt(cg, stmt->children[2]);
                codegen_dedent(cg);
            }
            break;

        case AST_STMT_BA:
            /* Blocking assignment */
            if (stmt->child_count > 0) codegen_emit_expr(cg, stmt->children[0]);
            buf_append(cg, " = ");
            if (stmt->child_count > 1) codegen_emit_expr(cg, stmt->children[1]);
            buf_append(cg, ";\n");
            break;

        case AST_STMT_NBA:
            /* Non-blocking assignment */
            if (stmt->child_count > 0) codegen_emit_expr(cg, stmt->children[0]);
            buf_append(cg, " <= ");
            if (stmt->child_count > 1) codegen_emit_expr(cg, stmt->children[1]);
            buf_append(cg, ";\n");
            break;

        default:
            buf_append(cg, "// <stmt>\n");
            break;
    }
}

void codegen_emit_assign(Codegen *cg, AstNode *assign) {
    codegen_write(cg, "  assign ");
    if (assign->child_count > 0) codegen_emit_expr(cg, assign->children[0]);
    codegen_write(cg, " = ");
    if (assign->child_count > 1) codegen_emit_expr(cg, assign->children[1]);
    codegen_write(cg, ";\n");
}

void codegen_emit_if(Codegen *cg, AstNode *if_stmt) {
    codegen_emit_stmt(cg, if_stmt);
}

void codegen_emit_case(Codegen *cg, AstNode *case_stmt) {
    codegen_write_line(cg, "case (...)");
    for (int i = 0; i < case_stmt->child_count; i++) {
        codegen_emit_stmt(cg, case_stmt->children[i]);
    }
    codegen_write_line(cg, "endcase");
}

void codegen_emit_always(Codegen *cg, AstNode *always) {
    codegen_write(cg, "  always @(");
    AstNode *sens = ast_find_child_by_type(always, AST_SENSITIVITY);
    if (sens) {
        for (int i = 0; i < sens->child_count; i++) {
            if (i > 0) codegen_write(cg, " or ");
            if (sens->children[i]) {
                codegen_emit_expr(cg, sens->children[i]);
            }
        }
    }
    codegen_write(cg, ")\n");

    /* Emit always body */
    for (int i = 0; i < always->child_count; i++) {
        if (always->children[i] && always->children[i]->type != AST_SENSITIVITY) {
            codegen_emit_stmt(cg, always->children[i]);
        }
    }
}

void codegen_emit_block(Codegen *cg, AstNode *block) {
    codegen_write_line(cg, "begin");
    codegen_indent(cg);
    for (int i = 0; i < block->child_count; i++) {
        codegen_emit_stmt(cg, block->children[i]);
    }
    codegen_dedent(cg);
    codegen_write_line(cg, "end");
}

/* ================================================================
 * Target-specific top-level emitters
 * ================================================================ */

void codegen_emit_verilog(Codegen *cg, AstNode *root) {
    if (!root) return;

    codegen_write_line(cg, "// Generated by mini-hdl-lang");
    codegen_write_line(cg, "// Target: Verilog (IEEE 1364-2001)");
    codegen_newline(cg);

    for (int i = 0; i < root->child_count; i++) {
        AstNode *child = root->children[i];
        if (!child) continue;

        if (child->type == AST_MODULE) {
            /* Module header */
            codegen_write_fmt(cg, "module %s", child->name);

            /* Port list */
            AstNode *pl = ast_find_child_by_type(child, AST_PORT_LIST);
            if (pl) {
                codegen_write(cg, " (");
                for (int j = 0; j < pl->child_count; j++) {
                    if (j > 0) codegen_write(cg, ", ");
                    AstNode *port = pl->children[j];
                    if (port && port->type == AST_PORT) {
                        const char *dir = "";
                        switch ((TokenKind)port->int_val) {
                            case TK_INPUT:  dir = "input ";  break;
                            case TK_OUTPUT: dir = "output "; break;
                            case TK_INOUT:  dir = "inout ";  break;
                            default: break;
                        }
                        codegen_write_fmt(cg, "%s%s", dir, port->name);
                    }
                }
                codegen_write(cg, ")");
            }
            codegen_write(cg, ";\n");

            /* Module body items */
            for (int j = 0; j < child->child_count; j++) {
                AstNode *item = child->children[j];
                if (!item || item->type == AST_PORT_LIST) continue;

                switch (item->type) {
                    case AST_NET_DECL:
                        codegen_write_fmt(cg, "  wire %s;\n", item->name);
                        break;
                    case AST_REG_DECL:
                        codegen_write_fmt(cg, "  reg %s;\n", item->name);
                        break;
                    case AST_PARAM_DECL:
                        codegen_write_fmt(cg, "  parameter %s = %d;\n",
                                          item->name, item->int_val);
                        break;
                    case AST_ASSIGN:
                        codegen_emit_assign(cg, item);
                        break;
                    case AST_ALWAYS:
                        codegen_emit_always(cg, item);
                        break;
                    default:
                        break;
                }
            }

            codegen_write_line(cg, "endmodule");
            codegen_newline(cg);
        }
    }
}

void codegen_emit_vhdl(Codegen *cg, AstNode *root) {
    if (!root) return;

    codegen_write_line(cg, "-- Generated by mini-hdl-lang");
    codegen_write_line(cg, "-- Target: VHDL (IEEE 1076-2008)");
    codegen_newline(cg);

    /* VHDL uses library/use clauses */
    codegen_write_line(cg, "library IEEE;");
    codegen_write_line(cg, "use IEEE.STD_LOGIC_1164.ALL;");
    codegen_newline(cg);

    for (int i = 0; i < root->child_count; i++) {
        AstNode *child = root->children[i];
        if (!child || child->type != AST_MODULE) continue;

        codegen_write_fmt(cg, "entity %s is\n", child->name);

        AstNode *pl = ast_find_child_by_type(child, AST_PORT_LIST);
        if (pl && pl->child_count > 0) {
            codegen_write_line(cg, "  port (");
            for (int j = 0; j < pl->child_count; j++) {
                AstNode *port = pl->children[j];
                if (!port || port->type != AST_PORT) continue;
                const char *dir_str = "";
                switch ((TokenKind)port->int_val) {
                    case TK_INPUT:  dir_str = "in";    break;
                    case TK_OUTPUT: dir_str = "out";   break;
                    case TK_INOUT:  dir_str = "inout"; break;
                    default: break;
                }
                codegen_write_fmt(cg, "    %s : %s std_logic",
                                  port->name, dir_str);
                if (j < pl->child_count - 1) codegen_write(cg, ";");
                codegen_newline(cg);
            }
            codegen_write_line(cg, "  );");
        }

        codegen_write_fmt(cg, "end entity %s;\n\n", child->name);
        codegen_write_fmt(cg, "architecture behavioral of %s is\n", child->name);
        codegen_write_line(cg, "begin");
        codegen_write_fmt(cg, "end architecture behavioral;\n\n");
    }
}

void codegen_emit_netlist(Codegen *cg, AstNode *root) {
    if (!root) return;

    codegen_write_line(cg, "# Netlist generated by mini-hdl-lang");
    codegen_newline(cg);

    for (int i = 0; i < root->child_count; i++) {
        AstNode *child = root->children[i];
        if (!child || child->type != AST_MODULE) continue;

        codegen_write_fmt(cg, "[MODULE] %s\n", child->name);

        /* List ports */
        AstNode *pl = ast_find_child_by_type(child, AST_PORT_LIST);
        if (pl) {
            for (int j = 0; j < pl->child_count; j++) {
                AstNode *port = pl->children[j];
                if (port && port->type == AST_PORT) {
                    const char *dir = "IN";
                    switch ((TokenKind)port->int_val) {
                        case TK_OUTPUT: dir = "OUT"; break;
                        case TK_INOUT:  dir = "INOUT"; break;
                        default: break;
                    }
                    codegen_write_fmt(cg, "  PORT %s %s\n", dir, port->name);
                }
            }
        }

        /* List nets */
        for (int j = 0; j < child->child_count; j++) {
            AstNode *item = child->children[j];
            if (!item || item->type == AST_PORT_LIST) continue;
            if (item->type == AST_NET_DECL) {
                codegen_write_fmt(cg, "  NET wire %s\n", item->name);
            } else if (item->type == AST_REG_DECL) {
                codegen_write_fmt(cg, "  NET reg %s\n", item->name);
            } else if (item->type == AST_ASSIGN) {
                codegen_write(cg, "  ASSIGN ");
                if (item->child_count > 0) codegen_emit_expr(cg, item->children[0]);
                codegen_write(cg, " = ");
                if (item->child_count > 1) codegen_emit_expr(cg, item->children[1]);
                codegen_newline(cg);
            }
        }

        codegen_write_fmt(cg, "[ENDMODULE] %s\n\n", child->name);
    }
}

void codegen_emit_c_model(Codegen *cg, AstNode *root) {
    if (!root) return;

    codegen_write_line(cg, "/* C Model generated by mini-hdl-lang */");
    codegen_write_line(cg, "#include <stdint.h>");
    codegen_write_line(cg, "#include <stdbool.h>");
    codegen_newline(cg);

    for (int i = 0; i < root->child_count; i++) {
        AstNode *child = root->children[i];
        if (!child || child->type != AST_MODULE) continue;

        /* Emit as C function */
        AstNode *pl = ast_find_child_by_type(child, AST_PORT_LIST);

        codegen_write_fmt(cg, "void %s(", child->name);

        /* Ports become function parameters */
        bool first = true;
        if (pl) {
            for (int j = 0; j < pl->child_count; j++) {
                AstNode *port = pl->children[j];
                if (!port || port->type != AST_PORT) continue;
                if (!first) codegen_write(cg, ", ");
                const char *dir = "";
                switch ((TokenKind)port->int_val) {
                    case TK_INPUT:  dir = "";         break;
                    case TK_OUTPUT: dir = "/*out*/ "; break;
                    case TK_INOUT:  dir = "/*io*/ ";  break;
                    default: break;
                }
                codegen_write_fmt(cg, "%sint *%s", dir, port->name);
                first = false;
            }
        }
        codegen_write_line(cg, ") {");

        /* Body: convert assigns to C assignments */
        for (int j = 0; j < child->child_count; j++) {
            AstNode *item = child->children[j];
            if (!item || item->type == AST_PORT_LIST) continue;
            if (item->type == AST_ASSIGN) {
                codegen_write(cg, "  *");
                if (item->child_count > 0) codegen_emit_expr(cg, item->children[0]);
                codegen_write(cg, " = ");
                if (item->child_count > 1) codegen_emit_expr(cg, item->children[1]);
                codegen_write(cg, ";\n");
            }
        }

        codegen_write_line(cg, "}");
        codegen_newline(cg);
    }
}
