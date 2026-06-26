#ifndef CLOCK_TREE_H
#define CLOCK_TREE_H

#include <stdbool.h>

#define MAX_CTS_NODES 128
#define MAX_CTS_SINKS 64
#define MAX_CTS_BUFFERS 32

typedef struct {
    int      id;
    double   x, y;
    double   delay;
    double   skew;
    int      parent;
    int      children[4];
    int      child_count;
    bool     is_sink;
    bool     is_buffer;
    bool     is_root;
} CtsNode;

typedef struct {
    double   x, y;
    double   cap_load;
    double   delay;
    int      node_id;
} CtsSink;

typedef struct {
    char     name[32];
    double   delay;
    double   drive_strength;
    double   input_cap;
} CtsBuffer;

typedef struct {
    CtsNode      nodes[MAX_CTS_NODES];
    int          node_count;
    CtsSink      sinks[MAX_CTS_SINKS];
    int          sink_count;
    CtsBuffer    buffers[MAX_CTS_BUFFERS];
    int          buffer_count;
    double       period;
    double       max_skew;
} ClockTree;

void cts_init(ClockTree *ct, double period);
int  cts_add_sink(ClockTree *ct, double x, double y, double load);
int  cts_add_buffer_type(ClockTree *ct, const char *name, double delay, double drive, double cap);
void cts_build_htree(ClockTree *ct, double cx, double cy, double size);
void cts_insert_buffers(ClockTree *ct);
void cts_skew_analysis(ClockTree *ct);
double cts_total_skew(ClockTree *ct);
double cts_global_skew(ClockTree *ct);
void cts_print(ClockTree *ct);
bool cts_meets_constraints(ClockTree *ct, double max_skew);

#endif
