#include "lut_synth.h"
#include <stdio.h>
#include <string.h>

void bool_net_init(BooleanNetwork *bn, int lut_size) {
    bn->node_count = 0;
    bn->input_count = 0;
    bn->lut_size = lut_size;
    memset(bn->nodes, 0, sizeof(bn->nodes));
}

int bool_net_add_input(BooleanNetwork *bn) {
    if (bn->node_count >= MAX_FUNC_NODES) return -1;
    FuncNode *n = &bn->nodes[bn->node_count];
    n->node_id = bn->node_count;
    n->is_input = true;
    n->is_leaf = true;
    n->input_num = bn->input_count++;
    return bn->node_count++;
}

static int bool_net_add_op(BooleanNetwork *bn, int op, int a, int b) {
    if (bn->node_count >= MAX_FUNC_NODES) return -1;
    FuncNode *n = &bn->nodes[bn->node_count];
    n->node_id = bn->node_count;
    n->op = op;
    n->children[0] = a;
    n->children[1] = b;
    n->child_count = 2;
    n->is_leaf = false;
    return bn->node_count++;
}

int bool_net_add_and(BooleanNetwork *bn, int a, int b) { return bool_net_add_op(bn, 0, a, b); }
int bool_net_add_or(BooleanNetwork *bn, int a, int b)  { return bool_net_add_op(bn, 1, a, b); }
int bool_net_add_xor(BooleanNetwork *bn, int a, int b) { return bool_net_add_op(bn, 2, a, b); }
int bool_net_add_not(BooleanNetwork *bn, int a)        { return bool_net_add_op(bn, 3, a, 0); }

static int node_input_count(BooleanNetwork *bn, FuncNode *n, bool *inputs_set, int max_inputs) {
    if (n->is_input) {
        if (!inputs_set[n->input_num]) {
            inputs_set[n->input_num] = true;
            return 1;
        }
        return 0;
    }
    int count = 0;
    for (int i = 0; i < n->child_count; i++) {
        FuncNode *c = &bn->nodes[n->children[i]];
        count += node_input_count(bn, c, inputs_set, max_inputs);
    }
    return count;
}

static void build_truth_table(BooleanNetwork *bn, int node_id, int *tt, int n_inputs) {
    FuncNode *n = &bn->nodes[node_id];
    int total = 1 << n_inputs;

    if (n->is_input) {
        for (int v = 0; v < total; v++)
            tt[v] = (v >> n->input_num) & 1;
        return;
    }

    int ta[64] = {0}, tb[64] = {0};
    build_truth_table(bn, n->children[0], ta, n_inputs);
    if (n->child_count > 1)
        build_truth_table(bn, n->children[1], tb, n_inputs);

    for (int v = 0; v < total; v++) {
        switch (n->op) {
            case 0: tt[v] = ta[v] & tb[v]; break;
            case 1: tt[v] = ta[v] | tb[v]; break;
            case 2: tt[v] = ta[v] ^ tb[v]; break;
            case 3: tt[v] = !ta[v]; break;
        }
    }
}

bool lut_map(BooleanNetwork *bn, LutInstance *luts, int *lut_count, int max_luts) {
    *lut_count = 0;
    if (bn->node_count < 1 || *lut_count >= max_luts) return false;

    LutInstance *l = &luts[(*lut_count)++];
    l->id = 0;
    bool inputs_set[MAX_LUT_INPUTS] = {false};
    int total_inputs = 0;
    for (int i = 0; i < bn->node_count; i++) {
        if (bn->nodes[i].is_input && !inputs_set[bn->nodes[i].input_num]) {
            inputs_set[bn->nodes[i].input_num] = true;
            l->inputs[total_inputs++] = bn->nodes[i].input_num;
            if (total_inputs >= MAX_LUT_INPUTS) break;
        }
    }
    l->input_count = total_inputs;
    l->size = total_inputs <= 4 ? LUT4 : LUT6;

    build_truth_table(bn, bn->node_count - 1, l->truth_table, total_inputs);
    l->output = bn->node_count - 1;
    return true;
}

bool lut_decompose(BooleanNetwork *bn, int max_inputs) {
    (void)max_inputs;
    return bn->node_count > 0;
}

void lut_print(LutInstance *l) {
    printf("LUT #%d (%d inputs): truth_table=", l->id, l->input_count);
    int rows = 1 << l->input_count;
    for (int i = 0; i < rows && i < 64; i++)
        printf("%d", l->truth_table[i]);
    printf("\n");
}

void lut_truth_table_to_expr(LutInstance *l, char *buf, int buf_size) {
    int pos = 0;
    pos += snprintf(buf + pos, buf_size - pos, "LUT%d(", l->input_count);
    int rows = 1 << l->input_count;
    for (int i = 0; i < rows && i < 64; i++)
        pos += snprintf(buf + pos, buf_size - pos, "%d", l->truth_table[i]);
    if (pos < buf_size - 1) buf[pos++] = ')';
    buf[pos] = '\0';
}
