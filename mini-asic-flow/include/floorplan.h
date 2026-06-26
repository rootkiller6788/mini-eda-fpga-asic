#ifndef FLOORPLAN_H
#define FLOORPLAN_H

#include "standard_cell.h"
#include <stdbool.h>

#define MAX_MODULES 64
#define MAX_BSTAR_NODES 128

typedef struct {
    char     name[32];
    double   x, y;
    double   width, height;
    double   area;
    bool     is_soft;
    bool     is_fixed;
    bool     is_rotated;
} FpModule;

typedef struct {
    int      left, right;
    bool     is_module;
    int      module_id;
    double   width, height;
} BStarNode;

typedef struct {
    FpModule    modules[MAX_MODULES];
    int         module_count;
    BStarNode   nodes[MAX_BSTAR_NODES];
    int         node_count;
    double      die_width, die_height;
    double      total_area;
    double      wire_estimate;
    double      cost;
} Floorplan;

void floorplan_init(Floorplan *fp, double w, double h);
int  floorplan_add_module(Floorplan *fp, const char *name, double w, double h, double area);
void floorplan_bstar(Floorplan *fp);
void floorplan_evaluate(Floorplan *fp);
void floorplan_anneal(Floorplan *fp, double init_temp, double cool_rate, int iters);
void floorplan_print(Floorplan *fp);
double floorplan_dead_space(Floorplan *fp);

#endif
