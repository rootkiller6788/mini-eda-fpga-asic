/**
 * noc_routing.c ? NoC Routing Algorithm Implementations
 *
 * Implements deterministic, turn-model, and adaptive routing
 * for 2D mesh/torus networks.
 */

#include "noc_routing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ??? L1: Static turn model tables ???????????????????????????????????? */

static const noc_turn_table_t g_turn_tables[ROUTE_COUNT] = {
    /* {algo, {ES,EN,WS,WN,SE,SW,NE,NW,180}, name} */
    {ROUTE_XY,           {1,1,1,1,1,1,1,1,0}, "XY"},
    {ROUTE_YX,           {1,1,1,1,1,1,1,1,0}, "YX"},
    {ROUTE_WEST_FIRST,   {1,1,1,1,1,0,1,0,0}, "West-First"},
    {ROUTE_NORTH_LAST,   {1,1,1,1,1,1,0,0,0}, "North-Last"},
    {ROUTE_NEGATIVE_FIRST,{1,0,1,1,1,0,1,1,0}, "Negative-First"},
    {ROUTE_ODD_EVEN,     {1,1,1,1,1,1,1,1,0}, "Odd-Even"},
    {ROUTE_ADAPTIVE,     {1,1,1,1,1,1,1,1,0}, "Adaptive"},
    {ROUTE_RANDOMIZED,   {1,1,1,1,1,1,1,1,0}, "Randomized"},
};

const noc_turn_table_t *noc_turn_model_get(noc_route_algo_t algo) {
    if (algo < 0 || algo >= ROUTE_COUNT) return NULL;
    return &g_turn_tables[algo];
}

int noc_turn_prohibited(noc_route_algo_t algo, noc_turn_t turn) {
    const noc_turn_table_t *tbl = noc_turn_model_get(algo);
    if (!tbl || turn < 0 || turn >= TURN_COUNT) return 0;
    return (tbl->allowed[turn] == 0) ? 1 : 0;
}

/* ??? L4: Turn model deadlock-freedom check ??????????????????????????? */

int noc_turn_model_is_deadlock_free(const noc_turn_table_t *table,
                                     int32_t mesh_size) {
    (void)mesh_size;
    if (!table) return 0;

    /* Build CDG for the turn model and check for cycles */
    noc_channel_dep_graph_t cdg = noc_cdg_build(table->algo, mesh_size);
    return (noc_cdg_has_cycle(&cdg) == 0) ? 1 : 0;
}

/* ??? XY Routing ?????????????????????????????????????????????????????? */

noc_direction_t noc_route_xy(int32_t src_x, int32_t src_y,
                              int32_t dst_x, int32_t dst_y,
                              int32_t cur_x, int32_t cur_y) {
    (void)src_x; (void)src_y;

    if (cur_x == dst_x && cur_y == dst_y) return NOC_DIR_LOCAL;

    /* Route in X first, then Y */
    if (cur_x < dst_x) return NOC_DIR_EAST;
    if (cur_x > dst_x) return NOC_DIR_WEST;
    if (cur_y < dst_y) return NOC_DIR_SOUTH;
    if (cur_y > dst_y) return NOC_DIR_NORTH;

    return NOC_DIR_LOCAL;
}

/* ??? YX Routing ?????????????????????????????????????????????????????? */

noc_direction_t noc_route_yx(int32_t src_x, int32_t src_y,
                              int32_t dst_x, int32_t dst_y,
                              int32_t cur_x, int32_t cur_y) {
    (void)src_x; (void)src_y;

    if (cur_x == dst_x && cur_y == dst_y) return NOC_DIR_LOCAL;

    if (cur_y < dst_y) return NOC_DIR_SOUTH;
    if (cur_y > dst_y) return NOC_DIR_NORTH;
    if (cur_x < dst_x) return NOC_DIR_EAST;
    if (cur_x > dst_x) return NOC_DIR_WEST;

    return NOC_DIR_LOCAL;
}

/* ??? West-First Routing (Turn Model) ??????????????????????????????????
 *
 * Glass & Ni (1992): Prohibit turns TO the west direction.
 * This breaks all cycles: packets can only go west at the beginning.
 *
 * Algorithm:
 *   1. If dst is to the west, route west first.
 *   2. Otherwise, route east or vertical as needed.
 */

noc_direction_t noc_route_west_first(int32_t src_x, int32_t src_y,
                                      int32_t dst_x, int32_t dst_y,
                                      int32_t cur_x, int32_t cur_y) {
    (void)src_x; (void)src_y;

    if (cur_x == dst_x && cur_y == dst_y) return NOC_DIR_LOCAL;

    /* If need to go west and not yet at target X, go west */
    if (dst_x < cur_x) return NOC_DIR_WEST;

    /* Once west is done (or not needed), do east or vertical */
    if (dst_x > cur_x) return NOC_DIR_EAST;

    /* Vertical routing */
    if (cur_y < dst_y) return NOC_DIR_SOUTH;
    if (cur_y > dst_y) return NOC_DIR_NORTH;

    return NOC_DIR_LOCAL;
}

/* ??? North-Last Routing ???????????????????????????????????????????????
 *
 * Glass & Ni (1992): Prohibit turns FROM the north direction.
 * North is always the last direction traveled.
 *
 * Algorithm:
 *   1. Complete all east/west/south routing before going north.
 *   2. Go north only when X matches and south is done.
 */

noc_direction_t noc_route_north_last(int32_t src_x, int32_t src_y,
                                      int32_t dst_x, int32_t dst_y,
                                      int32_t cur_x, int32_t cur_y) {
    (void)src_x; (void)src_y;

    if (cur_x == dst_x && cur_y == dst_y) return NOC_DIR_LOCAL;

    /* Avoid going north until all other directions are done */
    if (cur_y > dst_y) {
        /* Need to go north, but do east/west/south first */
        if (cur_x < dst_x) return NOC_DIR_EAST;
        if (cur_x > dst_x) return NOC_DIR_WEST;
        if (cur_y < dst_y) return NOC_DIR_SOUTH;
        return NOC_DIR_NORTH;
    }

    /* Not going north: regular routing */
    if (cur_x < dst_x) return NOC_DIR_EAST;
    if (cur_x > dst_x) return NOC_DIR_WEST;
    if (cur_y < dst_y) return NOC_DIR_SOUTH;

    return NOC_DIR_LOCAL;
}

/* ??? Negative-First Routing ??????????????????????????????????????????
 *
 * Complete all negative-direction movements (west and north in our
 * coordinate system where south=+y) before any positive movements.
 *   Negative: West (x-) and North (y-)
 *   Positive: East (x+) and South (y+)
 */

noc_direction_t noc_route_negative_first(int32_t src_x, int32_t src_y,
                                          int32_t dst_x, int32_t dst_y,
                                          int32_t cur_x, int32_t cur_y) {
    (void)src_x; (void)src_y;

    if (cur_x == dst_x && cur_y == dst_y) return NOC_DIR_LOCAL;

    /* Phase 1: Negative direction (west, north) */
    if (dst_x < cur_x) return NOC_DIR_WEST;
    if (dst_y < cur_y) return NOC_DIR_NORTH;

    /* Phase 2: Positive direction (east, south) */
    if (dst_x > cur_x) return NOC_DIR_EAST;
    if (dst_y > cur_y) return NOC_DIR_SOUTH;

    return NOC_DIR_LOCAL;
}

/* ??? Odd-Even Routing ????????????????????????????????????????????????
 *
 * Chiu (IEEE TPDS 2000): Restricted turns based on column parity.
 *
 * Rules:
 *   - ES turn on even columns (x mod 2 == 0): prohibited
 *     Exception: destination is at current y+1, x+1
 *   - EN turn on even columns: prohibited
 *     Exception: destination is at current y-1, x+1
 *   - NW turn on odd columns (x mod 2 == 1): prohibited
 *   - SW turn on odd columns: prohibited
 *
 * These restrictions guarantee deadlock freedom while providing
 * more adaptivity than West-First or North-Last.
 */

noc_direction_t noc_route_odd_even(int32_t src_x, int32_t src_y,
                                    int32_t dst_x, int32_t dst_y,
                                    int32_t cur_x, int32_t cur_y) {
    (void)src_x; (void)src_y;

    if (cur_x == dst_x && cur_y == dst_y) return NOC_DIR_LOCAL;

    int32_t even_col = (cur_x % 2 == 0) ? 1 : 0;

    /* Determine which directions are needed */
    int need_east  = (dst_x > cur_x) ? 1 : 0;
    int need_west  = (dst_x < cur_x) ? 1 : 0;
    int need_south = (dst_y > cur_y) ? 1 : 0;
    int need_north = (dst_y < cur_y) ? 1 : 0;

    if (even_col) {
        /* Even column: restrict ES and EN */
        /* Can still go east if needed */
        if (need_east) return NOC_DIR_EAST;
        /* ES: east?south prohibited on even col unless special case */
        if (need_south && !need_west) {
            /* Check exception: dst at (cur_x+1, cur_y+1) */
            if (dst_x == cur_x + 1 && dst_y == cur_y + 1) {
                return NOC_DIR_EAST; /* go east first, then south */
            }
        }
        /* EN: east?north prohibited on even col */
        if (need_north && !need_west) {
            if (dst_x == cur_x + 1 && dst_y == cur_y - 1) {
                return NOC_DIR_EAST;
            }
        }
        /* Default: go east/south/north, avoid restricted turns */
        if (need_west)  return NOC_DIR_WEST;
        if (need_south) return NOC_DIR_SOUTH;
        if (need_north) return NOC_DIR_NORTH;
        if (need_east)  return NOC_DIR_EAST;
    } else {
        /* Odd column: restrict NW and SW */
        if (need_east)  return NOC_DIR_EAST;
        if (need_west)  return NOC_DIR_WEST;
        if (need_south && !need_east) {
            if (dst_x == cur_x - 1 && dst_y == cur_y + 1) {
                return NOC_DIR_WEST;
            }
        }
        if (need_north && !need_east) {
            if (dst_x == cur_x - 1 && dst_y == cur_y - 1) {
                return NOC_DIR_WEST;
            }
        }
        if (need_south) return NOC_DIR_SOUTH;
        if (need_north) return NOC_DIR_NORTH;
    }

    return NOC_DIR_LOCAL;
}

/* ??? Minimal Adaptive Routing ????????????????????????????????????????
 *
 * Duato (1993): Choose among all minimal paths.
 * On a 2D mesh, a minimal path moves only in directions that
 * reduce the Manhattan distance to the destination.
 * When both X and Y are needed, choose the less congested direction.
 */

noc_direction_t noc_route_adaptive(int32_t src_x, int32_t src_y,
                                    int32_t dst_x, int32_t dst_y,
                                    int32_t cur_x, int32_t cur_y,
                                    const int32_t *channel_load) {
    (void)src_x; (void)src_y;

    if (cur_x == dst_x && cur_y == dst_y) return NOC_DIR_LOCAL;

    int need_east  = (dst_x > cur_x) ? 1 : 0;
    int need_west  = (dst_x < cur_x) ? 1 : 0;
    int need_south = (dst_y > cur_y) ? 1 : 0;
    int need_north = (dst_y < cur_y) ? 1 : 0;

    /* If only one dimension needed, take it */
    if (need_east && !need_west && !need_south && !need_north) return NOC_DIR_EAST;
    if (need_west && !need_east && !need_south && !need_north) return NOC_DIR_WEST;
    if (need_south && !need_east && !need_west && !need_north) return NOC_DIR_SOUTH;
    if (need_north && !need_east && !need_west && !need_south) return NOC_DIR_NORTH;

    /* Both dimensions needed: choose less congested */
    int32_t load_east  = channel_load ? channel_load[NOC_DIR_EAST]  : 0;
    int32_t load_west  = channel_load ? channel_load[NOC_DIR_WEST]  : 0;
    int32_t load_south = channel_load ? channel_load[NOC_DIR_SOUTH] : 0;
    int32_t load_north = channel_load ? channel_load[NOC_DIR_NORTH] : 0;

    int32_t min_x = INT32_MAX, min_y = INT32_MAX;
    noc_direction_t best_x = NOC_DIR_LOCAL, best_y = NOC_DIR_LOCAL;

    if (need_east  && load_east  < min_x) { min_x = load_east;  best_x = NOC_DIR_EAST;  }
    if (need_west  && load_west  < min_x) { min_x = load_west;  best_x = NOC_DIR_WEST;  }
    if (need_south && load_south < min_y) { min_y = load_south; best_y = NOC_DIR_SOUTH; }
    if (need_north && load_north < min_y) { min_y = load_north; best_y = NOC_DIR_NORTH; }

    /* Pick the direction with lower load, tie-breaking to X */
    if (best_x != NOC_DIR_LOCAL && min_x <= min_y) return best_x;
    if (best_y != NOC_DIR_LOCAL) return best_y;

    return NOC_DIR_LOCAL;
}

/* ??? Randomized Routing ??????????????????????????????????????????????
 *
 * When both X and Y directions are needed, randomly choose
 * which dimension to route first. Used for load balancing.
 */

noc_direction_t noc_route_randomized(int32_t src_x, int32_t src_y,
                                      int32_t dst_x, int32_t dst_y,
                                      int32_t cur_x, int32_t cur_y,
                                      uint32_t seed) {
    (void)src_x; (void)src_y;

    if (cur_x == dst_x && cur_y == dst_y) return NOC_DIR_LOCAL;

    /* Simple LCG: X_{n+1} = (a * X_n + c) mod m */
    uint32_t rand = (1103515245u * seed + 12345u) & 0x7FFFFFFFu;

    int need_x = (cur_x != dst_x) ? 1 : 0;
    int need_y = (cur_y != dst_y) ? 1 : 0;

    if (need_x && need_y) {
        if (rand & 1) {
            /* X first */
            if (dst_x > cur_x) return NOC_DIR_EAST;
            return NOC_DIR_WEST;
        } else {
            /* Y first */
            if (dst_y > cur_y) return NOC_DIR_SOUTH;
            return NOC_DIR_NORTH;
        }
    }

    if (need_x) {
        if (dst_x > cur_x) return NOC_DIR_EAST;
        return NOC_DIR_WEST;
    }
    if (need_y) {
        if (dst_y > cur_y) return NOC_DIR_SOUTH;
        return NOC_DIR_NORTH;
    }

    return NOC_DIR_LOCAL;
}

/* ??? Routing dispatch ???????????????????????????????????????????????? */

noc_direction_t noc_routing_dispatch(noc_route_algo_t algo,
                                      int32_t src_x, int32_t src_y,
                                      int32_t dst_x, int32_t dst_y,
                                      int32_t cur_x, int32_t cur_y) {
    switch (algo) {
    case ROUTE_XY:
        return noc_route_xy(src_x, src_y, dst_x, dst_y, cur_x, cur_y);
    case ROUTE_YX:
        return noc_route_yx(src_x, src_y, dst_x, dst_y, cur_x, cur_y);
    case ROUTE_WEST_FIRST:
        return noc_route_west_first(src_x, src_y, dst_x, dst_y, cur_x, cur_y);
    case ROUTE_NORTH_LAST:
        return noc_route_north_last(src_x, src_y, dst_x, dst_y, cur_x, cur_y);
    case ROUTE_NEGATIVE_FIRST:
        return noc_route_negative_first(src_x, src_y, dst_x, dst_y, cur_x, cur_y);
    case ROUTE_ODD_EVEN:
        return noc_route_odd_even(src_x, src_y, dst_x, dst_y, cur_x, cur_y);
    case ROUTE_ADAPTIVE:
        return noc_route_adaptive(src_x, src_y, dst_x, dst_y, cur_x, cur_y, NULL);
    case ROUTE_RANDOMIZED:
        return noc_route_randomized(src_x, src_y, dst_x, dst_y, cur_x, cur_y, 42);
    default:
        return NOC_DIR_LOCAL;
    }
}

/* ??? Routing table construction ?????????????????????????????????????? */

noc_routing_table_t noc_build_routing_table(int32_t router_x, int32_t router_y,
                                             int32_t mesh_size,
                                             noc_route_algo_t algo) {
    noc_routing_table_t table;
    memset(&table, 0, sizeof(table));
    table.router_id = router_y * mesh_size + router_x;

    int32_t dx, dy;
    for (dy = 0; dy < mesh_size; dy++) {
        for (dx = 0; dx < mesh_size; dx++) {
            if (dx == router_x && dy == router_y) continue;
            if (table.num_entries >= NOC_ROUTE_TABLE_MAX_ENTRIES) break;

            noc_direction_t dir = noc_routing_dispatch(algo,
                router_x, router_y, dx, dy, router_x, router_y);

            table.entries[table.num_entries].dst_node = dy * mesh_size + dx;
            table.entries[table.num_entries].output_port = dir;
            table.entries[table.num_entries].vc = 0;
            table.entries[table.num_entries].cost =
                ((dx > router_x) ? (dx - router_x) : (router_x - dx)) +
                ((dy > router_y) ? (dy - router_y) : (router_y - dy));
            table.num_entries++;
        }
    }

    return table;
}

/* ??? CDG Construction ????????????????????????????????????????????????
 *
 * Build Channel Dependency Graph for a given routing algorithm.
 * A directed edge A?B means: using channel B may depend on
 * previously using channel A.
 *
 * For the turn model, channels are directional links (port@node).
 * A dependency exists if the routing algorithm makes a turn from
 * one channel to another.
 */

noc_channel_dep_graph_t noc_cdg_build(noc_route_algo_t algo,
                                       int32_t mesh_size) {
    noc_channel_dep_graph_t cdg;
    memset(&cdg, 0, sizeof(cdg));

    if (mesh_size < 2 || mesh_size > 32) return cdg;

    cdg.num_channels = mesh_size * mesh_size * 4; /* 4 directions per node */
    int32_t sx, sy, dx, dy;

    /* For each (src, dst) pair, trace the path and record dependencies */
    for (sy = 0; sy < mesh_size; sy++) {
        for (sx = 0; sx < mesh_size; sx++) {
            int32_t src = sy * mesh_size + sx;
            for (dy = 0; dy < mesh_size; dy++) {
                for (dx = 0; dx < mesh_size; dx++) {
                    int32_t dst = dy * mesh_size + dx;
                    if (src == dst) continue;

                    /* Trace path from src to dst */
                    int32_t cur_x = sx, cur_y = sy;
                    int32_t prev_ch = -1; /* Previous channel traversed */
                    int32_t hops;
                    int32_t max_hops = 2 * mesh_size;

                    for (hops = 0; hops < max_hops; hops++) {
                        if (cur_x == dx && cur_y == dy) break;

                        noc_direction_t dir = noc_routing_dispatch(algo,
                            sx, sy, dx, dy, cur_x, cur_y);

                        int32_t ch_id = (cur_y * mesh_size + cur_x) * 4 +
                                        (int32_t)dir - 1;
                        if (dir == NOC_DIR_LOCAL) break;
                        if (dir < NOC_DIR_EAST || dir > NOC_DIR_NORTH) break;

                        /* Record dependency from prev_ch to current channel */
                        if (prev_ch >= 0) {
                            /* Check if dependency already recorded */
                            int already = 0;
                            int32_t e;
                            for (e = 0; e < cdg.num_edges; e++) {
                                if (cdg.edges[e].from_channel == prev_ch &&
                                    cdg.edges[e].to_channel == ch_id) {
                                    already = 1;
                                    break;
                                }
                            }
                            if (!already && cdg.num_edges < NOC_CDG_MAX_EDGES &&
                                (int32_t)ch_id < 256 && prev_ch < 256) {
                                cdg.edges[cdg.num_edges].from_channel = prev_ch;
                                cdg.edges[cdg.num_edges].to_channel = ch_id;
                                cdg.edges[cdg.num_edges].from_node = src;
                                cdg.edges[cdg.num_edges].to_node = dst;
                                cdg.adj_matrix[prev_ch][ch_id] = 1;
                                cdg.num_edges++;
                            }
                        }
                        prev_ch = ch_id;

                        /* Move to next node */
                        switch (dir) {
                        case NOC_DIR_EAST:  cur_x++; break;
                        case NOC_DIR_WEST:  cur_x--; break;
                        case NOC_DIR_SOUTH: cur_y++; break;
                        case NOC_DIR_NORTH: cur_y--; break;
                        default: break;
                        }
                        /* Clamp to mesh bounds */
                        if (cur_x < 0) cur_x = 0;
                        if (cur_x >= mesh_size) cur_x = mesh_size - 1;
                        if (cur_y < 0) cur_y = 0;
                        if (cur_y >= mesh_size) cur_y = mesh_size - 1;
                    }
                }
            }
        }
    }

    return cdg;
}

/* ??? CDG cycle detection (DFS with coloring) ?????????????????????????? */

/**
 * DFS helper: detect back edges using 3-color marking.
 * WHITE=0 (unvisited), GRAY=1 (in recursion stack), BLACK=2 (done)
 */
static int cdg_dfs_cycle(int node, uint8_t *color, int n,
                         const noc_channel_dep_graph_t *cdg) {
    color[node] = 1; /* GRAY */

    int32_t v;
    for (v = 0; v < n; v++) {
        if (cdg->adj_matrix[node][v]) {
            if (color[v] == 1) {
                return 1; /* Back edge ? cycle */
            }
            if (color[v] == 0) {
                if (cdg_dfs_cycle(v, color, n, cdg)) {
                    return 1;
                }
            }
        }
    }

    color[node] = 2; /* BLACK */
    return 0;
}

int noc_cdg_has_cycle(const noc_channel_dep_graph_t *cdg) {
    if (!cdg || cdg->num_channels <= 0) return 0;

    int32_t n = cdg->num_channels;
    uint8_t *color = (uint8_t *)calloc((size_t)n, sizeof(uint8_t));
    if (!color) return 0;

    int32_t i;
    int has_cycle = 0;
    for (i = 0; i < n; i++) {
        if (color[i] == 0) {
            if (cdg_dfs_cycle(i, color, n, cdg)) {
                has_cycle = 1;
                break;
            }
        }
    }

    free(color);
    return has_cycle;
}

/* ??? CDG DOT output ?????????????????????????????????????????????????? */

void noc_cdg_print_dot(const noc_channel_dep_graph_t *cdg) {
    if (!cdg) return;

    printf("digraph CDG {\n");
    printf("  rankdir=LR;\n");
    printf("  node [shape=ellipse];\n");

    int32_t i;
    for (i = 0; i < cdg->num_edges; i++) {
        printf("  ch%d -> ch%d;\n",
               cdg->edges[i].from_channel, cdg->edges[i].to_channel);
    }

    printf("}\n");
}
