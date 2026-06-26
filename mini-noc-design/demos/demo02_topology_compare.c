/**
 * demo02_topology_compare.c ? L7 Application: Topology Comparison
 *
 * Compares mesh, torus, ring, tree, and butterfly topologies
 * across key metrics: diameter, avg distance, bisection BW,
 * and cost.
 */

#include <stdio.h>
#include <stdlib.h>
#include "noc_topology.h"
#include "noc_routing.h"

static void print_comparison(noc_topology_t *topo) {
    if (!topo) return;

    int32_t diam = noc_topo_diameter(topo);
    double avg_dist = noc_topo_avg_distance(topo);
    int32_t bisect = noc_topo_bisection_width(topo);
    int32_t cost = topo->num_edges; /* Number of links = wiring cost */

    printf("  %-20s  N=%3d  D=%2d  ?=%.2f  B=%2d  Edges=%3d\n",
           topo->name, topo->num_nodes, diam, avg_dist, bisect, cost);
}

int main(void) {
    printf("????????????????????????????????????????????\n");
    printf("?  Demo 02: NoC Topology Comparison       ?\n");
    printf("????????????????????????????????????????????\n\n");

    printf("??? Topology Metrics (N?16 nodes) ???\n\n");
    printf("  %-20s  %-5s  %-3s  %-5s  %-3s  %-5s\n",
           "Topology", "N", "D", "?", "B", "Cost");

    /* 4x4 Mesh */
    noc_topology_t *mesh = noc_topo_create_mesh_2d(4);
    print_comparison(mesh);

    /* 4x4 Torus */
    noc_topology_t *torus = noc_topo_create_torus_2d(4);
    print_comparison(torus);

    /* 16-node Ring */
    noc_topology_t *ring = noc_topo_create_ring(16);
    print_comparison(ring);

    /* 2-ary 4-level Tree (=1+2+4+8=15 nodes) */
    noc_topology_t *tree = noc_topo_create_tree(2, 4);
    print_comparison(tree);

    /* 4-ary 2-stage Butterfly */
    noc_topology_t *bf = noc_topo_create_butterfly(4, 2);
    print_comparison(bf);

    printf("\n??? Analysis ???\n\n");
    printf("  ? Torus halves diameter vs mesh (4 vs 6) for same node count\n");
    printf("  ? Torus doubles bisection bandwidth (8 vs 4)\n");
    printf("  ? Ring has worst diameter (8) but lowest cost (32 edges)\n");
    printf("  ? Butterfly provides O(log N) diameter for O(N log N) cost\n");
    printf("  ? Fat-tree scales well for HPC applications (InfiniBand)\n");

    /* ??? Special: mesh size sweep ??????????????????????????????? */
    printf("\n??? Mesh Size Scaling ???\n\n");
    printf("  %-10s  %-8s  %-5s  %-8s  %-5s\n",
           "Size", "Nodes", "D", "?", "B");

    int32_t sizes[] = {2, 4, 8, 16};
    int32_t si;
    for (si = 0; si < 4; si++) {
        int32_t ks = sizes[si];
        noc_topology_t *m = noc_topo_create_mesh_2d(ks);
        if (!m) continue;
        printf("  %2dx%2d      %-8d  %-5d  %-8.2f  %-5d\n",
               ks, ks, m->num_nodes,
               noc_topo_diameter(m), noc_topo_avg_distance(m),
               noc_topo_bisection_width(m));
        noc_topo_free(m);
    }

    noc_topo_free(mesh);
    noc_topo_free(torus);
    noc_topo_free(ring);
    noc_topo_free(tree);
    noc_topo_free(bf);

    return 0;
}
