#ifndef HLS_ALLOCATE_H
#define HLS_ALLOCATE_H
#include "hls_dfg.h"
#include <stdbool.h>
#define MAX_FU_TYPES 8

typedef enum { FU_ADDER, FU_MULTIPLIER, FU_DIVIDER, FU_LOAD_UNIT, FU_STORE_UNIT, FU_ALU } FuType;

typedef struct { FuType type; int count; int used; double area; double delay; } FuAlloc;

typedef struct { FuAlloc units[MAX_FU_TYPES]; int type_count; double total_area; } AllocResult;

bool alloc_greedy(DataFlowGraph *dfg, AllocResult *result);
int alloc_clique_partition(DataFlowGraph *dfg, AllocResult *result);
void alloc_lifetime_analysis(DataFlowGraph *dfg, int *first_use, int *last_use);
void alloc_print(AllocResult *r);
#endif
