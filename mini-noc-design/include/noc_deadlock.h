/**
 * noc_deadlock.h ? L4/L5: NoC Deadlock Analysis
 *
 * Deadlock detection, avoidance, and recovery for NoC.
 *
 * Standards/Theorems (L4):
 *   - Dally & Seitz, "Deadlock-Free Message Routing in Multiprocessor
 *     Interconnection Networks" (IEEE TC 1987)
 *     ? Theorem: A routing algorithm is deadlock-free iff there exists
 *       a numbering of channels such that every route traverses channels
 *       in strictly increasing (or decreasing) order.
 *
 *   - Duato, "A New Theory of Deadlock-Free Adaptive Routing" (IEEE TPDS 1993)
 *     ? Theorem: Adaptive routing is deadlock-free if there exists a
 *       connected deadlock-free routing subfunction that acts as an escape path.
 *
 * Algorithms (L5):
 *   - Cycle detection in Channel Dependency Graph (DFS)
 *   - Escape VC allocation for deadlock recovery
 *   - Turn model verification
 *
 * Course Mapping:
 *   - MIT 6.004: deadlock in systems
 *   - CMU 15-418: parallel systems ? deadlock resolution
 *   - ?? ????: deadlock prevention
 */

#ifndef NOC_DEADLOCK_H
#define NOC_DEADLOCK_H

#include <stdint.h>
#include <stddef.h>
#include "noc_topology.h"
#include "noc_router.h"
#include "noc_routing.h"

/* ??? L1: Deadlock state machine ????????????????????????????????????? */

typedef enum {
    DL_STATE_FREE       = 0,
    DL_STATE_WAITING    = 1,
    DL_STATE_DETECTED   = 2,
    DL_STATE_RECOVERING = 3
} noc_dl_state_t;

typedef struct noc_deadlock_detector {
    int32_t       cycle_count;
    int32_t       timeout_threshold;  /* Flits waiting > threshold = potential DL */
    uint8_t       wait_status[256];   /* Per-VC: 1 if waiting for downstream */
    int32_t       wait_counters[256]; /* Per-VC: consecutive cycles waiting */
    int32_t       deadlock_flags;     /* Bitmask of VCs in deadlock */
    /* Recovery */
    int32_t       escape_vc_base;     /* First escape VC index */
    int32_t       num_escape_vcs;
    uint64_t      deadlocks_detected;
    uint64_t      deadlocks_recovered;
} noc_deadlock_detector_t;

/* ??? L1: Wait-for graph ????????????????????????????????????????????? */

typedef struct noc_wait_for_graph {
    int32_t num_nodes;
    int32_t adj_matrix[256][256];     /* [i][j] = 1 if i waits for j */
} noc_wait_for_graph_t;

/* ??? L1: Cycle analysis result ?????????????????????????????????????? */

typedef struct noc_cycle_result {
    int32_t has_cycle;
    int32_t cycle_nodes[256];
    int32_t cycle_length;
} noc_cycle_result_t;

/* ??? L4: Channel numbering per Dally & Seitz (1987) ?????????????????? */

/**
 * Assign a strict ordering to all channels in the network.
 * If every routing path visits channels in strictly increasing order,
 * the routing algorithm is deadlock-free (Dally & Seitz Theorem).
 *
 * Returns 1 if a valid numbering exists, 0 if deadlock-prone.
 * Complexity: O(V + E) for CDG topological sort.
 */
int noc_channel_numbering_exists(const noc_channel_dep_graph_t *cdg);

/**
 * Topological sort of CDG to verify acyclicity.
 * If sort succeeds, routing is deadlock-free.
 *
 * Returns number of sorted channels on success, -1 if cycle detected.
 * Complexity: O(V + E) ? Kahn's algorithm.
 *
 * Reference: Kahn, "Topological Sorting of Large Networks" (CACM 1962).
 */
int32_t noc_cdg_topological_sort(const noc_channel_dep_graph_t *cdg,
                                  int32_t *sorted_channels,
                                  int32_t max_channels);

/**
 * Build wait-for graph from current VC states across all routers.
 * Complexity: O(V^2) where V = total VCs.
 */
noc_wait_for_graph_t noc_wfg_build(const noc_vc_t *vcs, int32_t num_vcs);

/* ??? L5: Cycle detection ???????????????????????????????????????????? */

/**
 * Detect cycles in wait-for graph using DFS with coloring.
 *
 * DFS colors: 0=white(unvisited), 1=gray(in-stack), 2=black(done).
 *
 * Returns cycle information if a back-edge is found.
 * Complexity: O(V + E) ? standard DFS cycle detection.
 *
 * Reference: Tarjan, "Depth-First Search and Linear Graph
 * Algorithms" (SIAM J. Computing 1972).
 */
noc_cycle_result_t noc_wfg_detect_cycle(const noc_wait_for_graph_t *wfg);

/* ??? L5: Deadlock detection ????????????????????????????????????????? */

/**
 * Initialize deadlock detector.
 * Complexity: O(V) where V = num VCs.
 */
void noc_dl_detector_init(noc_deadlock_detector_t *det,
                          int32_t timeout_threshold,
                          int32_t escape_vc_base, int32_t num_escape_vcs);

/**
 * Update detector with current VC states. Called every cycle.
 * Complexity: O(V) per call.
 */
void noc_dl_detector_update(noc_deadlock_detector_t *det,
                            const noc_vc_t *vcs, int32_t num_vcs);

/**
 * Check if deadlock has been detected.
 * Returns 1 if deadlock exists, 0 otherwise.
 * Complexity: O(1)
 */
int noc_dl_is_deadlock(const noc_deadlock_detector_t *det);

/**
 * Recover from deadlock by routing affected packets through escape VCs.
 * Uses Duato's escape path method: drain deadlocked packets through
 * a pre-allocated deadlock-free escape network.
 *
 * Returns number of packets recovered.
 * Complexity: O(D * P) where D = deadlocked VCs, P = packets per VC.
 *
 * Reference: Duato, "A New Theory of Deadlock-Free Adaptive Routing" (1993).
 */
int32_t noc_dl_recover(noc_deadlock_detector_t *det,
                       noc_vc_t *vcs, int32_t num_vcs);

/**
 * Print deadlock detection statistics.
 */
void noc_dl_print_stats(const noc_deadlock_detector_t *det);

/* ??? L4: Livelock analysis ?????????????????????????????????????????? */

/**
 * Check if a routing path is livelock-free (always makes progress toward dest).
 *
 * For minimal routing on mesh, livelock is impossible because
 * Manhattan distance strictly decreases at each hop.
 *
 * For non-minimal routing, we verify that each hop reduces distance
 * or that a bound on maximum misroute steps exists.
 *
 * Returns 1 if livelock-free, 0 if potential livelock.
 * Complexity: O(H) where H = path length.
 */
int noc_path_livelock_free(const noc_path_t *path, const noc_topology_t *topo);

#endif /* NOC_DEADLOCK_H */
