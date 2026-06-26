#include "hdl_lexer.h"
#include "hdl_parser.h"
#include "hdl_ast.h"
#include "hdl_elaborate.h"
#include "hdl_codegen.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  TEST: %s ... ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while(0)
#define CHECK_INT_EQ(actual, expected, msg) \
    do { \
        if ((actual) == (expected)) PASS(); \
        else { printf("FAIL: %s (got %d, expected %d)\n", msg, actual, expected); tests_failed++; } \
    } while(0)

static void test_lexer_keywords(void) {
    TEST("lexer recognizes keywords");
    Lexer l;
    lexer_init(&l, "module wire reg assign always begin end");
    CHECK(l.current.type == TK_MODULE, "first token should be MODULE");
    lexer_next(&l);
    CHECK(l.current.type == TK_WIRE, "second token should be WIRE");
    lexer_next(&l);
    CHECK(l.current.type == TK_REG, "third token should be REG");
}

static void test_lexer_identifiers(void) {
    TEST("lexer recognizes identifiers");
    Lexer l;
    lexer_init(&l, "my_signal clk rst_n data_in");
    CHECK(l.current.type == TK_IDENT, "first should be IDENT");
    CHECK(strcmp(l.current.text, "my_signal") == 0, "ident text matches");
}

static void test_lexer_numbers(void) {
    TEST("lexer recognizes numbers");
    Lexer l;
    lexer_init(&l, "42 8'hFF 16'd1000");
    CHECK(l.current.type == TK_NUMBER, "first should be NUMBER");
    CHECK_INT_EQ(l.current.int_val, 42, "decimal value");
    lexer_next(&l);
    CHECK(l.current.type == TK_NUMBER, "second should be NUMBER");
    lexer_next(&l);
    CHECK(l.current.type == TK_NUMBER, "third should be NUMBER");
}

static void test_lexer_operators(void) {
    TEST("lexer recognizes operators");
    Lexer l;
    lexer_init(&l, "+ - * / && || == != <= >= << >>");
    CHECK(l.current.type == TK_PLUS, "+");
    lexer_next(&l); CHECK(l.current.type == TK_MINUS, "-");
    lexer_next(&l); CHECK(l.current.type == TK_STAR, "*");
    lexer_next(&l); CHECK(l.current.type == TK_SLASH, "/");
    lexer_next(&l); CHECK(l.current.type == TK_ANDAND, "&&");
    lexer_next(&l); CHECK(l.current.type == TK_PIPEPIPE, "||");
    lexer_next(&l); CHECK(l.current.type == TK_EQEQ, "==");
    lexer_next(&l); CHECK(l.current.type == TK_NEQ, "!=");
    lexer_next(&l); CHECK(l.current.type == TK_LE, "<=");
    lexer_next(&l); CHECK(l.current.type == TK_GE, ">=");
    lexer_next(&l); CHECK(l.current.type == TK_LSHIFT, "<<");
    lexer_next(&l); CHECK(l.current.type == TK_RSHIFT, ">>");
}

static void test_parser_simple_module(void) {
    TEST("parser creates AST for simple module");
    Parser p;
    parser_init(&p, "module test (input a, output b); assign b = a; endmodule");
    CHECK(parser_parse(&p), "parse succeeded");
    AstNode *root = parser_get_root(&p);
    CHECK(root != NULL, "root exists");
    CHECK(root->child_count >= 1, "has at least one module");
    ast_free(root);
}

static void test_parser_counter(void) {
    TEST("parser handles counter module");
    const char *src =
        "module counter (input clk, input rst_n, output reg [7:0] count);\n"
        "  wire [7:0] next_count;\n"
        "  assign next_count = count + 1;\n"
        "  always @(posedge clk or negedge rst_n) begin\n"
        "    if (!rst_n) count <= 8'd0;\n"
        "    else count <= next_count;\n"
        "  end\n"
        "endmodule\n";
    Parser p;
    parser_init(&p, src);
    CHECK(parser_parse(&p), "counter parse succeeded");
    AstNode *root = parser_get_root(&p);
    CHECK(root->child_count == 1, "one module parsed");
    ast_free(root);
}

static void test_ast_create(void) {
    TEST("AST node creation and tree building");
    AstNode *mod = ast_create_node(AST_MODULE, "test");
    CHECK(mod != NULL, "module created");
    CHECK(mod->type == AST_MODULE, "type is MODULE");
    CHECK(strcmp(mod->name, "test") == 0, "name is correct");

    AstNode *child = ast_create_node(AST_NET_DECL, "wire_a");
    ast_add_child(mod, child);
    CHECK_INT_EQ(mod->child_count, 1, "child count");
    CHECK(mod->children[0] == child, "child pointer correct");

    ast_free(mod);
}

static void test_elaborator_basic(void) {
    TEST("elaborator adds and finds modules");
    Elaborator e;
    elaborator_init(&e);
    CHECK(e.module_count == 0, "starts empty");

    AstNode *mod_ast = ast_create_node(AST_MODULE, "top");
    AstNode *pl = ast_create_node(AST_PORT_LIST, "");
    ast_add_child(mod_ast, pl);
    AstNode *port = ast_create_node(AST_PORT, "clk");
    port->int_val = (int)TK_INPUT;
    ast_add_child(pl, port);

    CHECK(elaborator_add_module(&e, mod_ast), "module added");
    CHECK_INT_EQ(e.module_count, 1, "module count 1");

    HdlModule *found = elaborator_find_module(&e, "top");
    CHECK(found != NULL, "top module found");
    CHECK(found->port_count == 1, "port count correct");

    ast_free(mod_ast);
}

static void test_codegen_verilog(void) {
    TEST("codegen emits valid Verilog");
    Parser p;
    parser_init(&p, "module test (input a, output b); assign b = a; endmodule");
    parser_parse(&p);
    AstNode *root = parser_get_root(&p);

    Codegen cg;
    codegen_init(&cg, CODEGEN_VERILOG);
    codegen_generate(&cg, root);

    const char *buf = codegen_get_buffer(&cg);
    CHECK(strstr(buf, "module test") != NULL, "contains module declaration");
    CHECK(strstr(buf, "assign") != NULL, "contains assign statement");
    CHECK(strstr(buf, "endmodule") != NULL, "contains endmodule");
    ast_free(root);
}

static void test_codegen_netlist(void) {
    TEST("codegen emits netlist format");
    Parser p;
    parser_init(&p, "module test (input a, output b); wire w; assign w = a; assign b = w; endmodule");
    parser_parse(&p);
    AstNode *root = parser_get_root(&p);

    Codegen cg;
    codegen_init(&cg, CODEGEN_NETLIST);
    codegen_generate(&cg, root);

    const char *buf = codegen_get_buffer(&cg);
    CHECK(strstr(buf, "[MODULE] test") != NULL, "netlist has module header");
    CHECK(strstr(buf, "NET") != NULL, "netlist has net declaration");
    ast_free(root);
}

int main(void) {
    printf("====== mini-hdl-lang Tests ======\n\n");

    printf("--- Lexer Tests ---\n");
    test_lexer_keywords();
    test_lexer_identifiers();
    test_lexer_numbers();
    test_lexer_operators();

    printf("\n--- Parser Tests ---\n");
    test_parser_simple_module();
    test_parser_counter();

    printf("\n--- AST Tests ---\n");
    test_ast_create();

    printf("\n--- Elaborator Tests ---\n");
    test_elaborator_basic();

    printf("\n--- Codegen Tests ---\n");
    test_codegen_verilog();
    test_codegen_netlist();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
