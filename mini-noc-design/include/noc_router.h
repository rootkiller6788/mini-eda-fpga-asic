/**
 * noc_router.h ? L2/L3: NoC Router Microarchitecture
 *
 * Defines the canonical 5-stage wormhole router pipeline:
 *   RC ? VA ? SA ? ST ? LT
 *
 * Core Concepts (L2):
 *   - Wormhole flow control: flit-level pipelining
 *   - Virtual Channels (VCs): deadlock avoidance + throughput
 *   - Credit-based backpressure
 *
 * Engineering Structures (L3):
 *   - Crossbar switch (NxN)
 *   - VC state machine (idle, routing, waiting, active)
 *   - Input-buffered router architecture
 *
 * Standards (L4):
 *   - Dally, "Virtual-Channel Flow Control" (IEEE TPDS 1992)
 *   - Peh & Dally, "A Delay Model for Router Microarchitectures" (IEEE Micro 2001)
 *
 * Course Mapping:
 *   - MIT 6.004: pipelined processor ? pipelined router analogy
 *   - Stanford EE 282: Computer Architecture ? interconnection networks
 *   - CMU 18-742: router microarchitecture, VC allocation
 */

#ifndef NOC_ROUTER_H
#define NOC_ROUTER_H

#include <stdint.h>
#include <stddef.h>
#include "noc_topology.h"

/* ??? L1: Flit/Packet definitions ???????????????????????????????????? */

typedef enum {
    FLIT_TYPE_HEAD  = 0,
    FLIT_TYPE_BODY  = 1,
    FLIT_TYPE_TAIL  = 2,
    FLIT_TYPE_SINGLE = 3
} flit_type_t;

typedef struct noc_flit {
    int32_t      flit_id;
    flit_type_t  type;
    int32_t      src_id;
    int32_t      dst_id;
    int32_t      packet_id;
    int32_t      vc_id;
    int32_t      sequence_no;
    uint64_t     payload;
    uint32_t     timestamp;
    uint32_t     priority;
} noc_flit_t;

#define NOC_MAX_VC    8
#define NOC_MAX_PORTS 8
#define NOC_FLIT_SIZE 128

typedef enum {
    VC_IDLE     = 0,
    VC_ROUTING  = 1,
    VC_WAITING  = 2,
    VC_ACTIVE   = 3
} vc_state_t;

typedef struct noc_vc {
    int32_t       vc_id;
    vc_state_t    state;
    int32_t       credits;
    int32_t       max_credits;
    int32_t       output_port;
    int32_t       output_vc;
    int32_t       route_dst;
    int32_t       packets_transmitted;
    int32_t       flits_transmitted;
    uint32_t      wait_cycles;
} noc_vc_t;

typedef struct noc_input_port {
    int32_t       port_id;
    noc_vc_t      vcs[NOC_MAX_VC];
    int32_t       num_vcs;
    noc_flit_t    buffer[NOC_MAX_VC][8];
    int32_t       buffer_head[NOC_MAX_VC];
    int32_t       buffer_tail[NOC_MAX_VC];
    int32_t       buffer_count[NOC_MAX_VC];
} noc_input_port_t;

typedef struct noc_crossbar {
    int32_t mapping[NOC_MAX_PORTS];
    int32_t num_ports;
    int32_t grants;
} noc_crossbar_t;

typedef struct noc_router {
    int32_t          router_id;
    int32_t          num_ports;
    int32_t          num_vcs;
    int32_t          num_pipeline_stages;
    uint32_t         cycle_count;
    noc_input_port_t input_ports[NOC_MAX_PORTS];
    noc_crossbar_t   crossbar;
    uint64_t         flits_in;
    uint64_t         flits_out;
    uint64_t         packets_routed;
    uint64_t         vc_alloc_failures;
    uint64_t         switch_alloc_failures;
    double           avg_latency;
    double           utilization[NOC_MAX_PORTS];
} noc_router_t;

typedef struct noc_router_config {
    int32_t router_id;
    int32_t num_ports;
    int32_t num_vcs;
    int32_t vc_buffer_depth;
    int32_t pipeline_stages;
    int32_t allocator_type;
} noc_router_config_t;

typedef struct noc_rr_arbiter {
    int32_t num_inputs;
    int32_t priority_ptr;
    int32_t last_grant;
} noc_rr_arbiter_t;

typedef struct noc_separable_allocator {
    noc_rr_arbiter_t va_arbiters[NOC_MAX_PORTS][NOC_MAX_VC];
    noc_rr_arbiter_t sa_arbiters[NOC_MAX_PORTS];
} noc_separable_allocator_t;

void noc_router_init(noc_router_t *router, const noc_router_config_t *cfg);
void noc_router_reset(noc_router_t *router);
int noc_router_inject_flit(noc_router_t *router, const noc_flit_t *flit);
int noc_router_eject_flit(noc_router_t *router, noc_flit_t *flit);
void noc_router_rc_stage(noc_router_t *router,
                         int (*route_func)(int src, int dst, int router_id));
void noc_router_va_stage(noc_router_t *router);
void noc_router_sa_stage(noc_router_t *router);
void noc_router_st_stage(noc_router_t *router);
void noc_router_lt_stage(noc_router_t *router);
void noc_router_cycle(noc_router_t *router,
                      int (*route_func)(int src, int dst, int router_id));
void noc_allocator_init(noc_separable_allocator_t *alloc,
                        int32_t num_ports, int32_t num_vcs);
int32_t noc_rr_arbitrate(noc_rr_arbiter_t *arb, uint32_t request_mask);
int noc_vc_has_credit(const noc_vc_t *vc);
int noc_vc_consume_credit(noc_vc_t *vc);
void noc_vc_return_credit(noc_vc_t *vc);
void noc_router_print_stats(const noc_router_t *router);
double noc_router_utilization(const noc_router_t *router);

#endif /* NOC_ROUTER_H */
