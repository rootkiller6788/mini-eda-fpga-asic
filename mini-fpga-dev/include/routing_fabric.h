#ifndef ROUTING_FABRIC_H
#define ROUTING_FABRIC_H

#include "fpga_arch.h"
#include <stdbool.h>

/* ================================================================
 * L3/L5: FPGA Routing Fabric — PathFinder & Maze Routing
 * References: VPR routing, PathFinder Negotiated Congestion
 * L4: Dijkstra's Shortest Path (for maze routing)
 * L5: PathFinder Algorithm (McMurchie & Ebeling, FPGA 1995)
 * L5: Lee's Maze Routing Algorithm (Lee, 1961)
 * L5: A* Search for timing-driven routing
 * ================================================================ */

/* --- Routing Resource Graph (RR Graph) ---
 * The routing architecture is modeled as a directed graph.
 * Nodes = wire segments / CLB pins
 * Edges = programmable switches
 */
typedef enum {
    RR_SOURCE,   /* CLB output pin — net source */
    RR_SINK,     /* CLB input pin — net sink */
    RR_CHANX,    /* Horizontal channel wire */
    RR_CHANY,    /* Vertical channel wire */
    RR_IPIN,     /* Input pin to CLB */
    RR_OPIN      /* Output pin from CLB */
} FpgaRrNodeType;

typedef struct {
    int             node_id;
    FpgaRrNodeType  type;
    int             x_low, y_low;   /* bounding box coordinates */
    int             x_high, y_high;
    int             capacity;       /* 1 for most wires */
    int             occupancy;      /* number of nets using this node */
    double          base_cost;      /* intrinsic delay cost */
    double          r;              /* resistance (Elmore model) */
    double          c;              /* capacitance (Elmore model) */
    int             fan_in;         /* number of input edges */
    int*            in_edges;       /* array of node IDs driving this */
    int             fan_out;        /* number of output edges */
    int*            out_edges;      /* array of node IDs driven by this */
} FpgaRrNode;

#define FPGA_MAX_RR_NODES  8192

/* --- RR Graph --- */
typedef struct {
    FpgaRrNode* nodes;
    int         num_nodes;
    int         grid_w, grid_h;
} FpgaRrGraph;

/* --- Routing Path ---
 * A path is a sequence of RR nodes from source to sink
 */
#define FPGA_MAX_PATH_LEN  256

typedef struct {
    int  nodes[FPGA_MAX_PATH_LEN];
    int  length;
    double total_cost;
    double total_delay;
    int  net_id;
    int  sink_id;
} FpgaRoutingPath;

/* --- PathFinder State ---
 * Tracks negotiation history for congestion resolution
 */
typedef struct {
    int   node_id;
    double hist_cost;     /* historical congestion cost */
    int   times_overused;
} FpgaPathfinderHist;

#define FPGA_MAX_PATHFINDER_HIST  8192

typedef struct {
    FpgaPathfinderHist* history;
    int                 num_hist_entries;
    double              pres_fac;     /* present congestion factor */
    double              hist_fac;     /* historical congestion factor */
    int                 max_iterations;
    double              init_pres_fac;
    double              pres_fac_mult;
} FpgaPathfinderState;

/* L1/L5 API */
/* Build routing resource graph from fabric architecture */
FpgaRrGraph* rr_graph_create(const FpgaFabric *fabric);
void         rr_graph_destroy(FpgaRrGraph *g);
int          rr_graph_add_node(FpgaRrGraph *g, FpgaRrNodeType type, int x, int y);
void         rr_graph_add_edge(FpgaRrGraph *g, int from, int to);

/* L5: Lee's Maze Routing (BFS-based)
 * Guarantees shortest path if one exists.
 * O(grid_width * grid_height * channel_width) time and space.
 * Reference: C.Y. Lee, "An Algorithm for Path Connections",
 *            IRE Trans. Electronic Computers, 1961 */
int          route_maze_bfs(FpgaRrGraph *g, int src_node, int sink_node,
                            FpgaRoutingPath *path);

/* L5: A* Search Routing
 * Heuristic-guided maze routing for performance.
 * f(n) = g(n) + h(n) where h(n) is Manhattan distance heuristic.
 * O(N log N) with priority queue. */
int          route_astar(FpgaRrGraph *g, int src_node, int sink_node,
                         FpgaRoutingPath *path, bool timing_driven);

/* L5: PathFinder Negotiated Congestion
 * Iterative rip-up and reroute to resolve congestion.
 * Cost function: cost(n) = base_cost + hist_cost(n) * occupancy(n)
 * Reference: McMurchie & Ebeling, "PathFinder: A Negotiation-Based
 *            Performance-Driven Router for FPGAs", FPGA 1995 */
int          route_pathfinder(FpgaRrGraph *g, FpgaFabric *fabric,
                              FpgaNet* nets, int num_nets,
                              FpgaPathfinderState *state);

/* PathFinder iteration control */
void         pathfinder_state_init(FpgaPathfinderState *s);
void         pathfinder_state_destroy(FpgaPathfinderState *s);
double       pathfinder_node_cost(const FpgaRrNode *node, FpgaPathfinderState *s, int node_id);
void         pathfinder_update_hist(FpgaPathfinderState *s);
bool         pathfinder_is_congested(const FpgaRrGraph *g);

/* Route a single net using PathFinder */
int          route_net_pathfinder(FpgaRrGraph *g, FpgaNet *net,
                                  FpgaPathfinderState *state);

/* Route all nets (full chip routing) */
int          route_all_nets(FpgaRrGraph *g, FpgaFabric *fabric,
                            FpgaNet* nets, int num_nets);

/* Timing analysis on routing path (Elmore delay model) */
double       route_path_delay(const FpgaRoutingPath *path, const FpgaRrGraph *g,
                              double driver_resistance);

/* L8: Non-Tree Routing (for fault tolerance) */
int          route_with_redundancy(FpgaRrGraph *g, FpgaNet *net,
                                   FpgaRoutingPath *primary,
                                   FpgaRoutingPath *backup);

/* L9: ML-assisted routing cost prediction */
double       route_predict_congestion(const FpgaRrGraph *g, int node_id);

/* Connectivity check */
bool         rr_graph_is_connected(const FpgaRrGraph *g, int src, int dst);

#endif
