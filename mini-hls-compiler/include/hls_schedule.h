#ifndef HLS_SCHEDULE_H
#define HLS_SCHEDULE_H
#include "hls_dfg.h"
#include <stdbool.h>

typedef struct { int latency; int num_resources; int *resource_usage; } ScheduleResult;

bool sched_asap(DataFlowGraph *dfg);
bool sched_alap(DataFlowGraph *dfg, int max_latency);
bool sched_list(DataFlowGraph *dfg, int max_resources);
bool sched_force_directed(DataFlowGraph *dfg, int max_latency);
void sched_print(DataFlowGraph *dfg, const char *title);
int sched_total_latency(DataFlowGraph *dfg);
#endif
