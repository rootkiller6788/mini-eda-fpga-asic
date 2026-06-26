#ifndef HDL_LEXER_H
#define HDL_LEXER_H

#include <stdbool.h>
#include <stddef.h>

/**
 * hdl_lexer.h — HDL Lexer / Tokenizer
 *
 * L1: Token types for Verilog/SystemVerilog/VHDL subset
 *     IEEE 1364-2001 Verilog lexical conventions
 *     IEEE 1800-2017 SystemVerilog lexical extensions
 * L2: Keyword recognition, identifier extraction, number literal parsing
 * L3: Single-pass lexer with lookahead, location tracking for error reporting
 * L4: IEEE Std 1364-2001 §2 (Lexical conventions)
 *     IEEE Std 1076-2008 §14 (VHDL lexical elements)
 */

#define LEXER_MAX_TEXT 512
#define LEXER_MAX_ERRORS 32

typedef enum {
    /* Structural keywords */
    TK_MODULE, TK_ENDMODULE, TK_INPUT, TK_OUTPUT, TK_INOUT,
    TK_WIRE, TK_REG, TK_ASSIGN, TK_ALWAYS, TK_BEGIN, TK_END,
    TK_IF, TK_ELSE, TK_CASE, TK_ENDCASE, TK_DEFAULT,
    TK_POSEDGE, TK_NEGEDGE, TK_OR,
    TK_PARAMETER, TK_LOCALPARAM, TK_GENERATE, TK_ENDGENERATE,

    /* SystemVerilog extensions */
    TK_LOGIC, TK_ALWAYS_FF, TK_ALWAYS_COMB, TK_ALWAYS_LATCH,
    TK_ENUM, TK_STRUCT, TK_PACKAGE, TK_IMPORT,
    TK_INTERFACE, TK_MODPORT, TK_ASSERT,

    /* VHDL keywords */
    TK_ENTITY, TK_ARCHITECTURE, TK_SIGNAL, TK_VARIABLE,
    TK_PROCESS, TK_COMPONENT, TK_PORT, TK_GENERIC,
    TK_STD_LOGIC, TK_STD_LOGIC_VECTOR,

    /* Operators and delimiters */
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_LSHIFT, TK_RSHIFT, TK_LT, TK_GT, TK_LE, TK_GE,
    TK_EQEQ, TK_NEQ, TK_AND, TK_PIPE, TK_ANDAND, TK_PIPEPIPE,
    TK_BANG, TK_TILDE, TK_CARET, TK_AMPER,
    TK_EQ, TK_COLON, TK_SEMICOLON, TK_COMMA, TK_DOT,
    TK_LPAREN, TK_RPAREN, TK_LBRACKET, TK_RBRACKET,
    TK_LBRACE, TK_RBRACE, TK_AT, TK_HASH, TK_QUESTION,
    TK_SINGLEQUOTE,

    /* Value tokens */
    TK_IDENT, TK_NUMBER, TK_STRING, TK_REAL,
    TK_EOF, TK_ERROR
} TokenKind;

typedef struct {
    char         text[LEXER_MAX_TEXT];
    int          text_len;
} LexerString;

typedef struct {
    unsigned     line;
    unsigned     col;
} LexerPos;

typedef struct {
    TokenKind    type;
    char         text[LEXER_MAX_TEXT];
    int          int_val;
    double       real_val;
    unsigned     radix;
    unsigned     size_bits;
    bool         is_signed;
    LexerPos     pos;
} Token;

typedef struct {
    const char  *source;
    int          pos;
    int          len;
    Token        current;
    Token        lookahead;
    bool         has_lookahead;
    LexerPos     loc;
    int          error_count;
    struct {
        char msg[256];
        LexerPos pos;
    } errors[LEXER_MAX_ERRORS];
} Lexer;

void lexer_init(Lexer *l, const char *source);
void lexer_next(Lexer *l);
Token lexer_peek(Lexer *l);
const char *token_kind_name(TokenKind kind);
bool token_is_keyword(TokenKind kind);
bool token_is_operator(TokenKind kind);
int  token_precedence(TokenKind kind);
bool lexer_parse_verilog_number(const char *text, int *out_val,
                                unsigned *out_radix, unsigned *out_bits);
void lexer_error(Lexer *l, const char *msg);
void lexer_print_errors(Lexer *l);

#endif
