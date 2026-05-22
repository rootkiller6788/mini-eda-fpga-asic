#ifndef LUT_SYNTH_H
#define LUT_SYNTH_H

#include "fpga_arch.h"
#include <stdbool.h>

#define MAX_LUT_INPUTS 6
#define MAX_FUNC_NODES 128

typedef struct {
    int      id;
    int      inputs[MAX_LUT_INPUTS];
    int      input_count;
    int      truth_table[64];
    int      output;
    LutSize  size;
} LutInstance;

typedef struct {
    int      node_id;
    int      children[2];
    int      child_count;
    int      op;  /* 0=AND, 1=OR, 2=XOR, 3=NOT */
    bool     is_leaf;
    bool     is_input;
    int      input_num;
} FuncNode;

typedef struct {
    FuncNode nodes[MAX_FUNC_NODES];
    int      node_count;
    int      input_count;
    int      lut_size;
} BooleanNetwork;

void bool_net_init(BooleanNetwork *bn, int lut_size);
int  bool_net_add_input(BooleanNetwork *bn);
int  bool_net_add_and(BooleanNetwork *bn, int a, int b);
int  bool_net_add_or(BooleanNetwork *bn, int a, int b);
int  bool_net_add_xor(BooleanNetwork *bn, int a, int b);
int  bool_net_add_not(BooleanNetwork *bn, int a);
bool lut_map(BooleanNetwork *bn, LutInstance *luts, int *lut_count, int max_luts);
bool lut_decompose(BooleanNetwork *bn, int max_inputs);
void lut_print(LutInstance *l);
void lut_truth_table_to_expr(LutInstance *l, char *buf, int buf_size);

#endif
