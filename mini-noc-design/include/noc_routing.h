/**
 * noc_routing.h ? L4/L5: NoC Routing Algorithms
 *
 * Implements deterministic, oblivious, and adaptive routing algorithms
 * for mesh/torus NoC topologies.
 *
 * Core Concepts (L2):
 *   - Dimension-Ordered Routing (DOR): XY-routing
 *   - Turn Model: restricting turns to prevent deadlock
 *   - Adaptive routing: path diversity for load balancing
 *
 * Standards/Theorems (L4):
 *   - Glass & Ni, "The Turn Model for Adaptive Routing" (ISCA 1992)
 *     ? Theorem: Prohibiting just enough turns breaks all cycles in CDG
 *   - Duato, "A New Theory of Deadlock-Free Adaptive Routing" (IEEE TPDS 1993)
 *     ? Theorem: A routing function is deadlock-free iff no cycle in
 *       the extended channel dependency graph exists without an escape path
 *
 * Course Mapping:
 *   - Stanford CS 315A: adaptive routing, turn model
 *   - CMU 18-742: deadlock-free routing theory
 *   - ?? ?????: routing algorithms, deadlock prevention
 */

#ifndef NOC_ROUTING_H
#define NOC_ROUTING_H

#include <stdint.h>
#include <stddef.h>
#include "noc_topology.h"

typedef noc_direction_t (*noc_routing_func_t)(int32_t src_x, int32_t src_y,
                                               int32_t dst_x, int32_t dst_y,
                                               int32_t router_x, int32_t router_y);

typedef enum {
    ROUTE_XY           = 0,
    ROUTE_YX           = 1,
    ROUTE_WEST_FIRST   = 2,
    ROUTE_NORTH_LAST   = 3,
    ROUTE_NEGATIVE_FIRST = 4,
    ROUTE_ODD_EVEN     = 5,
    ROUTE_ADAPTIVE     = 6,
    ROUTE_RANDOMIZED   = 7,
    ROUTE_COUNT
} noc_route_algo_t;

typedef enum {
    TURN_ES = 0, TURN_EN = 1, TURN_WS = 2, TURN_WN = 3,
    TURN_SE = 4, TURN_SW = 5, TURN_NE = 6, TURN_NW = 7,
    TURN_180 = 8, TURN_COUNT = 9
} noc_turn_t;

typedef struct noc_turn_table {
    noc_route_algo_t algo;
    int32_t allowed[TURN_COUNT];
    const char      *algo_name;
} noc_turn_table_t;

typedef struct noc_cdg_edge {
    int32_t from_channel;
    int32_t to_channel;
    int32_t from_node;
    int32_t to_node;
} noc_cdg_edge_t;

#define NOC_CDG_MAX_EDGES 4096

typedef struct noc_channel_dep_graph {
    int32_t        num_channels;
    int32_t        num_edges;
    noc_cdg_edge_t edges[NOC_CDG_MAX_EDGES];
    uint8_t        adj_matrix[256][256];
} noc_channel_dep_graph_t;

typedef struct noc_routing_table_entry {
    int32_t dst_node;
    noc_direction_t output_port;
    int32_t vc;
    int32_t cost;
} noc_routing_table_entry_t;

#define NOC_ROUTE_TABLE_MAX_ENTRIES 256

typedef struct noc_routing_table {
    int32_t                  router_id;
    int32_t                  num_entries;
    noc_routing_table_entry_t entries[NOC_ROUTE_TABLE_MAX_ENTRIES];
} noc_routing_table_t;

const noc_turn_table_t *noc_turn_model_get(noc_route_algo_t algo);
int noc_turn_prohibited(noc_route_algo_t algo, noc_turn_t turn);
int noc_turn_model_is_deadlock_free(const noc_turn_table_t *table, int32_t mesh_size);

noc_direction_t noc_route_xy(int32_t src_x, int32_t src_y,
                              int32_t dst_x, int32_t dst_y,
                              int32_t cur_x, int32_t cur_y);
noc_direction_t noc_route_yx(int32_t src_x, int32_t src_y,
                              int32_t dst_x, int32_t dst_y,
                              int32_t cur_x, int32_t cur_y);
noc_direction_t noc_route_west_first(int32_t src_x, int32_t src_y,
                                      int32_t dst_x, int32_t dst_y,
                                      int32_t cur_x, int32_t cur_y);
noc_direction_t noc_route_north_last(int32_t src_x, int32_t src_y,
                                      int32_t dst_x, int32_t dst_y,
                                      int32_t cur_x, int32_t cur_y);
noc_direction_t noc_route_negative_first(int32_t src_x, int32_t src_y,
                                          int32_t dst_x, int32_t dst_y,
                                          int32_t cur_x, int32_t cur_y);
noc_direction_t noc_route_odd_even(int32_t src_x, int32_t src_y,
                                    int32_t dst_x, int32_t dst_y,
                                    int32_t cur_x, int32_t cur_y);
noc_direction_t noc_route_adaptive(int32_t src_x, int32_t src_y,
                                    int32_t dst_x, int32_t dst_y,
                                    int32_t cur_x, int32_t cur_y,
                                    const int32_t *channel_load);
noc_direction_t noc_route_randomized(int32_t src_x, int32_t src_y,
                                      int32_t dst_x, int32_t dst_y,
                                      int32_t cur_x, int32_t cur_y,
                                      uint32_t seed);

noc_routing_table_t noc_build_routing_table(int32_t router_x, int32_t router_y,
                                             int32_t mesh_size,
                                             noc_route_algo_t algo);
noc_direction_t noc_routing_dispatch(noc_route_algo_t algo,
                                      int32_t src_x, int32_t src_y,
                                      int32_t dst_x, int32_t dst_y,
                                      int32_t cur_x, int32_t cur_y);
noc_channel_dep_graph_t noc_cdg_build(noc_route_algo_t algo, int32_t mesh_size);
int noc_cdg_has_cycle(const noc_channel_dep_graph_t *cdg);
void noc_cdg_print_dot(const noc_channel_dep_graph_t *cdg);

#endif /* NOC_ROUTING_H */
