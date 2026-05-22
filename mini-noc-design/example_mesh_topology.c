#include "noc_topology.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    noc_topology_t topo;
    int w = 4, h = 4;

    printf("=== NoC Topology Example ===\n\n");

    noc_topology_init(&topo, NOC_TOPO_MESH, w, h);
    noc_topology_dump(&topo);

    printf("\nXY Routing demo (4x4 Mesh):\n");
    for (int sy = 0; sy < h; sy++) {
        for (int sx = 0; sx < w; sx++) {
            noc_coord_t src = {sx, sy};
            noc_coord_t dst = {w - 1 - sx, h - 1 - sy};
            noc_direction_t dir = noc_xy_route(&topo, src, dst);
            int hops = noc_hop_count(&topo, src, dst);
            printf("  (%d,%d)->(%d,%d) dir=%s hops=%d\n",
                src.x, src.y, dst.x, dst.y, noc_direction_name(dir), hops);
        }
    }

    printf("\nBoundary check:\n");
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            noc_coord_t n = {x, y};
            bool n_bound = noc_is_boundary(&topo, n, NOC_DIR_NORTH);
            bool s_bound = noc_is_boundary(&topo, n, NOC_DIR_SOUTH);
            bool e_bound = noc_is_boundary(&topo, n, NOC_DIR_EAST);
            bool w_bound = noc_is_boundary(&topo, n, NOC_DIR_WEST);
            printf("  (%d,%d): N=%d S=%d E=%d W=%d\n", x, y, n_bound, s_bound, e_bound, w_bound);
        }
    }

    printf("\nNode ID mapping:\n");
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            noc_coord_t c = {x, y};
            int id = noc_node_id(&topo, c);
            noc_coord_t back;
            noc_coord_from_id(&topo, id, &back);
            printf("  id=%d coord=(%d,%d) back=(%d,%d)\n", id, c.x, c.y, back.x, back.y);
        }
    }

    printf("\nDeadlock escape routes:\n");
    for (int sy = 0; sy < h; sy += 2) {
        noc_coord_t src = {0, sy};
        noc_coord_t dst = {w - 1, h - 1 - sy};
        noc_direction_t primary, escape;
        noc_deadlock_escape_route(&topo, src, dst, &primary, &escape);
        printf("  (%d,%d)->(%d,%d): primary=%s escape=%s\n",
            src.x, src.y, dst.x, dst.y,
            noc_direction_name(primary), noc_direction_name(escape));
    }

    noc_topology_destroy(&topo);
    printf("\nDone.\n");
    return 0;
}
