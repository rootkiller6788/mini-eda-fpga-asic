#include "noc_topology.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void noc_topology_init(noc_topology_t *topo, noc_topology_type_t type, int w, int h) {
    topo->type   = type;
    topo->width  = w;
    topo->height = h;
    topo->num_routers = w * h;
    topo->wrap_around  = (type == NOC_TOPO_TORUS);
}

void noc_topology_destroy(noc_topology_t *topo) {
    memset(topo, 0, sizeof(*topo));
}

bool noc_is_boundary(const noc_topology_t *topo, noc_coord_t node, noc_direction_t dir) {
    if (topo->type == NOC_TOPO_TORUS || topo->type == NOC_TOPO_RING) return false;
    switch (dir) {
        case NOC_DIR_NORTH: return node.y == 0;
        case NOC_DIR_SOUTH: return node.y == topo->height - 1;
        case NOC_DIR_EAST:  return node.x == topo->width - 1;
        case NOC_DIR_WEST:  return node.x == 0;
        default:            return true;
    }
}

int noc_neighbor_port(const noc_topology_t *topo, noc_coord_t from, noc_direction_t dir, noc_coord_t *to) {
    int w = topo->width, h = topo->height;
    switch (dir) {
        case NOC_DIR_NORTH:
            to->x = from.x; to->y = from.y - 1;
            if (to->y < 0) to->y = topo->wrap_around ? h - 1 : -1;
            break;
        case NOC_DIR_SOUTH:
            to->x = from.x; to->y = from.y + 1;
            if (to->y >= h) to->y = topo->wrap_around ? 0 : -1;
            break;
        case NOC_DIR_EAST:
            to->x = from.x + 1; to->y = from.y;
            if (to->x >= w) to->x = topo->wrap_around ? 0 : -1;
            break;
        case NOC_DIR_WEST:
            to->x = from.x - 1; to->y = from.y;
            if (to->x < 0) to->x = topo->wrap_around ? w - 1 : -1;
            break;
        case NOC_DIR_LOCAL:
            to->x = from.x; to->y = from.y; break;
        default:
            to->x = -1; to->y = -1; return -1;
    }
    if (to->x < 0 || to->y < 0 || to->x >= w || to->y >= h) return -1;
    switch (dir) {
        case NOC_DIR_NORTH: return NOC_PORT_NORTH;
        case NOC_DIR_SOUTH: return NOC_PORT_SOUTH;
        case NOC_DIR_EAST:  return NOC_PORT_EAST;
        case NOC_DIR_WEST:  return NOC_PORT_WEST;
        case NOC_DIR_LOCAL: return NOC_PORT_SELF;
        default:            return -1;
    }
}

int noc_node_id(const noc_topology_t *topo, noc_coord_t coord) {
    return coord.y * topo->width + coord.x;
}

bool noc_coord_from_id(const noc_topology_t *topo, int id, noc_coord_t *out) {
    if (id < 0 || id >= topo->num_routers) return false;
    out->x = id % topo->width;
    out->y = id / topo->width;
    return true;
}

noc_direction_t noc_xy_route(const noc_topology_t *topo, noc_coord_t src, noc_coord_t dst) {
    (void)topo;
    if (dst.x > src.x) return NOC_DIR_EAST;
    if (dst.x < src.x) return NOC_DIR_WEST;
    if (dst.y > src.y) return NOC_DIR_SOUTH;
    if (dst.y < src.y) return NOC_DIR_NORTH;
    return NOC_DIR_LOCAL;
}

void noc_deadlock_escape_route(const noc_topology_t *topo, noc_coord_t src, noc_coord_t dst,
                                noc_direction_t *primary, noc_direction_t *escape) {
    *primary = noc_xy_route(topo, src, dst);
    *escape  = NOC_DIR_NONE;
    if (*primary == NOC_DIR_LOCAL) return;

    if (dst.y > src.y)      *escape = NOC_DIR_SOUTH;
    else if (dst.y < src.y) *escape = NOC_DIR_NORTH;
    else if (dst.x > src.x) *escape = NOC_DIR_EAST;
    else if (dst.x < src.x) *escape = NOC_DIR_WEST;
    else                    *escape = NOC_DIR_LOCAL;
}

bool noc_route_is_valid(const noc_topology_t *topo, noc_coord_t src, noc_coord_t dst,
                         noc_direction_t dir) {
    noc_coord_t next;
    int port = noc_neighbor_port(topo, src, dir, &next);
    if (port < 0) return false;
    return (noc_manhattan_distance(next, dst) <= noc_manhattan_distance(src, dst));
}

int noc_hop_count(const noc_topology_t *topo, noc_coord_t src, noc_coord_t dst) {
    (void)topo;
    return noc_manhattan_distance(src, dst);
}

int noc_manhattan_distance(noc_coord_t a, noc_coord_t b) {
    int dx = (a.x > b.x) ? (a.x - b.x) : (b.x - a.x);
    int dy = (a.y > b.y) ? (a.y - b.y) : (b.y - a.y);
    return dx + dy;
}

void noc_topology_dump(const noc_topology_t *topo) {
    printf("Topology: %s (%dx%d), %d routers%s\n",
        noc_topology_name(topo->type), topo->width, topo->height,
        topo->num_routers, topo->wrap_around ? " [WRAP]" : "");
}

const char *noc_topology_name(noc_topology_type_t type) {
    static const char *names[] = { "Mesh", "Torus", "Ring", "Fat-Tree", "Flattened-Bfly" };
    return (type < NOC_TOPO_COUNT) ? names[type] : "Unknown";
}

const char *noc_direction_name(noc_direction_t dir) {
    static const char *names[] = { "E", "W", "N", "S", "L", "-" };
    return (dir <= NOC_DIR_NONE) ? names[dir] : "?";
}

const char *noc_port_name(int port) {
    static const char *names[] = { "Self", "North", "South", "East", "West" };
    return (port >= 0 && port < NOC_PORT_MAX) ? names[port] : "?";
}
