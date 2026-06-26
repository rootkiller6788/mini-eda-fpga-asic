#ifndef NOC_TOPOLOGY_H
#define NOC_TOPOLOGY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NOC_PORT_SELF  = 0,
    NOC_PORT_NORTH = 1,
    NOC_PORT_SOUTH = 2,
    NOC_PORT_EAST  = 3,
    NOC_PORT_WEST  = 4,
    NOC_PORT_MAX   = 5
};

typedef enum {
    NOC_TOPO_MESH  = 0,
    NOC_TOPO_TORUS = 1,
    NOC_TOPO_RING  = 2,
    NOC_TOPO_FAT_TREE = 3,
    NOC_TOPO_FLATTENED_BFLY = 4,
    NOC_TOPO_COUNT
} noc_topology_type_t;

typedef struct {
    int x;
    int y;
} noc_coord_t;

typedef struct {
    int dx;
    int dy;
} noc_routing_offset_t;

typedef enum {
    NOC_DIR_EAST  = 0,
    NOC_DIR_WEST  = 1,
    NOC_DIR_NORTH = 2,
    NOC_DIR_SOUTH = 3,
    NOC_DIR_LOCAL = 4,
    NOC_DIR_NONE  = 5
} noc_direction_t;

typedef struct {
    noc_topology_type_t type;
    int width;
    int height;
    int num_routers;
    bool wrap_around;
} noc_topology_t;

void noc_topology_init(noc_topology_t *topo, noc_topology_type_t type, int w, int h);
void noc_topology_destroy(noc_topology_t *topo);

bool noc_is_boundary(const noc_topology_t *topo, noc_coord_t node, noc_direction_t dir);
int  noc_neighbor_port(const noc_topology_t *topo, noc_coord_t from, noc_direction_t dir, noc_coord_t *to);
int  noc_node_id(const noc_topology_t *topo, noc_coord_t coord);
bool noc_coord_from_id(const noc_topology_t *topo, int id, noc_coord_t *out);

noc_direction_t noc_xy_route(const noc_topology_t *topo, noc_coord_t src, noc_coord_t dst);
void noc_deadlock_escape_route(const noc_topology_t *topo, noc_coord_t src, noc_coord_t dst,
                                noc_direction_t *primary, noc_direction_t *escape);

bool noc_route_is_valid(const noc_topology_t *topo, noc_coord_t src, noc_coord_t dst,
                         noc_direction_t dir);

int noc_hop_count(const noc_topology_t *topo, noc_coord_t src, noc_coord_t dst);
int noc_manhattan_distance(noc_coord_t a, noc_coord_t b);

void noc_topology_dump(const noc_topology_t *topo);
const char *noc_topology_name(noc_topology_type_t type);
const char *noc_direction_name(noc_direction_t dir);
const char *noc_port_name(int port);

#ifdef __cplusplus
}
#endif

#endif
