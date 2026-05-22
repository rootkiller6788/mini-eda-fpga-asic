#ifndef STATIC_TIMING_H
#define STATIC_TIMING_H

#include <stdbool.h>

#define MAX_TIMING_NODES    256
#define MAX_TIMING_EDGES    512
#define MAX_TIMING_PATHS    64
#define MAX_CLOCK_DOMAINS   8

typedef enum {
    STA_RISE, STA_FALL, STA_BOTH
} StaTransition;

typedef struct {
    int      id;
    char     name[64];
    double   arrival_time;
    double   required_time;
    double   slack;
    double   delay;
    bool     is_register;
    bool     is_clock;
    bool     is_input;
    bool     is_output;
    double   cap;
    double   slew;
} StaNode;

typedef struct {
    int      from;
    int      to;
    double   delay;
    double   setup_time;
    double   hold_time;
    bool     is_clock_edge;
} StaEdge;

typedef struct {
    StaNode  nodes[MAX_TIMING_NODES];
    int      node_count;
    StaEdge  edges[MAX_TIMING_EDGES];
    int      edge_count;
    double   clock_period;
    double   clock_uncertainty;
    char     clock_name[32];
} StaGraph;

typedef struct {
    int      path[MAX_TIMING_NODES];
    int      path_len;
    double   total_delay;
    double   slack;
    bool     is_critical;
} TimingPath;

void sta_init(StaGraph *g, double clock_period, const char *clock_name);
int  sta_add_node(StaGraph *g, const char *name, double delay, bool is_reg, bool is_clk);
int  sta_add_edge(StaGraph *g, int from, int to, double delay, double setup, double hold);
void sta_compute_arrival(StaGraph *g);
void sta_compute_required(StaGraph *g);
void sta_compute_slack(StaGraph *g);
int  sta_find_critical_paths(StaGraph *g, TimingPath *paths, int max_paths);
void sta_report(StaGraph *g);
void sta_report_path(TimingPath *p, StaGraph *g);
double sta_worst_slack(StaGraph *g);
double sta_total_negative_slack(StaGraph *g);
bool sta_meets_timing(StaGraph *g);

#endif
