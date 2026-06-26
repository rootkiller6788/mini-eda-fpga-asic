#ifndef TIMING_FPGA_H
#define TIMING_FPGA_H

#include "fpga_arch.h"
#include "routing_fabric.h"
#include <stdbool.h>

/* ================================================================
 * L2/L4/L5: FPGA Static Timing Analysis
 * References: VPR STA engine, Xilinx TRACE, Synopsys PrimeTime
 * L4: Elmore Delay Model — tau = sum(R_k * C_downstream_k)
 * L4: Setup/Hold Time Constraints — T_clk ≥ t_cq + t_logic + t_su
 * L5: Critical Path Method (CPM) with topological sort
 * ================================================================ */

/* --- Timing Graph Node ---
 * Nodes represent pins or wire segments in the timing graph
 */
typedef enum {
    TNODE_INPUT,     /* primary input / FF output */
    TNODE_OUTPUT,    /* primary output / FF input */
    TNODE_COMB,      /* combinational logic (LUT output) */
    TNODE_FF_SRC,    /* flip-flop Q output */
    TNODE_FF_SINK,   /* flip-flop D input */
    TNODE_CLK_SRC,   /* clock source */
    TNODE_CLK_SINK   /* clock sink */
} FpgaTimingNodeType;

typedef struct {
    int                 node_id;
    FpgaTimingNodeType  type;
    double              arrival_time;    /* latest signal arrival */
    double              required_time;   /* latest required arrival */
    double              slack;           /* required - arrival */
    double              delay;           /* intrinsic delay of this node */
    int                 clk_domain;      /* clock domain index */
    int*                fanins;          /* incoming timing edges */
    int                 num_fanins;
    int*                fanouts;         /* outgoing timing edges */
    int                 num_fanouts;
    int                 related_net;     /* net driving this node */
} FpgaTimingNode;

/* --- Timing Edge ---
 * Directed edge in timing graph: from source to sink
 */
typedef struct {
    int     edge_id;
    int     from_node;
    int     to_node;
    double  delay;          /* total edge delay */
    double  wire_delay;     /* RC wire delay (Elmore) */
    double  logic_delay;    /* gate/LUT delay */
    bool    is_clock_path;
    bool    is_false_path;  /* intentionally not analyzed */
} FpgaTimingEdge;

#define FPGA_MAX_TIMING_NODES  4096
#define FPGA_MAX_TIMING_EDGES  8192

/* --- Timing Graph --- */
typedef struct {
    FpgaTimingNode* nodes;
    FpgaTimingEdge* edges;
    int             num_nodes;
    int             num_edges;
} FpgaTimingGraph;

/* --- Clock Domain ---
 * Clock period and uncertainty per domain
 */
typedef struct {
    int     domain_id;
    double  period;          /* clock period in ns */
    double  uncertainty;     /* clock jitter + skew margin */
    double  latency;         /* clock insertion delay */
    char    name[32];
} FpgaClockDomain;

#define FPGA_MAX_CLOCK_DOMAINS  8

/* --- Timing Constraints ---
 */
typedef struct {
    FpgaClockDomain clocks[FPGA_MAX_CLOCK_DOMAINS];
    int             num_clocks;
    double          default_period;     /* unconstrained paths */
    double          input_delay;        /* external input delay */
    double          output_delay;       /* external output delay */
} FpgaTimingConstraints;

/* --- STA Result ---
 */
typedef struct {
    double   critical_path_delay;    /* longest path delay */
    int      critical_path_length;   /* number of edges on CP */
    int*     critical_path_nodes;    /* node sequence on CP */
    double   worst_slack;            /* most negative slack */
    int      worst_slack_node;
    double   total_negative_slack;   /* TNS */
    double   worst_hold_slack;
    double   fmax;                   /* maximum operating frequency */
    int      num_setup_violations;
    int      num_hold_violations;
    double   total_wire_delay;
    double   total_logic_delay;
} FpgaStaResult;

/* L1 API */
FpgaTimingGraph* timing_graph_create(void);
void             timing_graph_destroy(FpgaTimingGraph *tg);
int              timing_graph_add_node(FpgaTimingGraph *tg, FpgaTimingNodeType type);
int              timing_graph_add_edge(FpgaTimingGraph *tg, int from, int to,
                                       double delay, double wire_delay);
void             timing_graph_set_node_delay(FpgaTimingGraph *tg, int id, double d);

/* L4: Elmore Delay Model
 * For a wire modeled as RC tree:
 *   Elmore_delay(N) = sum_over_all_C(R(source→N→C) * C)
 * This function computes the Elmore delay for a routing path.
 * Implementation: lumped RC pi-model */
double           elmore_delay(const FpgaRoutingPath *path, const FpgaRrGraph *g,
                              double r_per_seg, double c_per_seg);

/* L5: Critical Path Method
 * Forward traversal: compute arrival times
 * arr[i] = max(arr[fanin] + delay(edge))
 * Backward traversal: compute required times
 * req[i] = min(req[fanout] - delay(edge))
 * slack = req - arr
 * Complexity: O(V + E) using topological sort */
int              sta_compute_arrival(FpgaTimingGraph *tg);
int              sta_compute_required(FpgaTimingGraph *tg,
                                      const FpgaTimingConstraints *tc);
int              sta_compute_slack(FpgaTimingGraph *tg);
int              sta_analyze(FpgaTimingGraph *tg, const FpgaTimingConstraints *tc,
                             FpgaStaResult *result);

/* L5: Find critical path via back-trace */
int              sta_critical_path(FpgaTimingGraph *tg, FpgaStaResult *result);

/* L8: Statistical STA (SSTA) — Gaussian delay model
 * Mean and variance propagation instead of worst-case.
 * Reference: Visweswariah et al. "First-Order Incremental
 * Block-Based Statistical Timing Analysis", DAC 2004 */
double           ssta_propagate_arrival(FpgaTimingGraph *tg, int node_id,
                                        double mean, double variance);

/* L8: Clock Domain Crossing (CDC) analysis */
bool             cdc_check_synchronizer(const FpgaTimingGraph *tg,
                                         int src_clk, int dst_clk);
double           cdc_mtbf(const FpgaTimingGraph *tg, int src_node, int dst_node);

/* Setup/Hold check */
bool             sta_check_setup(double arrival, double required, double margin);
bool             sta_check_hold(double arrival, double required, double margin);

/* Clock constraint helpers */
void             timing_constraints_init(FpgaTimingConstraints *tc);
int              timing_add_clock(FpgaTimingConstraints *tc, double period,
                                  const char *name);
double           timing_get_clock_period(const FpgaTimingConstraints *tc, int domain);
void             timing_result_init(FpgaStaResult *r);
void             timing_result_print(const FpgaStaResult *r);
void             timing_result_destroy(FpgaStaResult *r);

#endif
