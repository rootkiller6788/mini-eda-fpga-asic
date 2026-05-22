#ifndef HLS_DFG_H
#define HLS_DFG_H
#include <stdbool.h>
#define MAX_DFG_NODES 128
#define MAX_DFG_EDGES 256

typedef enum { HLS_ADD, HLS_SUB, HLS_MUL, HLS_DIV, HLS_LOAD, HLS_STORE, HLS_PHI, HLS_CMP } HlsOp;

typedef struct { int id; HlsOp op; char name[32]; int latency; double area; int asap, alap; int schedule_cycle; int inputs[4]; int input_count; bool scheduled; } DfgNode;

typedef struct { int from, to; int delay; bool is_data; } DfgEdge;

typedef struct { DfgNode nodes[MAX_DFG_NODES]; int node_count; DfgEdge edges[MAX_DFG_EDGES]; int edge_count; } DataFlowGraph;

void dfg_init(DataFlowGraph *dfg);
int dfg_add_node(DataFlowGraph *dfg, HlsOp op, const char *name, double area);
int dfg_add_edge(DataFlowGraph *dfg, int from, int to, int delay);
int dfg_build_from_c(DataFlowGraph *dfg, const char *c_source);
void dfg_print(DataFlowGraph *dfg);
int dfg_critical_path(DataFlowGraph *dfg);
#endif
