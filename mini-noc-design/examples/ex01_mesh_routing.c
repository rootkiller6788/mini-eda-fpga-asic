/**
 * ex01_mesh_routing.c ? L6 Canonical: 2D Mesh XY Routing Demo
 *
 * Demonstrates deadlock-free XY routing on a 4?4 mesh NoC.
 * Shows path computation, hop-by-hop routing, and topology verification.
 *
 * This is the canonical "hello world" of NoC routing:
 * verify that XY-routing on a mesh never deadlocks
 * and reaches every destination in minimal steps.
 */

#include <stdio.h>
#include <stdlib.h>
#include "noc_topology.h"
#include "noc_routing.h"

int main(void) {
    printf("???????????????????????????????????????????\n");
    printf("  Example 01: 2D Mesh XY Routing\n");
    printf("???????????????????????????????????????????\n\n");

    int32_t k = 4;
    noc_topology_t *mesh = noc_topo_create_mesh_2d(k);
    if (!mesh) {
        printf("ERROR: Failed to create mesh\n");
        return 1;
    }

    noc_topo_print(mesh);

    printf("\n??? X?Y Routing Paths ???\n\n");

    /* Show XY-routing paths for several src?dst pairs */
    int32_t test_pairs[][2] = {
        {0, 15},   /* Corner to corner */
        {0, 5},    /* Same row */
        {3, 12},   /* Same column */
        {5, 10},   /* Center to edge */
        {0, 0},    /* Self */
    };
    int32_t num_pairs = 5;

    int32_t i;
    for (i = 0; i < num_pairs; i++) {
        int32_t src = test_pairs[i][0];
        int32_t dst = test_pairs[i][1];

        noc_path_t path = noc_topo_find_path_xy(mesh, src, dst);
        int32_t manhattan = noc_topo_manhattan_dist(mesh, src, dst);

        printf("  Path [%2d] ? [%2d]: ", src, dst);
        if (path.is_valid) {
            printf("%d hops (Manhattan: %d)", path.num_hops, manhattan);
            if (path.num_hops == manhattan) {
                printf(" ? MINIMAL");
            } else {
                printf(" (non-minimal ? torus would help)");
            }
            printf("\n    Route: ");
            int32_t h;
            const char *dir_names[] = {"L","E","W","S","N","U","D","?"};
            for (h = 0; h < path.num_hops && h < 20; h++) {
                printf("%s", dir_names[path.hops[h].out_port]);
                if (h < path.num_hops - 1) printf("?");
            }
            printf("\n");
        } else {
            printf("UNREACHABLE\n");
        }
    }

    printf("\n??? Topology Metrics ???\n");
    printf("  Diameter:        %d\n", noc_topo_diameter(mesh));
    printf("  Avg Distance:    %.3f\n", noc_topo_avg_distance(mesh));
    printf("  Bisection Width: %d\n", noc_topo_bisection_width(mesh));
    printf("  Manhattan (0?15): %d\n", noc_topo_manhattan_dist(mesh, 0, 15));

    printf("\n??? Deadlock-Freedom Check ???\n");
    noc_channel_dep_graph_t cdg = noc_cdg_build(ROUTE_XY, k);
    int has_cycle = noc_cdg_has_cycle(&cdg);
    printf("  XY Routing CDG has cycle? %s\n", has_cycle ? "YES ?" : "NO ?");
    printf("  CDG edges: %d\n", cdg.num_edges);

    /* Export DOT for visualization */
    noc_topo_export_dot(mesh, "build/ex01_mesh.dot");
    printf("\n  Topology DOT exported to build/ex01_mesh.dot\n");

    noc_topo_free(mesh);
    return 0;
}
