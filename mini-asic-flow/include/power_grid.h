#ifndef POWER_GRID_H
#define POWER_GRID_H

#include <stdbool.h>

#define MAX_PG_NODES 256
#define MAX_PG_STRAPS 128

typedef struct {
    int      id;
    double   x, y;
    double   voltage;
    double   current;
    bool     is_source;
    bool     is_connected;
} PgNode;

typedef struct {
    int      id;
    int      from;
    int      to;
    double   resistance;
    double   current;
    double   voltage_drop;
    int      metal_layer;
} PgStrap;

typedef struct {
    PgNode   nodes[MAX_PG_NODES];
    int      node_count;
    PgStrap  straps[MAX_PG_STRAPS];
    int      strap_count;
    double   vdd;
    double   vss;
    double   max_ir_drop;
} PowerGrid;

void pgrid_init(PowerGrid *pg, double vdd);
int  pgrid_add_node(PowerGrid *pg, double x, double y, double current, bool is_src);
int  pgrid_add_strap(PowerGrid *pg, int from, int to, double r, int layer);
void pgrid_analyze_ir(PowerGrid *pg);
void pgrid_report(PowerGrid *pg);
bool pgrid_meets_spec(PowerGrid *pg);
double pgrid_max_ir_drop(PowerGrid *pg);

#endif
