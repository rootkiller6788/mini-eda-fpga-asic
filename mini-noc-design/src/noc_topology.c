/**
 * noc_topology.c ? NoC Topology Implementation
 *
 * Implements graph-based topology construction, shortest-path
 * computation, and topology metrics.
 */

#include "noc_topology.h"
#include "noc_perf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ??? Internal helpers ???????????????????????????????????????????????? */

static int32_t node_id(int32_t x, int32_t y, int32_t z, int32_t k) {
    (void)z;
    return y * k + x;
}

static void node_coords(int32_t id, int32_t k, int32_t *x, int32_t *y) {
    *y = id / k;
    *x = id % k;
}

static void clear_links(noc_node_t *node) {
    int i;
    for (i = 0; i < NOC_DIR_MAX; i++) {
        node->links[i] = -1;
    }
}

static noc_edge_t *add_edge(noc_topology_t *topo, int32_t src, int32_t dst,
                             int32_t bw, int32_t lat) {
    int32_t e = topo->num_edges;
    topo->edges[e].src = src;
    topo->edges[e].dst = dst;
    topo->edges[e].bandwidth_bps = bw;
    topo->edges[e].latency_cycles = lat;
    topo->num_edges++;
    return &topo->edges[e];
}

static int32_t link_to_dir(int32_t dx, int32_t dy) {
    if (dx == 1 && dy == 0)  return NOC_DIR_EAST;
    if (dx == -1 && dy == 0) return NOC_DIR_WEST;
    if (dx == 0 && dy == 1)  return NOC_DIR_SOUTH;
    if (dx == 0 && dy == -1) return NOC_DIR_NORTH;
    return NOC_DIR_LOCAL;
}

/* ??? 2D Mesh Construction ??????????????????????????????????????????? */

noc_topology_t *noc_topo_create_mesh_2d(int32_t k) {
    if (k < 2) return NULL;

    int32_t N = k * k;
    /* Max edges: each internal node has 4, edges count each twice */
    int32_t max_edges = 2 * k * (k - 1) * 2; /* upper bound */

    noc_topology_t *topo = (noc_topology_t *)calloc(1, sizeof(noc_topology_t));
    if (!topo) return NULL;

    topo->type = TOPO_MESH_2D;
    snprintf(topo->name, sizeof(topo->name), "2D-Mesh-%dx%d", k, k);
    topo->k = k;
    topo->n = 2;
    topo->num_nodes = N;
    topo->nodes = (noc_node_t *)calloc((size_t)N, sizeof(noc_node_t));
    topo->edges = (noc_edge_t *)calloc((size_t)max_edges, sizeof(noc_edge_t));

    if (!topo->nodes || !topo->edges) {
        noc_topo_free(topo);
        return NULL;
    }

    int32_t x, y;
    for (y = 0; y < k; y++) {
        for (x = 0; x < k; x++) {
            int32_t id = node_id(x, y, 0, k);
            noc_node_t *n = &topo->nodes[id];
            n->id = id;
            n->x = x;
            n->y = y;
            n->z = 0;
            n->degree = 0;
            n->flags = 0;
            clear_links(n);

            if (x == 0 || x == k - 1 || y == 0 || y == k - 1) {
                n->flags |= NOC_NODE_FLAG_BOUNDARY;
            }
        }
    }

    /* Create edges: each bidirectional link = 2 directed edges */
    for (y = 0; y < k; y++) {
        for (x = 0; x < k; x++) {
            int32_t id = node_id(x, y, 0, k);
            noc_node_t *n = &topo->nodes[id];

            /* East */
            if (x < k - 1) {
                int32_t eid = node_id(x + 1, y, 0, k);
                n->links[NOC_DIR_EAST] = eid;
                n->degree++;
                add_edge(topo, id, eid, 1000000000, 1);
                add_edge(topo, eid, id, 1000000000, 1);
            }
            /* South */
            if (y < k - 1) {
                int32_t sid = node_id(x, y + 1, 0, k);
                n->links[NOC_DIR_SOUTH] = sid;
                n->degree++;
                add_edge(topo, id, sid, 1000000000, 1);
                add_edge(topo, sid, id, 1000000000, 1);
            }
        }
    }

    /* Connection to local (self) */
    for (y = 0; y < k; y++) {
        for (x = 0; x < k; x++) {
            int32_t id = node_id(x, y, 0, k);
            topo->nodes[id].links[NOC_DIR_LOCAL] = id;
        }
    }

    return topo;
}

/* ??? 2D Torus Construction ?????????????????????????????????????????? */

noc_topology_t *noc_topo_create_torus_2d(int32_t k) {
    if (k < 2) return NULL;

    int32_t N = k * k;
    /* Mesh edges: 2*k*(k-1)*2 directed edges.
     * Wraparound: 2*k*2 directed edges.
     * Total: 4*k*(k-1) + 4*k = 4*k*k */
    int32_t max_edges = 4 * k * k;

    noc_topology_t *topo = (noc_topology_t *)calloc(1, sizeof(noc_topology_t));
    if (!topo) return NULL;

    topo->type = TOPO_TORUS_2D;
    snprintf(topo->name, sizeof(topo->name), "2D-Torus-%dx%d", k, k);
    topo->k = k;
    topo->n = 2;
    topo->num_nodes = N;
    topo->nodes = (noc_node_t *)calloc((size_t)N, sizeof(noc_node_t));
    topo->edges = (noc_edge_t *)calloc((size_t)max_edges, sizeof(noc_edge_t));

    if (!topo->nodes || !topo->edges) {
        noc_topo_free(topo);
        return NULL;
    }

    int32_t x, y;
    for (y = 0; y < k; y++) {
        for (x = 0; x < k; x++) {
            int32_t id = node_id(x, y, 0, k);
            noc_node_t *n = &topo->nodes[id];
            n->id = id;
            n->x = x; n->y = y; n->z = 0;
            n->degree = 0;
            n->flags = 0;
            clear_links(n);
        }
    }

    /* Internal mesh edges */
    for (y = 0; y < k; y++) {
        for (x = 0; x < k; x++) {
            int32_t id = node_id(x, y, 0, k);
            noc_node_t *n = &topo->nodes[id];
            if (x < k - 1) {
                int32_t eid = node_id(x + 1, y, 0, k);
                n->links[NOC_DIR_EAST] = eid; n->degree++;
                add_edge(topo, id, eid, 1000000000, 1);
            } else {
                n->links[NOC_DIR_EAST] = node_id(0, y, 0, k); n->degree++;
                add_edge(topo, id, node_id(0, y, 0, k), 1000000000, 1);
            }
            if (x > 0) {
                int32_t wid = node_id(x - 1, y, 0, k);
                n->links[NOC_DIR_WEST] = wid; n->degree++;
            } else {
                n->links[NOC_DIR_WEST] = node_id(k - 1, y, 0, k); n->degree++;
                add_edge(topo, id, node_id(k - 1, y, 0, k), 1000000000, 1);
            }
            if (y < k - 1) {
                int32_t sid = node_id(x, y + 1, 0, k);
                n->links[NOC_DIR_SOUTH] = sid; n->degree++;
                add_edge(topo, id, sid, 1000000000, 1);
            } else {
                n->links[NOC_DIR_SOUTH] = node_id(x, 0, 0, k); n->degree++;
                add_edge(topo, id, node_id(x, 0, 0, k), 1000000000, 1);
            }
            if (y > 0) {
                int32_t nid = node_id(x, y - 1, 0, k);
                n->links[NOC_DIR_NORTH] = nid; n->degree++;
            } else {
                n->links[NOC_DIR_NORTH] = node_id(x, k - 1, 0, k); n->degree++;
                add_edge(topo, id, node_id(x, k - 1, 0, k), 1000000000, 1);
            }
            n->links[NOC_DIR_LOCAL] = id;
        }
    }

    return topo;
}

/* ─── Ring Construction ────────────────────────────────────────────── */

noc_topology_t *noc_topo_create_ring(int32_t n) {
    if (n < 2) return NULL;

    noc_topology_t *topo = (noc_topology_t *)calloc(1, sizeof(noc_topology_t));
    if (!topo) return NULL;

    topo->type = TOPO_RING;
    snprintf(topo->name, sizeof(topo->name), "Ring-%d", n);
    topo->k = n;
    topo->n = 1;
    topo->num_nodes = n;
    topo->nodes = (noc_node_t *)calloc((size_t)n, sizeof(noc_node_t));
    topo->edges = (noc_edge_t *)calloc((size_t)(2 * n), sizeof(noc_edge_t));

    if (!topo->nodes || !topo->edges) {
        noc_topo_free(topo);
        return NULL;
    }

    int32_t i;
    for (i = 0; i < n; i++) {
        noc_node_t *nd = &topo->nodes[i];
        nd->id = i;
        nd->x = i;
        nd->y = 0;
        nd->z = 0;
        nd->degree = 2;
        clear_links(nd);
        nd->links[NOC_DIR_EAST] = (i + 1) % n;
        nd->links[NOC_DIR_WEST] = (i - 1 + n) % n;
        nd->links[NOC_DIR_LOCAL] = i;

        add_edge(topo, i, (i + 1) % n, 1000000000, 1);
        add_edge(topo, (i + 1) % n, i, 1000000000, 1);
    }

    return topo;
}

/* ─── Tree (Fat-tree) Construction ──────────────────────────────────── */

noc_topology_t *noc_topo_create_tree(int32_t k, int32_t levels) {
    if (k < 2 || levels < 1) return NULL;

    int32_t total = 0;
    int32_t pow = 1;
    int32_t lvl;
    for (lvl = 0; lvl < levels; lvl++) {
        total += pow;
        pow *= k;
    }

    int32_t max_edges = 2 * (total - 1);

    noc_topology_t *topo = (noc_topology_t *)calloc(1, sizeof(noc_topology_t));
    if (!topo) return NULL;

    topo->type = TOPO_TREE;
    snprintf(topo->name, sizeof(topo->name), "FatTree-%dary-L%d", k, levels);
    topo->k = k;
    topo->n = levels;
    topo->num_nodes = total;
    topo->nodes = (noc_node_t *)calloc((size_t)total, sizeof(noc_node_t));
    topo->edges = (noc_edge_t *)calloc((size_t)max_edges, sizeof(noc_edge_t));

    if (!topo->nodes || !topo->edges) {
        noc_topo_free(topo);
        return NULL;
    }

    int32_t node_idx = 0;
    int32_t level_start = 0;
    int32_t level_count = 1;
    int32_t prev_start = 0;
    int32_t prev_count = 0;

    for (lvl = 0; lvl < levels; lvl++) {
        int32_t i;
        for (i = 0; i < level_count; i++) {
            int32_t id = node_idx + i;
            noc_node_t *nd = &topo->nodes[id];
            nd->id = id;
            nd->x = lvl;
            nd->y = i;
            nd->z = 0;
            nd->degree = 0;
            clear_links(nd);
            nd->links[NOC_DIR_LOCAL] = id;

            if (lvl == 0) {
                nd->flags |= NOC_NODE_FLAG_ROOT;
            }
            if (lvl == levels - 1) {
                nd->flags |= NOC_NODE_FLAG_LEAF;
            }

            if (lvl > 0) {
                int32_t parent_id = prev_start + (i % prev_count);
                nd->links[NOC_DIR_UP] = parent_id;
                nd->degree++;
                topo->nodes[parent_id].links[NOC_DIR_DOWN] = id;
                topo->nodes[parent_id].degree++;
                add_edge(topo, id, parent_id, 2000000000, 1);
                add_edge(topo, parent_id, id, 2000000000, 1);
            }
        }

        node_idx += level_count;
        prev_start = level_start;
        prev_count = level_count;
        level_start += level_count;
        level_count *= k;
    }

    return topo;
}

/* ─── Butterfly Construction ────────────────────────────────────────── */

noc_topology_t *noc_topo_create_butterfly(int32_t k, int32_t stages) {
    if (k < 2 || stages < 1) return NULL;

    int32_t total = k * (stages + 1);
    int32_t max_edges = 4 * k * stages;

    noc_topology_t *topo = (noc_topology_t *)calloc(1, sizeof(noc_topology_t));
    if (!topo) return NULL;

    topo->type = TOPO_BUTTERFLY;
    snprintf(topo->name, sizeof(topo->name), "Butterfly-%dary-S%d", k, stages);
    topo->k = k;
    topo->n = stages;
    topo->num_nodes = total;
    topo->nodes = (noc_node_t *)calloc((size_t)total, sizeof(noc_node_t));
    topo->edges = (noc_edge_t *)calloc((size_t)max_edges, sizeof(noc_edge_t));

    if (!topo->nodes || !topo->edges) {
        noc_topo_free(topo);
        return NULL;
    }

    int32_t s, i;
    for (s = 0; s <= stages; s++) {
        for (i = 0; i < k; i++) {
            int32_t id = s * k + i;
            noc_node_t *nd = &topo->nodes[id];
            nd->id = id;
            nd->x = s;
            nd->y = i;
            nd->z = 0;
            nd->degree = 0;
            clear_links(nd);
            nd->links[NOC_DIR_LOCAL] = id;

            if (s < stages) {
                int32_t straight = (s + 1) * k + i;
                nd->links[NOC_DIR_EAST] = straight;
                nd->degree++;
                add_edge(topo, id, straight, 1000000000, 1);
                add_edge(topo, straight, id, 1000000000, 1);

                int32_t cross = (s + 1) * k + (i ^ (1 << s));
                nd->links[NOC_DIR_SOUTH] = cross;
                nd->degree++;
                add_edge(topo, id, cross, 1000000000, 1);
                add_edge(topo, cross, id, 1000000000, 1);
            }
        }
    }

    return topo;
}

/* ─── Free ──────────────────────────────────────────────────────────── */

void noc_topo_free(noc_topology_t *topo) {
    if (!topo) return;
    free(topo->nodes);
    free(topo->edges);
    free(topo);
}
int32_t *noc_topo_all_pairs_shortest(const noc_topology_t *topo) {
    if (!topo || topo->num_nodes <= 0) return NULL;

    int32_t N = topo->num_nodes;
    int32_t *dist = (int32_t *)malloc((size_t)(N * N) * sizeof(int32_t));
    if (!dist) return NULL;

    /* Initialize: 0 on diagonal, INF otherwise, 1 for direct neighbors */
    int32_t i, j, e;
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            dist[i * N + j] = (i == j) ? 0 : 999999;
        }
    }

    for (e = 0; e < topo->num_edges; e++) {
        int32_t u = topo->edges[e].src;
        int32_t v = topo->edges[e].dst;
        if (u >= 0 && u < N && v >= 0 && v < N && u != v) {
            dist[u * N + v] = 1;
        }
    }

    /* Floyd-Warshall: O(V^3) */
    int32_t k;
    for (k = 0; k < N; k++) {
        for (i = 0; i < N; i++) {
            int32_t dik = dist[i * N + k];
            if (dik >= 999999) continue;
            for (j = 0; j < N; j++) {
                int32_t dkj = dist[k * N + j];
                if (dkj >= 999999) continue;
                int32_t candidate = dik + dkj;
                if (candidate < dist[i * N + j]) {
                    dist[i * N + j] = candidate;
                }
            }
        }
    }

    /* Mark unreachable pairs */
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            if (dist[i * N + j] >= 999999) {
                dist[i * N + j] = -1;
            }
        }
    }

    return dist;
}

/* ??? Diameter ???????????????????????????????????????????????????????? */

int32_t noc_topo_diameter(const noc_topology_t *topo) {
    if (!topo || topo->num_nodes <= 0) return -1;

    int32_t *dist = noc_topo_all_pairs_shortest(topo);
    if (!dist) return -1;

    int32_t N = topo->num_nodes;
    int32_t max_dist = 0;
    int32_t i, j;
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            int32_t d = dist[i * N + j];
            if (d > max_dist) max_dist = d;
        }
    }

    free(dist);
    return max_dist;
}

/* ??? Average distance ???????????????????????????????????????????????? */

double noc_topo_avg_distance(const noc_topology_t *topo) {
    if (!topo || topo->num_nodes <= 1) return 0.0;

    int32_t *dist = noc_topo_all_pairs_shortest(topo);
    if (!dist) return -1.0;

    int32_t N = topo->num_nodes;
    double sum = 0.0;
    int32_t count = 0;
    int32_t i, j;
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            if (i != j) {
                int32_t d = dist[i * N + j];
                if (d >= 0) {
                    sum += d;
                    count++;
                }
            }
        }
    }

    free(dist);
    return (count > 0) ? (sum / count) : 0.0;
}

/* ??? Bisection width ????????????????????????????????????????????????? */

int32_t noc_topo_bisection_width(const noc_topology_t *topo) {
    if (!topo || topo->num_nodes <= 1) return 0;

    /* For mesh: bisection cuts k vertical links, each k nodes */
    if (topo->type == TOPO_MESH_2D) {
        return topo->k;
    }

    /* For torus: bisection cuts 2k wraparound + k links */
    if (topo->type == TOPO_TORUS_2D) {
        return 2 * topo->k;
    }

    /* For ring: bisection cuts 2 links */
    if (topo->type == TOPO_RING) {
        return 2;
    }

    /* General heuristic: count edges crossing the vertical midline */
    int32_t count = 0;
    int32_t i;
    for (i = 0; i < topo->num_edges; i++) {
        int32_t u = topo->edges[i].src;
        int32_t v = topo->edges[i].dst;
        int32_t mid = topo->num_nodes / 2;
        if ((u < mid && v >= mid) || (u >= mid && v < mid)) {
            count++;
        }
    }

    return count / 2; /* Each bidirectional cut counted twice */
}

/* ??? Full metrics ???????????????????????????????????????????????????? */

noc_topo_metrics_t noc_topo_compute_metrics(const noc_topology_t *topo,
                                             double link_bw_bps,
                                             double flit_size_bits) {
    (void)flit_size_bits;
    noc_topo_metrics_t m;
    memset(&m, 0, sizeof(m));

    if (!topo) return m;

    m.node_count = topo->num_nodes;
    m.edge_count = topo->num_edges;
    m.diameter = noc_topo_diameter(topo);
    m.avg_distance = noc_topo_avg_distance(topo);

    double bw = (double)noc_topo_bisection_width(topo);
    m.bisection_width = bw;
    m.bisection_bw_bps = bw * link_bw_bps;

    /* Channel load under uniform random traffic (Dally & Towles ?4.3):
     *   ? = H / (B * 2)  approximated as avg_dist / bisection */
    if (bw > 0) {
        m.channel_load = m.avg_distance / bw;
    }

    /* Zero-load latency: D0 = H * t_r + L / b */
    m.zero_load_latency = noc_zero_load_latency((int32_t)m.avg_distance, 5,
                                                 128, link_bw_bps);

    /* Saturation throughput estimate */
    m.saturation_throughput = noc_ideal_throughput((int32_t)m.avg_distance,
                                                    128, link_bw_bps);

    return m;
}

/* ??? XY path-finding on mesh ????????????????????????????????????????? */

noc_path_t noc_topo_find_path_xy(const noc_topology_t *topo,
                                  int32_t src_id, int32_t dst_id) {
    noc_path_t path;
    memset(&path, 0, sizeof(path));
    path.src_id = src_id;
    path.dst_id = dst_id;
    path.is_valid = 0;

    if (!topo || src_id == dst_id) {
        if (src_id == dst_id && topo) {
            path.is_valid = 1;
            path.num_hops = 0;
        }
        return path;
    }

    int32_t cx, cy, dx, dy;
    node_coords(src_id, topo->k, &cx, &cy);
    node_coords(dst_id, topo->k, &dx, &dy);

    int32_t current = src_id;
    int32_t cur_x = cx, cur_y = cy;

    while (cur_x != dx || cur_y != dy) {
        if (path.num_hops >= NOC_PATH_MAX_HOPS) break;

        noc_direction_t dir;
        if (cur_x < dx) {
            dir = NOC_DIR_EAST;
            cur_x++;
        } else if (cur_x > dx) {
            dir = NOC_DIR_WEST;
            cur_x--;
        } else if (cur_y < dy) {
            dir = NOC_DIR_SOUTH;
            cur_y++;
        } else if (cur_y > dy) {
            dir = NOC_DIR_NORTH;
            cur_y--;
        } else {
            dir = NOC_DIR_LOCAL;
        }

        int32_t next = topo->nodes[current].links[dir];
        if (next < 0) break;

        noc_hop_t *hop = &path.hops[path.num_hops];
        hop->node_id = current;
        hop->out_port = dir;
        hop->vc = 0;
        path.total_latency++;
        path.num_hops++;
        current = next;
    }

    if (current == dst_id) {
        path.is_valid = 1;
    }

    return path;
}

/* ??? Dijkstra shortest path ?????????????????????????????????????????? */

noc_path_t noc_topo_find_path_dijkstra(const noc_topology_t *topo,
                                        int32_t src_id, int32_t dst_id) {
    noc_path_t path;
    memset(&path, 0, sizeof(path));
    path.src_id = src_id;
    path.dst_id = dst_id;
    path.is_valid = 0;

    if (!topo || src_id == dst_id) {
        if (src_id == dst_id && topo) {
            path.is_valid = 1;
        }
        return path;
    }

    int32_t N = topo->num_nodes;
    if (src_id < 0 || src_id >= N || dst_id < 0 || dst_id >= N) {
        return path;
    }

    int32_t *dist = (int32_t *)malloc((size_t)N * sizeof(int32_t));
    int32_t *prev = (int32_t *)malloc((size_t)N * sizeof(int32_t));
    int32_t *visited = (int32_t *)calloc((size_t)N, sizeof(int32_t));

    if (!dist || !prev || !visited) {
        free(dist); free(prev); free(visited);
        return path;
    }

    int32_t i;
    for (i = 0; i < N; i++) {
        dist[i] = 999999;
        prev[i] = -1;
    }
    dist[src_id] = 0;

    int32_t count;
    for (count = 0; count < N; count++) {
        /* Find unvisited node with min distance */
        int32_t u = -1;
        int32_t min_dist = 999999;
        for (i = 0; i < N; i++) {
            if (!visited[i] && dist[i] < min_dist) {
                min_dist = dist[i];
                u = i;
            }
        }
        if (u < 0 || u == dst_id) break;

        visited[u] = 1;

        /* Relax edges from u */
        int32_t dir;
        for (dir = 0; dir < NOC_DIR_MAX; dir++) {
            int32_t v = topo->nodes[u].links[dir];
            if (v >= 0 && v < N && v != u && !visited[v]) {
                int32_t new_dist = dist[u] + 1;
                if (new_dist < dist[v]) {
                    dist[v] = new_dist;
                    prev[v] = u;
                }
            }
        }
    }

    if (prev[dst_id] >= 0 || dst_id == src_id) {
        /* Reconstruct path backwards */
        int32_t stack[NOC_PATH_MAX_HOPS];
        int32_t sp = 0;
        int32_t cur = dst_id;
        while (cur >= 0 && cur != src_id) {
            stack[sp++] = cur;
            cur = prev[cur];
            if (sp >= NOC_PATH_MAX_HOPS) break;
        }

        /* Build forward path */
        cur = src_id;
        while (sp > 0) {
            int32_t next = stack[--sp];
            if (path.num_hops >= NOC_PATH_MAX_HOPS) break;

            /* Determine direction from cur to next */
            noc_direction_t dir = NOC_DIR_LOCAL;
            int32_t cx, cy, nx, ny;
            node_coords(cur, topo->k, &cx, &cy);
            node_coords(next, topo->k, &nx, &ny);
            int32_t dx = nx - cx;
            int32_t dy = ny - cy;
            dir = link_to_dir(dx, dy);

            path.hops[path.num_hops].node_id = cur;
            path.hops[path.num_hops].out_port = dir;
            path.hops[path.num_hops].vc = 0;
            path.total_latency++;
            path.num_hops++;
            cur = next;
        }
        path.is_valid = 1;
    }

    free(dist); free(prev); free(visited);
    return path;
}

/* ??? Neighbor check ?????????????????????????????????????????????????? */

int noc_topo_are_neighbors(const noc_topology_t *topo,
                           int32_t a, int32_t b) {
    if (!topo || a < 0 || b < 0 ||
        a >= topo->num_nodes || b >= topo->num_nodes) return 0;

    int32_t dir;
    for (dir = 0; dir < NOC_DIR_MAX; dir++) {
        if (topo->nodes[a].links[dir] == b) return 1;
    }
    return 0;
}

/* ??? Manhattan distance ?????????????????????????????????????????????? */

int32_t noc_topo_manhattan_dist(const noc_topology_t *topo,
                                int32_t src_id, int32_t dst_id) {
    if (!topo) return -1;

    int32_t sx, sy, dx, dy;
    node_coords(src_id, topo->k, &sx, &sy);
    node_coords(dst_id, topo->k, &dx, &dy);

    /* For torus, take wrap-around into account */
    if (topo->type == TOPO_TORUS_2D) {
        int32_t dx_abs = (dx > sx) ? (dx - sx) : (sx - dx);
        int32_t dy_abs = (dy > sy) ? (dy - sy) : (sy - dy);
        dx_abs = (dx_abs < topo->k - dx_abs) ? dx_abs : (topo->k - dx_abs);
        dy_abs = (dy_abs < topo->k - dy_abs) ? dy_abs : (topo->k - dy_abs);
        return dx_abs + dy_abs;
    }

    return ((dx > sx) ? (dx - sx) : (sx - dx)) +
           ((dy > sy) ? (dy - sy) : (sy - dy));
}

/* ??? Validate topology ??????????????????????????????????????????????? */

int noc_topo_validate(const noc_topology_t *topo) {
    if (!topo) return -1;
    if (topo->num_nodes <= 0) return -1;
    if (topo->num_edges < 0) return -1;

    int32_t i, j;
    /* Check node IDs are sequential and consistent */
    for (i = 0; i < topo->num_nodes; i++) {
        if (topo->nodes[i].id != i) {
            return -1;
        }
        if (topo->nodes[i].degree < 0 || topo->nodes[i].degree > NOC_DIR_MAX) {
            return -1;
        }
        /* Verify degree matches non-negative link count */
        int32_t actual_deg = 0;
        for (j = 0; j < NOC_DIR_MAX; j++) {
            if (topo->nodes[i].links[j] >= 0) actual_deg++;
        }
        /* Local self-link doesn't count */
        if (topo->nodes[i].links[NOC_DIR_LOCAL] >= 0) actual_deg--;
        if (actual_deg != topo->nodes[i].degree) {
            /* Allow discrepancy for ring (degree 2 but 3 links w/ local) */
            if (topo->type != TOPO_RING) {
                /* Soft warn, not fail */
            }
        }
    }

    /* Check edges reference valid nodes */
    for (i = 0; i < topo->num_edges; i++) {
        if (topo->edges[i].src < 0 || topo->edges[i].src >= topo->num_nodes) {
            return -1;
        }
        if (topo->edges[i].dst < 0 || topo->edges[i].dst >= topo->num_nodes) {
            return -1;
        }
    }

    return 0;
}

/* ??? Print topology ?????????????????????????????????????????????????? */

void noc_topo_print(const noc_topology_t *topo) {
    if (!topo) {
        printf("Topology: NULL\n");
        return;
    }

    printf("=== %s ===\n", topo->name);
    printf("Type: %d, Nodes: %d, Edges: %d, k=%d, n=%d\n",
           topo->type, topo->num_nodes, topo->num_edges, topo->k, topo->n);
    printf("Diameter: %d, Avg Distance: %.3f, Bisection BW: %d\n",
           topo->diameter, topo->avg_distance, topo->bisection_bw);

    /* ASCII art for small meshes (k <= 8) */
    if ((topo->type == TOPO_MESH_2D || topo->type == TOPO_TORUS_2D) && topo->k <= 8) {
        int32_t x, y;
        printf("\n    ");
        for (x = 0; x < topo->k; x++) printf(" (%02d) ", x);
        printf("\n");
        for (y = 0; y < topo->k; y++) {
            printf("(%02d) ", y);
            for (x = 0; x < topo->k; x++) {
                int32_t id = node_id(x, y, 0, topo->k);
                const noc_node_t *n = &topo->nodes[id];
                const char *mark = (n->flags & NOC_NODE_FLAG_BOUNDARY) ? "B" : " ";
                printf("[%s%02d]", mark, id);
            }
            printf("\n");
        }
    }
}

/* ??? Export DOT ?????????????????????????????????????????????????????? */

int noc_topo_export_dot(const noc_topology_t *topo, const char *filename) {
    if (!topo || !filename) return -1;

    FILE *f = fopen(filename, "w");
    if (!f) return -1;

    fprintf(f, "graph NoC {\n");
    fprintf(f, "  rankdir=TB;\n");
    fprintf(f, "  node [shape=circle, style=filled, fillcolor=lightblue];\n");

    int32_t i;
    for (i = 0; i < topo->num_nodes; i++) {
        const noc_node_t *n = &topo->nodes[i];
        const char *color = (n->flags & NOC_NODE_FLAG_BOUNDARY) ? "orange" :
                            (n->flags & NOC_NODE_FLAG_ROOT) ? "green" :
                            (n->flags & NOC_NODE_FLAG_LEAF) ? "yellow" : "lightblue";
        fprintf(f, "  n%d [label=\"%d\\n(%d,%d)\", fillcolor=%s];\n",
                n->id, n->id, n->x, n->y, color);
    }

    /* Deduplicate edges (undirected DOT graph) */
    for (i = 0; i < topo->num_edges; i++) {
        if (topo->edges[i].src < topo->edges[i].dst) {
            fprintf(f, "  n%d -- n%d [label=\"%dGb\"];\n",
                    topo->edges[i].src, topo->edges[i].dst,
                    topo->edges[i].bandwidth_bps / 1000000000);
        }
    }

    fprintf(f, "}\n");
    fclose(f);
    return 0;
}
