#include "hdl_lexer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ================================================================
 * L4: IEEE 1364-2001 §2.5 — Verilog number literal parsing
 *
 * Format:  [size]'[s|S]?[base][digits]
 *   size:  decimal digit sequence
 *   base:  b|B (binary), o|O (octal), d|D (decimal), h|H (hex)
 *   digits: x|X|z|Z|?|0-9|a-f|A-F (depends on base)
 *
 * Examples:  8'hFF, 16'd1000, 4'b1010, '1, 32'shDEAD
 * ================================================================ */

static bool is_verilog_digit(char c, unsigned radix) {
    if (c == 'x' || c == 'X' || c == 'z' || c == 'Z' || c == '?') return true;
    switch (radix) {
        case 2:  return (c == '0' || c == '1');
        case 8:  return (c >= '0' && c <= '7');
        case 10: return (c >= '0' && c <= '9');
        case 16: return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        default: return false;
    }
}

bool lexer_parse_verilog_number(const char *text, int *out_val,
                                unsigned *out_radix, unsigned *out_bits) {
    if (!text || !out_val || !out_radix || !out_bits) return false;

    const char *p = text;
    unsigned size_bits = 0;
    /* Parse optional size prefix */
    if (isdigit((unsigned char)*p)) {
        size_bits = (unsigned)strtoul(p, (char **)&p, 10);
        if (size_bits == 0) return false;
    }

    /* Parse quote */
    if (*p != '\'') return false;
    p++;

    /* Parse optional signed flag */
    if (*p == 's' || *p == 'S') {
        p++;
    }

    /* Parse radix */
    unsigned radix = 10;
    switch (*p) {
        case 'b': case 'B': radix = 2;  p++; break;
        case 'o': case 'O': radix = 8;  p++; break;
        case 'd': case 'D': radix = 10; p++; break;
        case 'h': case 'H': radix = 16; p++; break;
        default: radix = 10; break;
    }

    /* Parse digit sequence with X/Z awareness */
    int value = 0;
    bool has_unknown = false;
    while (*p && is_verilog_digit(*p, radix)) {
        if (*p == 'x' || *p == 'X' || *p == 'z' || *p == 'Z' || *p == '?') {
            has_unknown = true;
            value = (value << 4) | 0;
        } else if (*p == '_') {
            /* Ignore underscores per §2.5.1 */
        } else {
            int digit = isdigit((unsigned char)*p) ? (*p - '0') : (toupper((unsigned char)*p) - 'A' + 10);
            value = (value * (int)radix) + digit;
        }
        p++;
    }

    /* If no size given, infer minimum */
    if (size_bits == 0) {
        size_bits = 32;
    }

    *out_val   = has_unknown ? 0 : value;
    *out_radix = radix;
    *out_bits  = size_bits;
    return true;
}

/* ================================================================
 * L3: Single-pass lexer with keyword trie for O(len) lookup
 * ================================================================ */

static const struct {
    const char *kw;
    TokenKind   kind;
} keyword_table[] = {
    {"module",       TK_MODULE},
    {"endmodule",    TK_ENDMODULE},
    {"input",        TK_INPUT},
    {"output",       TK_OUTPUT},
    {"inout",        TK_INOUT},
    {"wire",         TK_WIRE},
    {"reg",          TK_REG},
    {"assign",       TK_ASSIGN},
    {"always",       TK_ALWAYS},
    {"begin",        TK_BEGIN},
    {"end",          TK_END},
    {"if",           TK_IF},
    {"else",         TK_ELSE},
    {"case",         TK_CASE},
    {"endcase",      TK_ENDCASE},
    {"default",      TK_DEFAULT},
    {"posedge",      TK_POSEDGE},
    {"negedge",      TK_NEGEDGE},
    {"or",           TK_OR},
    {"parameter",    TK_PARAMETER},
    {"localparam",   TK_LOCALPARAM},
    {"generate",     TK_GENERATE},
    {"endgenerate",  TK_ENDGENERATE},
    {"logic",        TK_LOGIC},
    {"always_ff",    TK_ALWAYS_FF},
    {"always_comb",  TK_ALWAYS_COMB},
    {"always_latch", TK_ALWAYS_LATCH},
    {"enum",         TK_ENUM},
    {"struct",       TK_STRUCT},
    {"package",      TK_PACKAGE},
    {"import",       TK_IMPORT},
    {"interface",    TK_INTERFACE},
    {"modport",      TK_MODPORT},
    {"assert",       TK_ASSERT},
    {"entity",       TK_ENTITY},
    {"architecture", TK_ARCHITECTURE},
    {"signal",       TK_SIGNAL},
    {"variable",     TK_VARIABLE},
    {"process",      TK_PROCESS},
    {"component",    TK_COMPONENT},
    {"port",         TK_PORT},
    {"generic",      TK_GENERIC},
    {NULL,           TK_ERROR}
};

static TokenKind lookup_keyword(const char *text, int len) {
    for (int i = 0; keyword_table[i].kw; i++) {
        if ((int)strlen(keyword_table[i].kw) == len &&
            strncmp(keyword_table[i].kw, text, (size_t)len) == 0) {
            return keyword_table[i].kind;
        }
    }
    return TK_IDENT;
}

/* ================================================================
 * L2: Character classification
 * ================================================================ */

static bool is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static bool is_ident_continue(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == '$';
}

/* ================================================================
 * L3: Location tracking
 * ================================================================ */

static LexerPos make_pos(unsigned line, unsigned col) {
    LexerPos p;
    p.line = line;
    p.col  = col;
    return p;
}

static void advance_pos(LexerPos *pos, char c) {
    if (c == '\n') {
        pos->line++;
        pos->col = 1;
    } else {
        pos->col++;
    }
}

/* ================================================================
 * Public API
 * ================================================================ */

void lexer_init(Lexer *l, const char *source) {
    memset(l, 0, sizeof(*l));
    l->source = source;
    l->len    = source ? (int)strlen(source) : 0;
    l->pos    = 0;
    l->loc    = make_pos(1, 1);
    l->has_lookahead = false;
    lexer_next(l);
}

static char lexer_peek_char(Lexer *l, int offset) {
    int idx = l->pos + offset;
    if (idx >= 0 && idx < l->len) return l->source[idx];
    return '\0';
}

static char lexer_advance(Lexer *l) {
    if (l->pos >= l->len) return '\0';
    char c = l->source[l->pos++];
    advance_pos(&l->loc, c);
    return c;
}

static void lexer_skip_whitespace(Lexer *l) {
    while (l->pos < l->len) {
        char c = l->source[l->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            lexer_advance(l);
        } else if (c == '/' && lexer_peek_char(l, 1) == '/') {
            while (l->pos < l->len && l->source[l->pos] != '\n') {
                l->pos++;
            }
        } else if (c == '/' && lexer_peek_char(l, 1) == '*') {
            l->pos += 2;
            while (l->pos < l->len - 1) {
                if (l->source[l->pos] == '*' && l->source[l->pos + 1] == '/') {
                    l->pos += 2;
                    break;
                }
                if (l->source[l->pos] == '\n') { l->loc.line++; l->loc.col = 1; }
                l->pos++;
            }
        } else {
            break;
        }
    }
}

static void lexer_read_token(Lexer *l, Token *tok) {
    memset(tok, 0, sizeof(*tok));
    lexer_skip_whitespace(l);

    if (l->pos >= l->len) {
        tok->type = TK_EOF;
        tok->pos  = l->loc;
        return;
    }

    LexerPos start_pos = l->loc;
    char c = l->source[l->pos];

    /* Identifier or keyword */
    if (is_ident_start(c)) {
        int start = l->pos;
        while (l->pos < l->len && is_ident_continue(l->source[l->pos])) {
            lexer_advance(l);
        }
        int len = l->pos - start;
        if (len >= LEXER_MAX_TEXT) len = LEXER_MAX_TEXT - 1;
        memcpy(tok->text, l->source + start, (size_t)len);
        tok->text[len] = '\0';
        tok->type = lookup_keyword(tok->text, len);
        tok->pos  = start_pos;
        return;
    }

    /* Number literal */
    if (isdigit((unsigned char)c)) {
        int start = l->pos;
        bool is_real = false;

        /* Check for sized literal pattern: digits ' [sS]? [bBoOdDhH] */
        bool maybe_sized = false;
        int  saved_pos = l->pos;

        while (l->pos < l->len && isdigit((unsigned char)l->source[l->pos])) {
            lexer_advance(l);
        }
        if (l->pos < l->len && l->source[l->pos] == '\'') {
            lexer_advance(l);
            if (l->pos < l->len && (l->source[l->pos] == 's' || l->source[l->pos] == 'S')) {
                lexer_advance(l);
            }
            if (l->pos < l->len && strchr("bBoOdDhH", l->source[l->pos])) {
                lexer_advance(l);
                maybe_sized = true;
                while (l->pos < l->len && (isxdigit((unsigned char)l->source[l->pos]) ||
                       l->source[l->pos] == 'x' || l->source[l->pos] == 'X' ||
                       l->source[l->pos] == 'z' || l->source[l->pos] == 'Z' ||
                       l->source[l->pos] == '_')) {
                    lexer_advance(l);
                }
            }
        }

        if (maybe_sized) {
            goto emit_number;
        }

        /* Unsized decimal or real */
        l->pos = saved_pos;
        while (l->pos < l->len && isdigit((unsigned char)l->source[l->pos])) {
            lexer_advance(l);
        }
        if (l->pos < l->len && l->source[l->pos] == '.') {
            is_real = true;
            lexer_advance(l);
            while (l->pos < l->len && isdigit((unsigned char)l->source[l->pos])) {
                lexer_advance(l);
            }
        }

emit_number:
        {
            int len = l->pos - start;
            if (len >= LEXER_MAX_TEXT) len = LEXER_MAX_TEXT - 1;
            memcpy(tok->text, l->source + start, (size_t)len);
            tok->text[len] = '\0';

            if (is_real) {
                tok->type = TK_REAL;
                tok->real_val = strtod(tok->text, NULL);
            } else {
                tok->type = TK_NUMBER;
                unsigned radix, bits;
                if (lexer_parse_verilog_number(tok->text, &tok->int_val, &radix, &bits)) {
                    tok->radix    = radix;
                    tok->size_bits = bits;
                } else {
                    tok->int_val = atoi(tok->text);
                    tok->radix    = 10;
                    tok->size_bits = 32;
                }
            }
            tok->pos = start_pos;
            return;
        }
    }

    /* String literal */
    if (c == '"') {
        lexer_advance(l); /* skip opening " */
        int start = l->pos;
        while (l->pos < l->len && l->source[l->pos] != '"' && l->source[l->pos] != '\n') {
            lexer_advance(l);
        }
        int len = l->pos - start;
        if (len >= LEXER_MAX_TEXT) len = LEXER_MAX_TEXT - 1;
        memcpy(tok->text, l->source + start, (size_t)len);
        tok->text[len] = '\0';
        tok->type = TK_STRING;
        if (l->pos < l->len && l->source[l->pos] == '"') lexer_advance(l);
        tok->pos = start_pos;
        return;
    }

    /* Operators and delimiters — L2: IEEE 1364-2001 §2.2 two-character lookahead */
    lexer_advance(l);
    tok->pos = start_pos;

    switch (c) {
        case '+': tok->type = TK_PLUS; break;
        case '-': tok->type = TK_MINUS; break;
        case '*': tok->type = TK_STAR; break;
        case '/': tok->type = TK_SLASH; break;
        case '%': tok->type = TK_PERCENT; break;
        case '(': tok->type = TK_LPAREN; break;
        case ')': tok->type = TK_RPAREN; break;
        case '[': tok->type = TK_LBRACKET; break;
        case ']': tok->type = TK_RBRACKET; break;
        case '{': tok->type = TK_LBRACE; break;
        case '}': tok->type = TK_RBRACE; break;
        case ':': tok->type = TK_COLON; break;
        case ';': tok->type = TK_SEMICOLON; break;
        case ',': tok->type = TK_COMMA; break;
        case '.': tok->type = TK_DOT; break;
        case '@': tok->type = TK_AT; break;
        case '#': tok->type = TK_HASH; break;
        case '?': tok->type = TK_QUESTION; break;
        case '\'': tok->type = TK_SINGLEQUOTE; break;
        case '=':
            if (lexer_peek_char(l, 0) == '=') { lexer_advance(l); tok->type = TK_EQEQ; }
            else { tok->type = TK_EQ; }
            break;
        case '!':
            if (lexer_peek_char(l, 0) == '=') { lexer_advance(l); tok->type = TK_NEQ; }
            else { tok->type = TK_BANG; }
            break;
        case '<':
            if (lexer_peek_char(l, 0) == '<') { lexer_advance(l); tok->type = TK_LSHIFT; }
            else if (lexer_peek_char(l, 0) == '=') { lexer_advance(l); tok->type = TK_LE; }
            else { tok->type = TK_LT; }
            break;
        case '>':
            if (lexer_peek_char(l, 0) == '>') { lexer_advance(l); tok->type = TK_RSHIFT; }
            else if (lexer_peek_char(l, 0) == '=') { lexer_advance(l); tok->type = TK_GE; }
            else { tok->type = TK_GT; }
            break;
        case '&':
            if (lexer_peek_char(l, 0) == '&') { lexer_advance(l); tok->type = TK_ANDAND; }
            else { tok->type = TK_AMPER; }
            break;
        case '|':
            if (lexer_peek_char(l, 0) == '|') { lexer_advance(l); tok->type = TK_PIPEPIPE; }
            else { tok->type = TK_PIPE; }
            break;
        case '^': tok->type = TK_CARET; break;
        case '~': tok->type = TK_TILDE; break;
        default:
            tok->type = TK_ERROR;
            tok->text[0] = c;
            tok->text[1] = '\0';
            lexer_error(l, "unrecognized character");
            break;
    }
}

void lexer_next(Lexer *l) {
    if (l->has_lookahead) {
        l->current = l->lookahead;
        l->has_lookahead = false;
        return;
    }
    lexer_read_token(l, &l->current);
}

Token lexer_peek(Lexer *l) {
    if (!l->has_lookahead) {
        lexer_read_token(l, &l->lookahead);
        l->has_lookahead = true;
    }
    return l->lookahead;
}

const char *token_kind_name(TokenKind kind) {
    static const char *names[] = {
        [TK_MODULE]="MODULE", [TK_ENDMODULE]="ENDMODULE",
        [TK_INPUT]="INPUT", [TK_OUTPUT]="OUTPUT", [TK_INOUT]="INOUT",
        [TK_WIRE]="WIRE", [TK_REG]="REG", [TK_ASSIGN]="ASSIGN",
        [TK_ALWAYS]="ALWAYS", [TK_BEGIN]="BEGIN", [TK_END]="END",
        [TK_IF]="IF", [TK_ELSE]="ELSE", [TK_CASE]="CASE",
        [TK_ENDCASE]="ENDCASE", [TK_DEFAULT]="DEFAULT",
        [TK_POSEDGE]="POSEDGE", [TK_NEGEDGE]="NEGEDGE", [TK_OR]="OR",
        [TK_PARAMETER]="PARAMETER", [TK_LOCALPARAM]="LOCALPARAM",
        [TK_GENERATE]="GENERATE", [TK_ENDGENERATE]="ENDGENERATE",
        [TK_LOGIC]="LOGIC", [TK_ALWAYS_FF]="ALWAYS_FF",
        [TK_ALWAYS_COMB]="ALWAYS_COMB", [TK_ALWAYS_LATCH]="ALWAYS_LATCH",
        [TK_ENUM]="ENUM", [TK_STRUCT]="STRUCT",
        [TK_PACKAGE]="PACKAGE", [TK_IMPORT]="IMPORT",
        [TK_INTERFACE]="INTERFACE", [TK_MODPORT]="MODPORT",
        [TK_ASSERT]="ASSERT",
        [TK_ENTITY]="ENTITY", [TK_ARCHITECTURE]="ARCHITECTURE",
        [TK_SIGNAL]="SIGNAL", [TK_VARIABLE]="VARIABLE",
        [TK_PROCESS]="PROCESS", [TK_COMPONENT]="COMPONENT",
        [TK_PORT]="PORT", [TK_GENERIC]="GENERIC",
        [TK_STD_LOGIC]="STD_LOGIC", [TK_STD_LOGIC_VECTOR]="STD_LOGIC_VECTOR",
        [TK_IDENT]="IDENT", [TK_NUMBER]="NUMBER",
        [TK_STRING]="STRING", [TK_REAL]="REAL",
        [TK_PLUS]="+", [TK_MINUS]="-", [TK_STAR]="*", [TK_SLASH]="/",
        [TK_PERCENT]="%", [TK_LSHIFT]="<<", [TK_RSHIFT]=">>",
        [TK_LT]="<", [TK_GT]=">", [TK_LE]="<=", [TK_GE]=">=",
        [TK_EQEQ]="==", [TK_NEQ]="!=", [TK_BANG]="!",
        [TK_TILDE]="~", [TK_CARET]="^", [TK_AMPER]="&", [TK_PIPE]="|",
        [TK_ANDAND]="&&", [TK_PIPEPIPE]="||",
        [TK_EQ]="=", [TK_COLON]=":", [TK_SEMICOLON]=";",
        [TK_COMMA]=",", [TK_DOT]=".", [TK_AT]="@", [TK_HASH]="#",
        [TK_QUESTION]="?", [TK_SINGLEQUOTE]="'",
        [TK_LPAREN]="(", [TK_RPAREN]=")", [TK_LBRACKET]="[",
        [TK_RBRACKET]="]", [TK_LBRACE]="{", [TK_RBRACE]="}",
        [TK_EOF]="EOF", [TK_ERROR]="ERROR"
    };
    if (kind < sizeof(names)/sizeof(names[0]) && names[kind]) return names[kind];
    return "UNKNOWN";
}

bool token_is_keyword(TokenKind kind) {
    return kind >= TK_MODULE && kind <= TK_STD_LOGIC_VECTOR;
}

bool token_is_operator(TokenKind kind) {
    return kind >= TK_PLUS && kind <= TK_SINGLEQUOTE;
}

/* L5: Operator precedence table for Pratt parser
 * Higher number = tighter binding
 * Based on IEEE 1364-2001 Table 4-1 */
int token_precedence(TokenKind kind) {
    switch (kind) {
        case TK_PIPEPIPE: return 10;
        case TK_ANDAND:   return 20;
        case TK_PIPE:     return 30;
        case TK_CARET:    return 40;
        case TK_AMPER:    return 50;
        case TK_EQEQ:
        case TK_NEQ:      return 60;
        case TK_LT: case TK_GT:
        case TK_LE: case TK_GE: return 70;
        case TK_LSHIFT:
        case TK_RSHIFT:   return 80;
        case TK_PLUS:
        case TK_MINUS:    return 90;
        case TK_STAR:
        case TK_SLASH:
        case TK_PERCENT:  return 100;
        case TK_BANG:
        case TK_TILDE:    return 110; /* unary */
        default: return 0;
    }
}

void lexer_error(Lexer *l, const char *msg) {
    if (l->error_count < LEXER_MAX_ERRORS) {
        int i = l->error_count++;
        strncpy(l->errors[i].msg, msg, 255);
        l->errors[i].pos = l->loc;
    }
}

void lexer_print_errors(Lexer *l) {
    for (int i = 0; i < l->error_count; i++) {
        fprintf(stderr, "Lexer error at %u:%u: %s\n",
                l->errors[i].pos.line, l->errors[i].pos.col, l->errors[i].msg);
    }
}
