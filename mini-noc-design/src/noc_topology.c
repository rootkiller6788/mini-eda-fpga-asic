#include "noc_topology.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void topo_init(Topology *t) {
    memset(t, 0, sizeof(*t));
    for (int i = 0; i < MAX_NODES; i++) t->degree[i] = 0;
}

const char *topo_type_name(TopoType type) {
    switch (type) { case TOPO_MESH: return "Mesh"; case TOPO_TORUS: return "Torus"; case TOPO_FAT_TREE: return "FatTree"; case TOPO_FLATTENED_BUTTERFLY: return "FlattenedButterfly"; case TOPO_RING: return "Ring"; case TOPO_CROSSBAR: return "Crossbar"; default: return "?"; }
}

static int node_id(int x, int y, int dim_x) { return y * dim_x + x; }
static void add_link(Topology *t, int src, int dst) {
    if (t->link_count < MAX_LINKS) {
        t->links[t->link_count].id = t->link_count;
        t->links[t->link_count].src = src; t->links[t->link_count].dst = dst;
        t->links[t->link_count].delay = 1; t->links[t->link_count].bw = 1.0;
        t->link_count++;
    }
    if (t->degree[src] < 8) t->adj[src][t->degree[src]++] = dst;
}

int topo_create(Topology *t, TopoType type, int dim_x, int dim_y) {
    topo_init(t); t->type = type; t->dim_x = dim_x; t->dim_y = dim_y;
    switch (type) {
        case TOPO_MESH:
        case TOPO_TORUS: {
            for (int y = 0; y < dim_y; y++) {
                for (int x = 0; x < dim_x; x++) {
                    int id = node_id(x, y, dim_x);
                    if (id >= MAX_NODES) return -1;
                    t->nodes[t->node_count].id = id;
                    t->nodes[t->node_count].x = x;
                    t->nodes[t->node_count].y = y;
                    t->node_count++;
                    /* East */ if (x + 1 < dim_x) add_link(t, id, node_id(x+1, y, dim_x));
                    else if (type == TOPO_TORUS) add_link(t, id, node_id(0, y, dim_x));
                    /* West */ if (x > 0) add_link(t, id, node_id(x-1, y, dim_x));
                    else if (type == TOPO_TORUS) add_link(t, id, node_id(dim_x-1, y, dim_x));
                    /* South */ if (y + 1 < dim_y) add_link(t, id, node_id(x, y+1, dim_x));
                    else if (type == TOPO_TORUS) add_link(t, id, node_id(x, 0, dim_x));
                    /* North */ if (y > 0) add_link(t, id, node_id(x, y-1, dim_x));
                    else if (type == TOPO_TORUS) add_link(t, id, node_id(x, dim_y-1, dim_x));
                }
            }
            break;
        }
        case TOPO_RING:
            for (int i = 0; i < dim_x && t->node_count < MAX_NODES; i++) {
                t->nodes[t->node_count].id = i; t->nodes[t->node_count].x = i; t->nodes[t->node_count].y = 0; t->node_count++;
            }
            for (int i = 0; i < t->node_count; i++) { add_link(t, i, (i+1) % t->node_count); add_link(t, i, (i-1+(int)t->node_count) % t->node_count); }
            break;
        case TOPO_CROSSBAR:
            for (int i = 0; i < dim_x; i++) { t->nodes[t->node_count].id = i; t->nodes[t->node_count].x = i; t->nodes[t->node_count].y = 0; t->node_count++; }
            for (int i = 0; i < t->node_count; i++) for (int j = 0; j < t->node_count; j++) if (i != j) add_link(t, i, j);
            break;
        case TOPO_FAT_TREE:
        case TOPO_FLATTENED_BUTTERFLY:
            /* Simplified: treat as mesh for now */ t->node_count = 0; t->link_count = 0;
            break;
    }
    return t->node_count;
}

int topo_connectivity(Topology *t) {
    int min_cut = 9999;
    for (int i = 0; i < t->node_count; i++) { if (t->degree[i] < min_cut) min_cut = t->degree[i]; }
    return min_cut;
}

/* BFS-based diameter and shortest path */
int topo_diameter(Topology *t) {
    int max_dist = 0;
    for (int src = 0; src < t->node_count && src < 32; src++) {
        int dist[MAX_NODES]; int q[MAX_NODES]; int front = 0, back = 0;
        for (int i = 0; i < t->node_count; i++) dist[i] = -1;
        dist[src] = 0; q[back++] = src;
        while (front < back) {
            int u = q[front++];
            for (int k = 0; k < t->degree[u]; k++) {
                int v = t->adj[u][k];
                if (dist[v] == -1) { dist[v] = dist[u] + 1; q[back++] = v; }
            }
        }
        for (int i = 0; i < t->node_count; i++) if (dist[i] > max_dist) max_dist = dist[i];
    }
    return max_dist;
}

int topo_shortest_path(Topology *t, int src, int dst, int *path, int max_hops) {
    if (src == dst) { path[0] = src; return 1; }
    int dist[MAX_NODES], prev[MAX_NODES], q[MAX_NODES], f=0,b=0;
    for (int i = 0; i < t->node_count; i++) { dist[i] = -1; prev[i] = -1; }
    dist[src] = 0; q[b++] = src;
    while (f < b) { int u = q[f++]; for (int k = 0; k < t->degree[u]; k++) { int v = t->adj[u][k]; if (dist[v] == -1) { dist[v]=dist[u]+1; prev[v]=u; q[b++]=v; } } }
    if (dist[dst] == -1 || dist[dst] >= max_hops) return -1;
    int hops = 0, cur = dst;
    while (cur != -1 && hops < max_hops) { path[hops++] = cur; cur = prev[cur]; }
    for (int i = 0; i < hops/2; i++) { int tmp = path[i]; path[i] = path[hops-1-i]; path[hops-1-i] = tmp; }
    return hops;
}

void topo_print(Topology *t) {
    printf("=== %s Topology ===\n", topo_type_name(t->type));
    printf("  Nodes: %d, Links: %d\n", t->node_count, t->link_count);
    printf("  Dimension: %dx%d, Diameter: %d, Bisection: %d\n", t->dim_x, t->dim_y, topo_diameter(t), topo_connectivity(t));
}
