#include "hdl_ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ================================================================
 * L3: Arena allocator for AST nodes
 *
 * All nodes are allocated from a single contiguous block to
 * improve cache locality and simplify deallocation.
 * This implements a simple bump-pointer arena.
 * ================================================================ */

#define AST_ARENA_CHUNK_SIZE (1024 * 1024)

typedef struct AstArena {
    char   *data;
    size_t  capacity;
    size_t  used;
    struct AstArena *next;
} AstArena;

static AstArena *arena_head = NULL;

static AstArena *arena_new_chunk(size_t min_size) {
    size_t size = min_size > AST_ARENA_CHUNK_SIZE ? min_size : AST_ARENA_CHUNK_SIZE;
    AstArena *a = (AstArena *)malloc(sizeof(AstArena) + size);
    if (!a) return NULL;
    a->data     = (char *)(a + 1);
    a->capacity = size;
    a->used     = 0;
    a->next     = arena_head;
    arena_head  = a;
    return a;
}

static void *arena_alloc(size_t size) {
    if (!arena_head || arena_head->used + size > arena_head->capacity) {
        if (!arena_new_chunk(size)) return NULL;
    }
    void *ptr = arena_head->data + arena_head->used;
    arena_head->used += size;
    return ptr;
}

static void arena_free_all(void) {
    while (arena_head) {
        AstArena *next = arena_head->next;
        free(arena_head);
        arena_head = next;
    }
}

/* ================================================================
 * L2: AST construction
 * ================================================================ */

AstNode *ast_create_node(AstNodeType type, const char *name) {
    AstNode *node = (AstNode *)arena_alloc(sizeof(AstNode));
    if (!node) return NULL;
    memset(node, 0, sizeof(*node));
    node->type = type;
    if (name) strncpy(node->name, name, AST_MAX_NAME - 1);
    node->is_signed = false;
    node->radix     = 10;
    node->size_bits = 32;
    return node;
}

void ast_add_child(AstNode *parent, AstNode *child) {
    if (!parent || !child) return;
    if (parent->child_count >= AST_MAX_CHILDREN) return;
    parent->children[parent->child_count++] = child;
}

AstNode *ast_set_child(AstNode *parent, int idx, AstNode *child) {
    if (!parent || idx < 0 || idx >= AST_MAX_CHILDREN || !child) return NULL;
    if (idx >= parent->child_count) {
        parent->child_count = idx + 1;
    }
    parent->children[idx] = child;
    return child;
}

/* ================================================================
 * L2: Tree queries
 * ================================================================ */

AstNode *ast_find_child_by_type(const AstNode *parent, AstNodeType type) {
    if (!parent) return NULL;
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] && parent->children[i]->type == type) {
            return parent->children[i];
        }
    }
    return NULL;
}

AstNode *ast_find_child_by_name(const AstNode *parent, const char *name) {
    if (!parent || !name) return NULL;
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] &&
            strcmp(parent->children[i]->name, name) == 0) {
            return parent->children[i];
        }
    }
    return NULL;
}

int ast_count_type(const AstNode *parent, AstNodeType type) {
    if (!parent) return 0;
    int count = 0;
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] && parent->children[i]->type == type) count++;
    }
    return count;
}

/* ================================================================
 * L5: Visitor pattern — GoF Visitor specialized for AST
 *
 * pre_visit  is called before children (top-down)
 * post_visit is called after children (bottom-up)
 * Either can be NULL.
 * ================================================================ */

void ast_visit(AstNode *root, AstVisitor *visitor) {
    if (!root || !visitor) return;

    if (visitor->pre_visit) {
        visitor->pre_visit(visitor, root);
    }

    for (int i = 0; i < root->child_count; i++) {
        ast_visit(root->children[i], visitor);
    }

    if (visitor->post_visit) {
        visitor->post_visit(visitor, root);
    }
}

void ast_walk_preorder(AstNode *root, void (*fn)(AstNode *, void *), void *ctx) {
    if (!root || !fn) return;
    fn(root, ctx);
    for (int i = 0; i < root->child_count; i++) {
        ast_walk_preorder(root->children[i], fn, ctx);
    }
}

void ast_walk_postorder(AstNode *root, void (*fn)(AstNode *, void *), void *ctx) {
    if (!root || !fn) return;
    for (int i = 0; i < root->child_count; i++) {
        ast_walk_postorder(root->children[i], fn, ctx);
    }
    fn(root, ctx);
}

/* ================================================================
 * L3: DOT graph output for visualization (Graphviz)
 * ================================================================ */

static const char *ast_type_name(AstNodeType t) {
    static const char *names[] = {
        [AST_MODULE]="module", [AST_ENTITY]="entity",
        [AST_ARCHITECTURE]="architecture", [AST_INTERFACE]="interface",
        [AST_PACKAGE]="package",
        [AST_PORT_LIST]="port_list", [AST_PORT]="port",
        [AST_NET_DECL]="net_decl", [AST_REG_DECL]="reg_decl",
        [AST_PARAM_DECL]="param_decl",
        [AST_ASSIGN]="assign", [AST_ALWAYS]="always",
        [AST_ALWAYS_FF]="always_ff", [AST_ALWAYS_COMB]="always_comb",
        [AST_SENSITIVITY]="sensitivity",
        [AST_STMT_BLOCK]="block", [AST_STMT_IF]="if",
        [AST_STMT_CASE]="case", [AST_STMT_FOR]="for",
        [AST_STMT_WHILE]="while", [AST_STMT_NBA]="nba",
        [AST_STMT_BA]="ba",
        [AST_EXPR_BINARY]="binary", [AST_EXPR_UNARY]="unary",
        [AST_EXPR_TERNARY]="ternary", [AST_EXPR_IDENT]="ident",
        [AST_EXPR_NUMBER]="number", [AST_EXPR_STRING]="string",
        [AST_EXPR_CONCAT]="concat", [AST_EXPR_RANGE]="range",
        [AST_EXPR_BIT_SELECT]="bit_select", [AST_EXPR_FUNC_CALL]="func_call",
        [AST_ERROR]="error", [AST_ROOT]="root"
    };
    if (t < sizeof(names)/sizeof(names[0]) && names[t]) return names[t];
    return "unknown";
}

static int dot_node_id = 0;

static void ast_dump_dot_rec(AstNode *node, FILE *f) {
    if (!node || !f) return;
    int my_id = dot_node_id++;
    const char *label = node->name[0] ? node->name : ast_type_name(node->type);
    fprintf(f, "  n%d [label=\"%s\\n[%s]\"];\n", my_id, label, ast_type_name(node->type));
    for (int i = 0; i < node->child_count; i++) {
        if (node->children[i]) {
            int child_id = dot_node_id;
            ast_dump_dot_rec(node->children[i], f);
            fprintf(f, "  n%d -> n%d;\n", my_id, child_id);
        }
    }
}

void ast_dump_dot(AstNode *root, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return;
    dot_node_id = 0;
    fprintf(f, "digraph AST {\n");
    fprintf(f, "  node [shape=box, fontname=\"Courier\"];\n");
    ast_dump_dot_rec(root, f);
    fprintf(f, "}\n");
    fclose(f);
}

/* ================================================================
 * L3: Pretty-printer for debugging
 * ================================================================ */

void ast_print(AstNode *root, int indent) {
    if (!root) return;
    for (int i = 0; i < indent; i++) printf("  ");
    printf("[%s]", ast_type_name(root->type));
    if (root->name[0]) printf(" '%s'", root->name);
    if (root->type == AST_EXPR_NUMBER) printf(" value=%d", root->int_val);
    printf("\n");
    for (int i = 0; i < root->child_count; i++) {
        ast_print(root->children[i], indent + 1);
    }
}

/* ================================================================
 * L2: Free
 * ================================================================ */

void ast_free(AstNode *root) {
    /* With arena allocation, we can just free all arenas.
     * Individual nodes do not need separate deallocation. */
    (void)root;
    arena_free_all();
}
