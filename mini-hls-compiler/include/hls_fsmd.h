#ifndef HLS_FSMD_H
#define HLS_FSMD_H
#include "hls_dfg.h"
#include <stdbool.h>
#define MAX_STATES 64
#define MAX_CTL_TRANSITIONS 128

typedef struct { int id; char name[32]; int operations[MAX_DFG_NODES]; int op_count; int next_state; int alt_state; bool branch; } FsmdState;

typedef struct { FsmdState states[MAX_STATES]; int state_count; int start_state; DataFlowGraph *dfg; } FsmdController;

void fsmd_init(FsmdController *c, DataFlowGraph *dfg);
int fsmd_create_state(FsmdController *c, const char *name);
bool fsmd_generate(FsmdController *c);
void fsmd_emit_verilog(FsmdController *c);
void fsmd_print(FsmdController *c);
#endif
