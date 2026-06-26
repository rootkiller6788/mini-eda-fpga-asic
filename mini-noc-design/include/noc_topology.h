/**
 * noc_topology.h — L1/L2: Network-on-Chip Topology Definitions
 *
 * Defines graph-based NoC topologies: mesh, torus, ring, tree,
 * butterfly (BFT), fat-tree, and Clos networks.
 *
 * Core Concepts (L2):
 *   - Direct vs indirect networks
 *   - Node degree, diameter, bisection bandwidth
 *   - k-ary n-cube family
 *
 * Standards (L4):
 *   - Dally & Towles: Principles and Practices of Interconnection Networks (2004)
 *   - Bisection bandwidth lower bound: B >= N/2 for uniform random traffic
 *
 * Course Mapping:
 *   - MIT 6.004: Computation Structures → interconnect networks
 *   - Stanford CS 315A: Parallel Computer Architecture and Programming → topology
 *   - CMU 18-742: Parallel Computer Architecture → network topology
 */

#ifndef NOC_TOPOLOGY_H
#define NOC_TOPOLOGY_H

#include <stdint.h>
#include <stddef.h>

/* ─── L1: Core Type Definitions ─────────────────────────────────────── */

/** Topology type enumeration (k-ary n-cube family + indirect topologies) */
typedef enum {
    TOPO_MESH_2D,
    TOPO_TORUS_2D,
    TOPO_RING,
    TOPO_TREE,
    TOPO_BUTTERFLY,
    TOPO_CUSTOM,
    TOPO_COUNT
} noc_topo_type_t;

/** Direction encoding for port mapping (up to 8 ports) */
typedef enum {
    NOC_DIR_LOCAL  = 0,
    NOC_DIR_EAST   = 1,
    NOC_DIR_WEST   = 2,
    NOC_DIR_SOUTH  = 3,
    NOC_DIR_NORTH  = 4,
    NOC_DIR_UP     = 5,
    NOC_DIR_DOWN   = 6,
    NOC_DIR_MAX    = 7
} noc_direction_t;

/** Node in the NoC topology graph */
typedef struct noc_node {
    int32_t  id;
    int32_t  x, y, z;
    int32_t  degree;
    int32_t  links[NOC_DIR_MAX]; /* -1 = no connection */
    uint32_t flags;
} noc_node_t;

#define NOC_NODE_FLAG_BOUNDARY   (1u << 0)
#define NOC_NODE_FLAG_ROOT       (1u << 1)
#define NOC_NODE_FLAG_LEAF       (1u << 2)
#define NOC_NODE_FLAG_HOTSPOT    (1u << 3)

/** Edge/link in the topology */
typedef struct noc_edge {
    int32_t src;
    int32_t dst;
    int32_t bandwidth_bps;
    int32_t latency_cycles;
} noc_edge_t;

/** Topology graph representation */
typedef struct noc_topology {
    noc_topo_type_t type;
    char            name[64];
    int32_t         num_nodes;
    int32_t         num_edges;
    int32_t         k;
    int32_t         n;
    noc_node_t     *nodes;
    noc_edge_t     *edges;
    int32_t         diameter;
    int32_t         bisection_bw;
    double          avg_distance;
} noc_topology_t;

/* ─── L1: Path and routing structures ──────────────────────────────── */

typedef struct noc_hop {
    int32_t         node_id;
    noc_direction_t out_port;
    int32_t         vc;
} noc_hop_t;

#define NOC_PATH_MAX_HOPS 256
typedef struct noc_path {
    int32_t     src_id;
    int32_t     dst_id;
    int32_t     num_hops;
    noc_hop_t   hops[NOC_PATH_MAX_HOPS];
    int32_t     total_latency;
    int32_t     is_valid;
} noc_path_t;

/* ─── L4: Analytical topology metrics ──────────────────────────────── */

typedef struct noc_topo_metrics {
    int32_t node_count;
    int32_t edge_count;
    int32_t diameter;
    double  avg_distance;
    double  bisection_width;
    double  bisection_bw_bps;
    double  channel_load;
    double  zero_load_latency;
    double  saturation_throughput;
} noc_topo_metrics_t;

/* ─── API: Topology Construction ──────────────────────────────────── */

noc_topology_t *noc_topo_create_mesh_2d(int32_t k);
noc_topology_t *noc_topo_create_torus_2d(int32_t k);
noc_topology_t *noc_topo_create_ring(int32_t n);
noc_topology_t *noc_topo_create_tree(int32_t k, int32_t levels);
noc_topology_t *noc_topo_create_butterfly(int32_t k, int32_t stages);
void noc_topo_free(noc_topology_t *topo);

/* ─── L3: Graph Operations ────────────────────────────────────────── */

int32_t *noc_topo_all_pairs_shortest(const noc_topology_t *topo);
int32_t noc_topo_diameter(const noc_topology_t *topo);
double noc_topo_avg_distance(const noc_topology_t *topo);
int32_t noc_topo_bisection_width(const noc_topology_t *topo);
noc_topo_metrics_t noc_topo_compute_metrics(const noc_topology_t *topo,
                                             double link_bw_bps,
                                             double flit_size_bits);
noc_path_t noc_topo_find_path_xy(const noc_topology_t *topo,
                                  int32_t src_id, int32_t dst_id);
noc_path_t noc_topo_find_path_dijkstra(const noc_topology_t *topo,
                                        int32_t src_id, int32_t dst_id);
int noc_topo_are_neighbors(const noc_topology_t *topo,
                           int32_t a, int32_t b);
int32_t noc_topo_manhattan_dist(const noc_topology_t *topo,
                                int32_t src_id, int32_t dst_id);
int noc_topo_validate(const noc_topology_t *topo);
void noc_topo_print(const noc_topology_t *topo);
int noc_topo_export_dot(const noc_topology_t *topo, const char *filename);

#endif /* NOC_TOPOLOGY_H */
