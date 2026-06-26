#ifndef HLS_BINDING_H
#define HLS_BINDING_H
#include "hls_dfg.h"
#include <stdbool.h>
#define MAX_REGISTERS 32
#define MAX_FU_INSTANCES 16

typedef struct { int id; int bound_node; int from_cycle; int to_cycle; bool shared; } RegisterBind;
typedef struct { int id; int bound_node; int op; int cycle; } FuBind;

typedef struct { RegisterBind regs[MAX_REGISTERS]; int reg_count; FuBind fus[MAX_FU_INSTANCES]; int fu_count; } BindingResult;

bool bind_register_lifetime(DataFlowGraph *dfg, BindingResult *result);
bool bind_functional_unit(DataFlowGraph *dfg, BindingResult *result);
void bind_print(BindingResult *r);
int bind_register_count(BindingResult *r);
#endif
