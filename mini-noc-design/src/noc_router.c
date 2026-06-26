#include "noc_router.h"
#include <stdio.h>
#include <string.h>

void router_init(Router *r, int id, int num_ports) {
    memset(r, 0, sizeof(*r)); r->id = id; r->port_count = num_ports;
    for (int p = 0; p < num_ports; p++) {
        for (int v = 0; v < MAX_VC; v++) {
            r->in_ports[p].vcs[v].head = 0; r->in_ports[p].vcs[v].tail = 0;
            r->in_ports[p].vcs[v].count = 0; r->in_ports[p].credits[v] = VC_BUFFER_DEPTH;
        }
        r->in_ports[p].active_vc = 0;
    }
}

bool router_receive(Router *r, int port, int vc, uint32_t flit) {
    if (port >= r->port_count || vc >= MAX_VC) return false;
    VcBuffer *buf = &r->in_ports[port].vcs[vc];
    if (buf->count >= VC_BUFFER_DEPTH) return false;
    buf->flits[buf->tail] = flit; buf->tail = (buf->tail + 1) % VC_BUFFER_DEPTH; buf->count++;
    return true;
}

bool router_route(Router *r, int in_port, int vc, int dst_x, int dst_y, int *out_port) {
    (void)in_port; (void)vc;
    /* Simple XY routing */
    int cur_x = (r->id >> 8) & 0xFF, cur_y = r->id & 0xFF;
    if (dst_x > cur_x) { *out_port = 2; return true; } /* East */
    if (dst_x < cur_x) { *out_port = 3; return true; } /* West */
    if (dst_y > cur_y) { *out_port = 1; return true; } /* South */
    if (dst_y < cur_y) { *out_port = 0; return true; } /* North */
    *out_port = 4; return true; /* Local/eject */
}

int router_vc_alloc(Router *r, int in_port, int out_port) {
    (void)out_port;
    for (int v = 0; v < MAX_VC; v++) if (r->in_ports[in_port].credits[v] > 0) return v;
    return -1;
}

bool router_switch_traverse(Router *r, SwitchAlloc *sa) {
    if (r->busy) return false;
    sa->granted = true; r->busy = true;
    return true;
}

void router_credit_return(Router *r, int port, int vc) {
    if (port < r->port_count && vc < MAX_VC && r->in_ports[port].credits[vc] < VC_BUFFER_DEPTH)
        r->in_ports[port].credits[vc]++;
}

void router_print(Router *r) {
    printf("  Router %d: %d ports\n", r->id, r->port_count);
    for (int p = 0; p < r->port_count; p++) {
        int total = 0; for (int v = 0; v < MAX_VC; v++) total += r->in_ports[p].vcs[v].count;
        printf("    Port %d: %d buffered flits\n", p, total);
    }
}
