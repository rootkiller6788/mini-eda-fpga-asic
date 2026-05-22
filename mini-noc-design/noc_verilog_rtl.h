#ifndef NOC_VERILOG_RTL_H
#define NOC_VERILOG_RTL_H

#include <stdint.h>
#include <stdbool.h>
#include "noc_topology.h"
#include "vc_wormhole.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NOC_PIPE_RC = 0,
    NOC_PIPE_VA = 1,
    NOC_PIPE_SA = 2,
    NOC_PIPE_ST = 3,
    NOC_PIPE_LT = 4,
    NOC_PIPE_COUNT
} noc_pipeline_stage_t;

typedef struct {
    noc_flit_t flits[NOC_BUFFER_DEPTH];
    int        head;
    int        tail;
    int        count;
    int        vc_id;
    bool       is_empty;
    int        credits;
} noc_router_input_buffer_t;

typedef struct {
    int   vc_requests[VC_MAX][NOC_PORT_MAX];
    int   vc_grants[VC_MAX];
    bool  vc_state[VC_MAX];
    int   vc_assignment[NOC_PORT_MAX][VC_MAX];
    int   esc_vc_usage;
} noc_vc_allocator_t;

typedef struct {
    int  port_requests[NOC_PORT_MAX][NOC_PORT_MAX];
    int  port_grants[NOC_PORT_MAX];
    int  input_port_map[NOC_PORT_MAX];
    bool crossbar_config[NOC_PORT_MAX][NOC_PORT_MAX];
} noc_switch_allocator_t;

typedef struct {
    int routing_table[NOC_PORT_MAX][NOC_PORT_MAX];
} noc_crossbar_t;

typedef struct {
    int    router_id;
    noc_coord_t location;

    noc_router_input_buffer_t input_buffers[NOC_PORT_MAX];
    noc_vc_allocator_t vc_alloc;
    noc_switch_allocator_t sw_alloc;
    noc_crossbar_t crossbar;

    noc_pipeline_stage_t current_stage[NOC_PORT_MAX];
    int  lookahead_routes[NOC_PORT_MAX];
    bool speculative_vc[NOC_PORT_MAX];

    int cycle_count;
    int flits_processed;
    int flits_stalled;
} noc_router_t;

void noc_router_init(noc_router_t *router, int id, noc_coord_t loc);
void noc_router_reset(noc_router_t *router);

void noc_router_input_buffer_init(noc_router_input_buffer_t *buf, int vc);
bool noc_router_input_buffer_push(noc_router_input_buffer_t *buf, const noc_flit_t *flit);
bool noc_router_input_buffer_pop(noc_router_input_buffer_t *buf, noc_flit_t *out);

void noc_vc_allocator_init(noc_vc_allocator_t *alloc);
int  noc_vc_allocator_request(noc_vc_allocator_t *alloc, int input_port, int output_port);
bool noc_vc_allocator_grant(noc_vc_allocator_t *alloc, int vc_id, int output_port);
void noc_vc_allocator_release(noc_vc_allocator_t *alloc, int vc_id);

void noc_switch_allocator_init(noc_switch_allocator_t *alloc);
int  noc_switch_request(noc_switch_allocator_t *alloc, int input_port, int output_port);
bool noc_switch_grant(noc_switch_allocator_t *alloc, int output_port, int *winner_input);

void noc_crossbar_init(noc_crossbar_t *xbar);
void noc_crossbar_configure(noc_crossbar_t *xbar, int input_port, int output_port);
void noc_crossbar_transfer(noc_crossbar_t *xbar, const noc_flit_t *flits_in[NOC_PORT_MAX],
                            noc_flit_t *flits_out[NOC_PORT_MAX]);

int  noc_lookahead_route(const noc_router_t *router, const noc_topology_t *topo,
                          noc_coord_t dst, int input_port);
bool noc_speculative_vc_allocation(const noc_router_t *router, int input_port);

void noc_router_cycle(noc_router_t *router, const noc_topology_t *topo,
                       const noc_flit_t *flits_in[NOC_PORT_MAX],
                       noc_flit_t *flits_out[NOC_PORT_MAX]);

int  noc_two_stage_pipeline(noc_router_t *router);
int  noc_three_stage_pipeline(noc_router_t *router);
int  noc_five_stage_pipeline(noc_router_t *router);

void noc_router_dump(const noc_router_t *router);
void noc_pipeline_dump(const noc_router_t *router);
const char *noc_pipeline_stage_name(noc_pipeline_stage_t stage);

#ifdef __cplusplus
}
#endif

#endif
