#ifndef NOC_TOPOLOGY_H
#define NOC_TOPOLOGY_H
#include <stdbool.h>

#define MAX_NODES 128
#define MAX_LINKS 512

typedef enum { TOPO_MESH, TOPO_TORUS, TOPO_FAT_TREE, TOPO_FLATTENED_BUTTERFLY, TOPO_RING, TOPO_CROSSBAR } TopoType;

typedef struct { int id; int x, y; int router_id; } TopoNode;

typedef struct { int id; int src, dst; int delay; double bw; } TopoLink;

typedef struct {
    TopoType type;
    int dim_x, dim_y, radix; /* dimensions */
    TopoNode nodes[MAX_NODES]; int node_count;
    TopoLink links[MAX_LINKS]; int link_count;
    int adj[MAX_NODES][8]; /* adjacency list (up to 8 neighbors per node) */
    int degree[MAX_NODES];
} Topology;

void topo_init(Topology *t);
int  topo_create(Topology *t, TopoType type, int dim_x, int dim_y);
int  topo_connectivity(Topology *t); /* returns bisection bandwidth */
int  topo_diameter(Topology *t);     /* returns maximum hop count */
int  topo_shortest_path(Topology *t, int src, int dst, int *path, int max_hops);
void topo_print(Topology *t);
const char *topo_type_name(TopoType type);
#endif
