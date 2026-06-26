#include "noc_verilog_rtl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void noc_router_init(noc_router_t *router, int id, noc_coord_t loc) {
    int i;
    memset(router, 0, sizeof(*router));
    router->router_id = id;
    router->location  = loc;
    for (i = 0; i < NOC_PORT_MAX; i++) {
        noc_router_input_buffer_init(&router->input_buffers[i], i);
        router->current_stage[i] = NOC_PIPE_RC;
    }
    noc_vc_allocator_init(&router->vc_alloc);
    noc_switch_allocator_init(&router->sw_alloc);
    noc_crossbar_init(&router->crossbar);
}

void noc_router_reset(noc_router_t *router) {
    noc_coord_t loc = router->location;
    int id = router->router_id;
    noc_router_init(router, id, loc);
}

void noc_router_input_buffer_init(noc_router_input_buffer_t *buf, int vc) {
    memset(buf, 0, sizeof(*buf));
    buf->vc_id    = vc;
    buf->is_empty = true;
    buf->credits  = NOC_BUFFER_DEPTH;
}

bool noc_router_input_buffer_push(noc_router_input_buffer_t *buf, const noc_flit_t *flit) {
    if (buf->count >= NOC_BUFFER_DEPTH || buf->credits <= 0) return false;
    buf->flits[buf->tail] = *flit;
    buf->tail = (buf->tail + 1) % NOC_BUFFER_DEPTH;
    buf->count++;
    buf->is_empty = false;
    buf->credits--;
    return true;
}

bool noc_router_input_buffer_pop(noc_router_input_buffer_t *buf, noc_flit_t *out) {
    if (buf->count == 0) return false;
    *out = buf->flits[buf->head];
    buf->head = (buf->head + 1) % NOC_BUFFER_DEPTH;
    buf->count--;
    buf->is_empty = (buf->count == 0);
    buf->credits++;
    return true;
}

void noc_vc_allocator_init(noc_vc_allocator_t *alloc) {
    memset(alloc, 0, sizeof(*alloc));
}

int noc_vc_allocator_request(noc_vc_allocator_t *alloc, int input_port, int output_port) {
    int vc;
    (void)output_port;
    for (vc = 0; vc < VC_MAX; vc++) {
        if (!alloc->vc_state[vc]) {
            alloc->vc_state[vc] = true;
            alloc->vc_grants[vc] = input_port;
            alloc->vc_assignment[input_port][output_port] = vc;
            return vc;
        }
    }
    return -1;
}

bool noc_vc_allocator_grant(noc_vc_allocator_t *alloc, int vc_id, int output_port) {
    (void)output_port;
    return alloc->vc_state[vc_id];
}

void noc_vc_allocator_release(noc_vc_allocator_t *alloc, int vc_id) {
    if (vc_id >= 0 && vc_id < VC_MAX) alloc->vc_state[vc_id] = false;
}

void noc_switch_allocator_init(noc_switch_allocator_t *alloc) {
    int i, j;
    memset(alloc, 0, sizeof(*alloc));
    for (i = 0; i < NOC_PORT_MAX; i++) alloc->input_port_map[i] = -1;
    for (i = 0; i < NOC_PORT_MAX; i++)
        for (j = 0; j < NOC_PORT_MAX; j++)
            alloc->crossbar_config[i][j] = false;
}

int noc_switch_request(noc_switch_allocator_t *alloc, int input_port, int output_port) {
    alloc->port_requests[input_port][output_port]++;
    return alloc->port_requests[input_port][output_port];
}

bool noc_switch_grant(noc_switch_allocator_t *alloc, int output_port, int *winner_input) {
    int i;
    for (i = 0; i < NOC_PORT_MAX; i++) {
        if (alloc->port_requests[i][output_port] > 0) {
            alloc->port_requests[i][output_port]--;
            *winner_input = i;
            return true;
        }
    }
    return false;
}

void noc_crossbar_init(noc_crossbar_t *xbar) {
    memset(xbar, 0, sizeof(*xbar));
}

void noc_crossbar_configure(noc_crossbar_t *xbar, int input_port, int output_port) {
    if (input_port >= 0 && input_port < NOC_PORT_MAX &&
        output_port >= 0 && output_port < NOC_PORT_MAX) {
        xbar->routing_table[input_port][output_port] = 1;
    }
}

void noc_crossbar_transfer(noc_crossbar_t *xbar, const noc_flit_t *flits_in[NOC_PORT_MAX],
                            noc_flit_t *flits_out[NOC_PORT_MAX]) {
    int i, j;
    for (i = 0; i < NOC_PORT_MAX; i++) {
        for (j = 0; j < NOC_PORT_MAX; j++) {
            if (xbar->routing_table[i][j] && flits_in[i]) {
                flits_out[j] = (noc_flit_t *)flits_in[i];
            }
        }
    }
}

int noc_lookahead_route(const noc_router_t *router, const noc_topology_t *topo,
                         noc_coord_t dst, int input_port) {
    noc_coord_t src = router->location;
    (void)input_port;
    if (dst.x > src.x) return NOC_PORT_EAST;
    if (dst.x < src.x) return NOC_PORT_WEST;
    if (dst.y > src.y) return NOC_PORT_SOUTH;
    if (dst.y < src.y) return NOC_PORT_NORTH;
    (void)topo;
    return NOC_PORT_SELF;
}

bool noc_speculative_vc_allocation(const noc_router_t *router, int input_port) {
    return router->speculative_vc[input_port];
}

void noc_router_cycle(noc_router_t *router, const noc_topology_t *topo,
                       const noc_flit_t *flits_in[NOC_PORT_MAX],
                       noc_flit_t *flits_out[NOC_PORT_MAX]) {
    int port;
    router->cycle_count++;
    (void)topo;

    for (port = 0; port < NOC_PORT_MAX; port++) {
        if (flits_in[port]) {
            noc_router_input_buffer_push(&router->input_buffers[port], flits_in[port]);
        }
    }

    for (port = 0; port < NOC_PORT_MAX; port++) {
        noc_flit_t flit;
        if (noc_router_input_buffer_pop(&router->input_buffers[port], &flit)) {
            int out_port = port;
            if (out_port >= 0 && out_port < NOC_PORT_MAX) {
                flits_out[out_port] = (noc_flit_t *)&flit;
                router->flits_processed++;
            }
        }
    }
}

int noc_two_stage_pipeline(noc_router_t *router) {
    int latency = 2;
    router->cycle_count += latency;
    return latency;
}

int noc_three_stage_pipeline(noc_router_t *router) {
    int latency = 3;
    router->cycle_count += latency;
    return latency;
}

int noc_five_stage_pipeline(noc_router_t *router) {
    int latency = 5;
    router->cycle_count += latency;
    return latency;
}

void noc_router_dump(const noc_router_t *router) {
    printf("Router[%d] @(%d,%d) cycles=%d flits=%d stalled=%d\n",
        router->router_id, router->location.x, router->location.y,
        router->cycle_count, router->flits_processed, router->flits_stalled);
}

void noc_pipeline_dump(const noc_router_t *router) {
    int i;
    printf("Router[%d] pipeline stages:\n", router->router_id);
    for (i = 0; i < NOC_PORT_MAX; i++) {
        printf("  Port[%d]=%s LA=%d Spec=%d\n",
            i, noc_pipeline_stage_name(router->current_stage[i]),
            router->lookahead_routes[i], router->speculative_vc[i]);
    }
}

const char *noc_pipeline_stage_name(noc_pipeline_stage_t stage) {
    static const char *names[] = { "RC", "VA", "SA", "ST", "LT" };
    return (stage < NOC_PIPE_COUNT) ? names[stage] : "?";
}
