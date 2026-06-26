#ifndef HDL_AST_H
#define HDL_AST_H

#include <stdbool.h>
#include <stddef.h>

/**
 * hdl_ast.h — Abstract Syntax Tree for HDLs
 *
 * L1: AST node type taxonomy for Verilog/VHDL/SystemVerilog
 * L2: Tree construction, traversal, and free; child-list management
 * L3: Arena-based allocation for memory efficiency
 *     Visitor pattern dispatch tables
 * L4: Abstract grammar G = (N, T, P, S) for HDL subset
 *     Chomsky-style parse tree → AST reduction rules
 * L5: Tree rewriting for elaboration (parameter substitution,
 *     generate unrolling, port binding)
 */

#define AST_MAX_NAME     256
#define AST_MAX_CHILDREN  64

typedef enum {
    /* Module / entity level */
    AST_MODULE,
    AST_ENTITY,
    AST_ARCHITECTURE,
    AST_INTERFACE,
    AST_PACKAGE,

    /* Port / signal declarations */
    AST_PORT_LIST,
    AST_PORT,
    AST_NET_DECL,
    AST_REG_DECL,
    AST_PARAM_DECL,

    /* Combinational logic */
    AST_ASSIGN,
    AST_ALWAYS,
    AST_ALWAYS_FF,
    AST_ALWAYS_COMB,
    AST_SENSITIVITY,

    /* Statements */
    AST_STMT_BLOCK,
    AST_STMT_IF,
    AST_STMT_CASE,
    AST_STMT_FOR,
    AST_STMT_WHILE,
    AST_STMT_NBA,
    AST_STMT_BA,

    /* Expressions */
    AST_EXPR_BINARY,
    AST_EXPR_UNARY,
    AST_EXPR_TERNARY,
    AST_EXPR_IDENT,
    AST_EXPR_NUMBER,
    AST_EXPR_STRING,
    AST_EXPR_CONCAT,
    AST_EXPR_RANGE,
    AST_EXPR_BIT_SELECT,
    AST_EXPR_FUNC_CALL,

    /* Special */
    AST_ERROR,
    AST_ROOT
} AstNodeType;

typedef struct AstNode {
    AstNodeType     type;
    char            name[AST_MAX_NAME];
    struct AstNode *children[AST_MAX_CHILDREN];
    int             child_count;
    int             int_val;
    double          real_val;
    bool            is_signed;
    unsigned        radix;
    unsigned        size_bits;

    /* Source location for error reporting */
    int             src_line;
    int             src_col;

    /* Symbol resolution (L3: scoped name lookup) */
    struct AstNode *symbol_ref;
} AstNode;

/**
 * L5: Visitor interface for AST traversal
 * Implements the GoF Visitor pattern specialized for HDL ASTs
 */
typedef struct AstVisitor AstVisitor;
typedef void (*AstVisitFn)(AstVisitor *v, AstNode *node);

struct AstVisitor {
    AstVisitFn  pre_visit;
    AstVisitFn  post_visit;
    void       *user_data;
};

/* --- Construction --- */
AstNode *ast_create_node(AstNodeType type, const char *name);
void     ast_add_child(AstNode *parent, AstNode *child);
AstNode *ast_set_child(AstNode *parent, int idx, AstNode *child);

/* --- Query --- */
AstNode *ast_find_child_by_type(const AstNode *parent, AstNodeType type);
AstNode *ast_find_child_by_name(const AstNode *parent, const char *name);
int      ast_count_type(const AstNode *parent, AstNodeType type);

/* --- Traversal (L5: Depth-first visitor) --- */
void     ast_visit(AstNode *root, AstVisitor *visitor);
void     ast_walk_preorder(AstNode *root, void (*fn)(AstNode *, void *), void *ctx);
void     ast_walk_postorder(AstNode *root, void (*fn)(AstNode *, void *), void *ctx);

/* --- Serialization (L3: DOT format for graphviz) --- */
void     ast_dump_dot(AstNode *root, const char *filename);

/* --- Debug --- */
void     ast_print(AstNode *root, int indent);

/* --- Lifecycle --- */
void     ast_free(AstNode *root);

#endif
