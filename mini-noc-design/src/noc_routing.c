#include "noc_routing.h"
#include <stdio.h>
#include <string.h>

void routing_init(RoutingState *rs, RoutingAlgo algo, int sx, int sy, int dx, int dy) {
    rs->algo = algo; rs->src_x = sx; rs->src_y = sy; rs->dst_x = dx; rs->dst_y = dy;
    rs->cur_x = sx; rs->cur_y = sy; rs->hops_remaining = 0; rs->congested = false;
}

const char *routing_algo_name(RoutingAlgo algo) {
    switch (algo) { case ROUTE_XY: return "XY"; case ROUTE_OE_FIXED: return "Odd-Even"; case ROUTE_WEST_FIRST: return "West-First"; case ROUTE_NORTH_LAST: return "North-Last"; case ROUTE_ADAPTIVE: return "Adaptive"; default: return "?"; }
}

bool routing_deadlock_free(RoutingAlgo algo) {
    switch (algo) { case ROUTE_XY: case ROUTE_OE_FIXED: case ROUTE_WEST_FIRST: case ROUTE_NORTH_LAST: return true; default: return false; }
}

int routing_xy(RoutingState *rs) {
    /* Returns: 0=N, 1=S, 2=E, 3=W, 4=Local */
    if (rs->cur_x == rs->dst_x && rs->cur_y == rs->dst_y) return 4;
    if (rs->dst_x > rs->cur_x) { rs->cur_x++; return 2; }
    if (rs->dst_x < rs->cur_x) { rs->cur_x--; return 3; }
    if (rs->dst_y > rs->cur_y) { rs->cur_y++; return 1; }
    if (rs->dst_y < rs->cur_y) { rs->cur_y--; return 0; }
    return 4;
}

int routing_odd_even(RoutingState *rs) {
    if (rs->cur_x == rs->dst_x && rs->cur_y == rs->dst_y) return 4;
    int x = rs->cur_x, y = rs->cur_y;
    /* OE rule: no N→E or N→W turn on even columns */
    int dx = rs->dst_x - x, dy = rs->dst_y - y;
    if (dx > 0) { rs->cur_x++; return 2; }
    if (dx < 0) { rs->cur_x--; return 3; }
    if (dy > 0) { rs->cur_y++; return 1; }
    if (dy < 0) { rs->cur_y--; return 0; }
    return 4;
}

int routing_west_first(RoutingState *rs) {
    if (rs->cur_x == rs->dst_x && rs->cur_y == rs->dst_y) return 4;
    int dx = rs->dst_x - rs->cur_x, dy_dir = rs->dst_y - rs->cur_y;
    if (dx < 0) { rs->cur_x--; return 3; }
    if (dy_dir > 0) { rs->cur_y++; return 1; }
    if (dy_dir < 0) { rs->cur_y--; return 0; }
    if (dx > 0) { rs->cur_x++; return 2; }
    return 4;
}

int routing_north_last(RoutingState *rs) {
    if (rs->cur_x == rs->dst_x && rs->cur_y == rs->dst_y) return 4;
    int dx = rs->dst_x - rs->cur_x, dy = rs->dst_y - rs->cur_y;
    if (dy > 0) { rs->cur_y++; return 1; }
    if (dx > 0) { rs->cur_x++; return 2; }
    if (dx < 0) { rs->cur_x--; return 3; }
    if (dy < 0) { rs->cur_y--; return 0; }
    return 4;
}

int routing_adaptive(RoutingState *rs, bool *congestion_map) {
    if (rs->cur_x == rs->dst_x && rs->cur_y == rs->dst_y) return 4;
    int dx = rs->dst_x - rs->cur_x, dy = rs->dst_y - rs->cur_y;
    /* Try productive directions, avoiding congested ones */
    int options[4], oc = 0;
    if (dx > 0 && (!congestion_map || !congestion_map[2])) options[oc++] = 2;
    if (dx < 0 && (!congestion_map || !congestion_map[3])) options[oc++] = 3;
    if (dy > 0 && (!congestion_map || !congestion_map[1])) options[oc++] = 1;
    if (dy < 0 && (!congestion_map || !congestion_map[0])) options[oc++] = 0;
    if (oc == 0) { /* all congested, use XY fallback */
        if (dx > 0) { rs->cur_x++; return 2; }
        if (dx < 0) { rs->cur_x--; return 3; }
        if (dy > 0) { rs->cur_y++; return 1; }
        if (dy < 0) { rs->cur_y--; return 0; }
    }
    int pick = options[0];
    if (pick == 2) rs->cur_x++; else if (pick == 3) rs->cur_x--;
    else if (pick == 1) rs->cur_y++; else if (pick == 0) rs->cur_y--;
    return pick;
}
