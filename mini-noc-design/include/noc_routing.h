#ifndef NOC_ROUTING_H
#define NOC_ROUTING_H
#include <stdbool.h>

typedef enum { ROUTE_XY, ROUTE_OE_FIXED, ROUTE_WEST_FIRST, ROUTE_NORTH_LAST, ROUTE_ADAPTIVE } RoutingAlgo;

typedef struct {
    RoutingAlgo algo;
    int src_x, src_y, dst_x, dst_y;
    int cur_x, cur_y;
    int hops_remaining;
    bool congested;
} RoutingState;

void   routing_init(RoutingState *rs, RoutingAlgo algo, int sx, int sy, int dx, int dy);
int    routing_xy(RoutingState *rs);            /* returns next port direction: 0=N 1=S 2=E 3=W 4=L */
int    routing_odd_even(RoutingState *rs);       /* OE turn model: no N→E or N→W turns on even cols */
int    routing_west_first(RoutingState *rs);
int    routing_north_last(RoutingState *rs);
int    routing_adaptive(RoutingState *rs, bool *congestion_map);
bool   routing_deadlock_free(RoutingAlgo algo);
const char *routing_algo_name(RoutingAlgo algo);
#endif
