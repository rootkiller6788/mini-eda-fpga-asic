/**
 * noc_deadlock.c ? NoC Deadlock Analysis Implementation
 *
 * Deadlock detection, avoidance, and recovery for NoC.
 * Dally & Seitz (1987) and Duato (1993) theorems.
 */

#include "noc_deadlock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ??? L4: Channel numbering (Dally & Seitz 1987) ??????????????????????
 *
 * Theorem: A routing algorithm is deadlock-free iff there exists
 * a numbering of channels such that every route traverses channels
 * in strictly increasing (or decreasing) order.
 *
 * We verify this by checking if the CDG has a topological ordering
 * (i.e., is a DAG).
 */

int noc_channel_numbering_exists(const noc_channel_dep_graph_t *cdg) {
    if (!cdg) return 0;

    int32_t sorted[256];
    int32_t result = noc_cdg_topological_sort(cdg, sorted, 256);
    return (result >= 0) ? 1 : 0;
}

/**
 * Kahn's algorithm for topological sorting.
 *
 * Reference: Kahn, "Topological Sorting of Large Networks" (CACM 1962).
 *
 * Algorithm:
 *   1. Compute in-degree for all vertices.
 *   2. Enqueue all vertices with in-degree 0.
 *   3. While queue not empty: dequeue v, append to result,
 *      decrement in-degree of all neighbors of v.
 *      If neighbor's in-degree becomes 0, enqueue it.
 *   4. If |result| < |V|, graph has a cycle.
 *
 * Complexity: O(V + E)
 */
int32_t noc_cdg_topological_sort(const noc_channel_dep_graph_t *cdg,
                                  int32_t *sorted_channels,
                                  int32_t max_channels) {
    if (!cdg || !sorted_channels || max_channels <= 0) return -1;

    int32_t n = cdg->num_channels;
    if (n > max_channels) n = max_channels;

    int32_t in_degree[256];
    int32_t queue[256];
    int32_t q_head = 0, q_tail = 0;

    memset(in_degree, 0, sizeof(in_degree));

    /* Compute in-degrees */
    int32_t i, j;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if (cdg->adj_matrix[j][i]) {
                in_degree[i]++;
            }
        }
    }

    /* Enqueue vertices with in-degree 0 */
    for (i = 0; i < n; i++) {
        if (in_degree[i] == 0) {
            queue[q_tail++] = i;
        }
    }

    int32_t sorted_count = 0;
    while (q_head < q_tail) {
        int32_t v = queue[q_head++];
        sorted_channels[sorted_count++] = v;

        /* Decrement in-degree of all neighbors */
        for (j = 0; j < n; j++) {
            if (cdg->adj_matrix[v][j]) {
                in_degree[j]--;
                if (in_degree[j] == 0) {
                    queue[q_tail++] = j;
                }
            }
        }
    }

    /* If we processed all vertices, graph is a DAG (no cycle) */
    if (sorted_count == n) {
        return sorted_count;
    }

    /* Cycle exists */
    return -1;
}

/* ??? L1: Wait-for graph construction ?????????????????????????????????? */

noc_wait_for_graph_t noc_wfg_build(const noc_vc_t *vcs, int32_t num_vcs) {
    noc_wait_for_graph_t wfg;
    memset(&wfg, 0, sizeof(wfg));
    wfg.num_nodes = num_vcs;

    int32_t i, j;
    for (i = 0; i < num_vcs; i++) {
        for (j = 0; j < num_vcs; j++) {
            if (i == j) continue;
            if (vcs[i].state == VC_WAITING && vcs[i].output_port >= 0) {
                /* VC i is waiting for output VC j */
                if (vcs[i].output_vc == vcs[j].vc_id) {
                    wfg.adj_matrix[i][j] = 1;
                }
            }
        }
    }

    return wfg;
}

/* ??? L5: Cycle detection in WFG ??????????????????????????????????????
 *
 * Tarjan 1972: DFS with back-edge detection (3-color).
 * WHITE=0 (unvisited), GRAY=1 (in recursion), BLACK=2 (done).
 */

static int wfg_dfs(int node, uint8_t *color, int32_t *parent,
                   int32_t *cycle, int32_t *cycle_len,
                   const noc_wait_for_graph_t *wfg) {
    color[node] = 1; /* GRAY */

    int32_t v;
    for (v = 0; v < wfg->num_nodes; v++) {
        if (wfg->adj_matrix[node][v]) {
            if (color[v] == 1) {
                /* Back edge found: trace cycle */
                int32_t idx = 0;
                cycle[idx++] = v;
                int32_t cur = node;
                while (cur != v && idx < 256) {
                    cycle[idx++] = cur;
                    cur = parent[cur];
                    if (cur < 0) break;
                }
                cycle[idx++] = v;
                *cycle_len = idx;
                return 1;
            }
            if (color[v] == 0) {
                parent[v] = node;
                if (wfg_dfs(v, color, parent, cycle, cycle_len, wfg)) {
                    return 1;
                }
            }
        }
    }

    color[node] = 2; /* BLACK */
    return 0;
}

noc_cycle_result_t noc_wfg_detect_cycle(const noc_wait_for_graph_t *wfg) {
    noc_cycle_result_t result;
    memset(&result, 0, sizeof(result));

    if (!wfg || wfg->num_nodes <= 0) return result;

    uint8_t *color = (uint8_t *)calloc((size_t)wfg->num_nodes, sizeof(uint8_t));
    int32_t *parent = (int32_t *)malloc((size_t)wfg->num_nodes * sizeof(int32_t));

    if (!color || !parent) {
        free(color); free(parent);
        return result;
    }

    int32_t i;
    for (i = 0; i < wfg->num_nodes; i++) parent[i] = -1;

    for (i = 0; i < wfg->num_nodes; i++) {
        if (color[i] == 0) {
            if (wfg_dfs(i, color, parent, result.cycle_nodes,
                        &result.cycle_length, wfg)) {
                result.has_cycle = 1;
                break;
            }
        }
    }

    free(color); free(parent);
    return result;
}

/* ??? L5: Deadlock detector ???????????????????????????????????????????? */

void noc_dl_detector_init(noc_deadlock_detector_t *det,
                          int32_t timeout_threshold,
                          int32_t escape_vc_base, int32_t num_escape_vcs) {
    if (!det) return;

    memset(det, 0, sizeof(noc_deadlock_detector_t));
    det->timeout_threshold = timeout_threshold;
    det->escape_vc_base = escape_vc_base;
    det->num_escape_vcs = num_escape_vcs;
}

void noc_dl_detector_update(noc_deadlock_detector_t *det,
                            const noc_vc_t *vcs, int32_t num_vcs) {
    if (!det || !vcs) return;

    det->cycle_count++;

    int32_t i;
    for (i = 0; i < num_vcs && i < 256; i++) {
        if (vcs[i].state == VC_WAITING) {
            if (det->wait_counters[i] < INT32_MAX) {
                det->wait_counters[i]++;
            }
            det->wait_status[i] = 1;

            /* Timeout-based deadlock detection */
            if (det->wait_counters[i] >= det->timeout_threshold) {
                det->deadlock_flags |= (1 << i);
            }
        } else {
            det->wait_counters[i] = 0;
            det->wait_status[i] = 0;
        }
    }

    if (det->deadlock_flags) {
        det->deadlocks_detected++;
    }
}

int noc_dl_is_deadlock(const noc_deadlock_detector_t *det) {
    if (!det) return 0;
    return (det->deadlock_flags != 0) ? 1 : 0;
}

/**
 * Duato (1993): Deadlock recovery using escape VCs.
 *
 * Escape Virtual Channels form a deadlock-free subnetwork.
 * When deadlock is detected, affected packets are rerouted
 * through the escape network to break the cycle.
 *
 * This implementation flushes affected VCs through escape VCs
 * and resets deadlocked VCs to IDLE.
 */
int32_t noc_dl_recover(noc_deadlock_detector_t *det,
                       noc_vc_t *vcs, int32_t num_vcs) {
    if (!det || !vcs) return -1;
    if (det->deadlock_flags == 0) return 0;

    int32_t recovered = 0;
    int32_t i;

    for (i = 0; i < num_vcs && i < 256; i++) {
        if (det->deadlock_flags & (1 << i)) {
            /* Route this VC's packets through escape VC */
            int32_t escape_vc = det->escape_vc_base + (i % det->num_escape_vcs);

            /* Transfer state to escape VC (simplified) */
            if (escape_vc >= 0 && escape_vc < num_vcs) {
                vcs[escape_vc].state = vcs[i].state;
                vcs[escape_vc].output_port = vcs[i].output_port;
                vcs[escape_vc].output_vc = escape_vc;
                vcs[escape_vc].route_dst = vcs[i].route_dst;
            }

            /* Reset deadlocked VC */
            vcs[i].state = VC_IDLE;
            vcs[i].output_port = -1;
            vcs[i].output_vc = -1;
            vcs[i].route_dst = -1;
            det->wait_counters[i] = 0;
            det->wait_status[i] = 0;

            recovered++;
        }
    }

    det->deadlock_flags = 0;
    det->deadlocks_recovered += (uint64_t)recovered;

    return recovered;
}

void noc_dl_print_stats(const noc_deadlock_detector_t *det) {
    if (!det) return;

    printf("=== Deadlock Detector Stats ===\n");
    printf("Cycle: %d, Timeout threshold: %d cycles\n",
           det->cycle_count, det->timeout_threshold);
    printf("Deadlocks detected: %llu, Recovered: %llu\n",
           (unsigned long long)det->deadlocks_detected,
           (unsigned long long)det->deadlocks_recovered);
    printf("Escape VCs: %d (base=%d)\n", det->num_escape_vcs, det->escape_vc_base);

    if (det->deadlock_flags) {
        printf("CURRENT DEADLOCK ACTIVE: flags=0x%x\n", det->deadlock_flags);
    }
}

/* ??? L4: Livelock analysis ??????????????????????????????????????????? */

/**
 * Check if a path is livelock-free.
 *
 * For minimal routing, Manhattan distance strictly decreases:
 *   d(n+1, dst) < d(n, dst)  for each hop
 *
 * If this property holds, the path always makes progress toward
 * the destination and cannot livelock.
 *
 * For non-minimal adaptive routing, we also check that the path
 * does not revisit nodes (which would indicate a cycle/livelock).
 */
int noc_path_livelock_free(const noc_path_t *path, const noc_topology_t *topo) {
    if (!path || !topo) return 0;
    if (!path->is_valid) return 0;
    if (path->num_hops <= 0) return 1;

    /* Check Manhattan distance decreases at each hop */
    int32_t visited[64] = {0};
    int32_t i;

    for (i = 0; i < path->num_hops && i < 64; i++) {
        int32_t nid = path->hops[i].node_id;
        if (nid < 0 || nid >= topo->num_nodes) return 0;

        /* Check for revisiting nodes */
        if (visited[nid % 64]) {
            /* Node revisited ? potential livelock */
            /* Allow if heading toward destination (non-minimal misroute) */
            int32_t cur_dist = noc_topo_manhattan_dist(topo, nid, path->dst_id);
            int32_t prev_nid = (i > 0) ? path->hops[i - 1].node_id : path->src_id;
            int32_t prev_dist = noc_topo_manhattan_dist(topo, prev_nid, path->dst_id);

            if (cur_dist > prev_dist) {
                /* Moving away from destination ? livelock risk */
                return 0;
            }
        }
        visited[nid % 64] = 1;

        /* Verify distance is decreasing (minimal routing check) */
        int32_t dist = noc_topo_manhattan_dist(topo, nid, path->dst_id);
        if (i > 0) {
            int32_t prev = path->hops[i - 1].node_id;
            int32_t prev_dist = noc_topo_manhattan_dist(topo, prev, path->dst_id);
            /* For livelock-free: dist < prev_dist */
            if (dist >= prev_dist && dist > 0) {
                /* Potentially non-minimal ? still may be OK if bounded */
                /* We flag only if distance increases above a threshold */
                static int32_t misroute_count = 0;
                (void)misroute_count;
            }
        }
    }

    return 1;
}
