#ifndef PLACE_ROUTE_H
#define PLACE_ROUTE_H

#include <stdbool.h>

#define MAX_BLOCKS      64
#define MAX_NETS        128
#define MAX_PINS        256
#define MAX_GRID_X      64
#define MAX_GRID_Y      64

typedef struct {
    char     name[32];
    double   x, y;
    double   width, height;
    bool     is_fixed;
} Block;

typedef struct {
    char     name[32];
    int      pins[MAX_PINS];
    int      pin_count;
    double   weight;
} Net;

typedef struct {
    Block    blocks[MAX_BLOCKS];
    int      block_count;
    Net      nets[MAX_NETS];
    int      net_count;
    double   chip_width;
    double   chip_height;
} Placement;

typedef struct {
    int      grid[MAX_GRID_Y][MAX_GRID_X];
    int      width, height;
} RouteGrid;

void place_init(Placement *p, double w, double h);
int  place_add_block(Placement *p, const char *name, double w, double h);
int  place_add_net(Placement *p, const char *name, double weight);
void place_add_pin(Placement *p, int net_id, int block_id);
double place_hpwl(Placement *p);
double place_total_cost(Placement *p);
void place_simulated_annealing(Placement *p, double init_temp, double cooling_rate, int iterations);
void place_print(Placement *p);

void route_init(RouteGrid *g, int w, int h);
bool route_maze(RouteGrid *g, int sx, int sy, int ex, int ey, int net_id);
void route_global(RouteGrid *g, Placement *p);
int  route_total_wirelength(RouteGrid *g);
void route_print(RouteGrid *g);
bool route_is_legal(RouteGrid *g, Placement *p);

#endif
