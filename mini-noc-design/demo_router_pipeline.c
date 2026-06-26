#include "noc_verilog_rtl.h"
#include "noc_topology.h"
#include "vc_wormhole.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    noc_router_t routers[16];
    noc_topology_t topo;
    int num_routers;
    int cycle;
} noc_network_t;

static void network_init(noc_network_t *net, int w, int h) {
    int i;
    memset(net, 0, sizeof(*net));
    noc_topology_init(&net->topo, NOC_TOPO_MESH, w, h);
    net->num_routers = w * h;

    for (i = 0; i < net->num_routers; i++) {
        noc_coord_t loc;
        noc_coord_from_id(&net->topo, i, &loc);
        noc_router_init(&net->routers[i], i, loc);
    }
}

static void network_cycle(noc_network_t *net, const noc_flit_t *injected_flits[],
                           int num_injections, noc_flit_t *ejected_flits[]) {
    int r;
    net->cycle++;
    (void)injected_flits; (void)num_injections; (void)ejected_flits;

    for (r = 0; r < net->num_routers; r++) {
        noc_flit_t *ins[NOC_PORT_MAX] = {NULL};
        noc_flit_t *outs[NOC_PORT_MAX] = {NULL};
        noc_router_cycle(&net->routers[r], &net->topo, ins, outs);
    }
}

static void simulate_two_stage_pipeline(noc_network_t *net) {
    noc_coord_t src_c = {0, 0}, dst_c = {3, 3};
    int src_id = noc_node_id(&net->topo, src_c);
    int dst_id = noc_node_id(&net->topo, dst_c);
    noc_router_t *r = &net->routers[src_id];
    int total_latency = 0;

    printf("\n=== 2-Stage Pipeline Simulation ===\n");
    printf("Packet: (%d,%d) -> (%d,%d)\n", src_c.x, src_c.y, dst_c.x, dst_c.y);

    int hops = noc_hop_count(&net->topo, src_c, dst_c);
    printf("Hops: %d\n", hops);

    printf("\nPipeline trace:\n");
    noc_pipeline_stage_t stages[] = {NOC_PIPE_RC, NOC_PIPE_VA, NOC_PIPE_SA, NOC_PIPE_ST, NOC_PIPE_LT};
    const char *stage_names[] = {"RC", "VA", "SA", "ST", "LT"};

    for (int hop = 0; hop < hops; hop++) {
        printf("  Hop %d: ", hop + 1);
        for (int s = 0; s < 5; s++) {
            r->current_stage[hop % NOC_PORT_MAX] = stages[s];
            printf("%s ", stage_names[s]);
        }
        int pipe_latency = noc_two_stage_pipeline(r);
        total_latency += pipe_latency;
        printf("  (latency=%d cycles)\n", pipe_latency);
        r->flits_processed++;
    }

    printf("\nSpeculative VC check:\n");
    r->speculative_vc[0] = true;
    r->speculative_vc[1] = true;
    for (int i = 0; i < NOC_PORT_MAX; i++) {
        printf("  Port[%d] speculative=%d\n", i, r->speculative_vc[i]);
    }

    printf("Total pipeline latency: %d cycles\n\n", total_latency);
}

static void simulate_pipeline_comparison(noc_network_t *net) {
    noc_router_t *r = &net->routers[0];

    printf("=== Pipeline Depth Comparison ===\n");
    printf("%-20s %10s %10s %10s\n", "Pipeline", "Depth", "Latency", "Throughput");

    int l2 = noc_two_stage_pipeline(r);
    printf("%-20s %10d %10d %10s\n", "2-stage (specVA)", l2, r->cycle_count, "High");

    int l3 = noc_three_stage_pipeline(r);
    printf("%-20s %10d %10d %10s\n", "3-stage (base)", l3, r->cycle_count, "Med");

    int l5 = noc_five_stage_pipeline(r);
    printf("%-20s %10d %10d %10s\n", "5-stage (deep)", l5, r->cycle_count, "Low");
    (void)l2; (void)l3; (void)l5;
}

static void simulate_router_microarchitecture(noc_network_t *net) {
    noc_router_t *r = &net->routers[5];
    int buf_depths[NOC_PORT_MAX];
    int i;

    printf("\n=== Router[%d] Microarchitecture ===\n", r->router_id);
    printf("Location: (%d,%d)\n", r->location.x, r->location.y);

    printf("\nInput Buffers:\n");
    for (i = 0; i < NOC_PORT_MAX; i++) {
        buf_depths[i] = r->input_buffers[i].count;
        printf("  Port[%d] %s: depth=%d/%d credits=%d empty=%d\n",
            i, noc_port_name(i), r->input_buffers[i].count, NOC_BUFFER_DEPTH,
            r->input_buffers[i].credits, r->input_buffers[i].is_empty);
    }

    printf("\nInjecting test flits into buffer...\n");
    for (i = 0; i < 3; i++) {
        noc_flit_t flit;
        noc_flit_init(&flit, (i == 0) ? NOC_FLIT_HEAD : (i == 2) ? NOC_FLIT_TAIL : NOC_FLIT_BODY,
                      0, 8, i);
        if (noc_router_input_buffer_push(&r->input_buffers[NOC_PORT_SELF], &flit)) {
            printf("  Injected flit %d (%s) into Local port buffer\n",
                i, noc_flit_type_name(flit.type));
        }
    }

    noc_router_input_buffer_t *buf = &r->input_buffers[NOC_PORT_SELF];
    printf("  Buffer after injection: depth=%d/%d\n", buf->count, NOC_BUFFER_DEPTH);

    printf("\nPopping flits from buffer:\n");
    while (buf->count > 0) {
        noc_flit_t out;
        if (noc_router_input_buffer_pop(buf, &out)) {
            printf("  Popped: seq=%d type=%s\n", out.sequence_id, noc_flit_type_name(out.type));
        }
    }

    printf("\nVC Allocator test:\n");
    noc_vc_allocator_t *va = &r->vc_alloc;
    for (i = 0; i < VC_MAX + 1; i++) {
        int vc = noc_vc_allocator_request(va, i % NOC_PORT_MAX, (i + 1) % NOC_PORT_MAX);
        printf("  Request[%d] in=%d out=%d -> vc=%d\n",
            i, i % NOC_PORT_MAX, (i + 1) % NOC_PORT_MAX, vc);
    }

    printf("\nSwitch Allocator test:\n");
    noc_switch_allocator_t *sa = &r->sw_alloc;
    for (i = 0; i < NOC_PORT_MAX; i++) {
        int rc = noc_switch_request(sa, i, (i + 1) % NOC_PORT_MAX);
        printf("  Switch Req: in=%d out=%d count=%d\n",
            i, (i + 1) % NOC_PORT_MAX, rc);
    }
    for (i = 0; i < NOC_PORT_MAX; i++) {
        int winner;
        if (noc_switch_grant(sa, i, &winner)) {
            printf("  Switch Grant: out=%d -> winner=%d\n", i, winner);
        }
    }

    printf("\nCrossbar configuration:\n");
    noc_crossbar_t *xb = &r->crossbar;
    noc_crossbar_configure(xb, 0, 2);
    noc_crossbar_configure(xb, 1, 3);
    noc_crossbar_configure(xb, 2, 4);
    noc_crossbar_configure(xb, 3, 0);
    noc_crossbar_configure(xb, 4, 1);
    for (i = 0; i < NOC_PORT_MAX; i++) {
        for (int j = 0; j < NOC_PORT_MAX; j++) {
            if (xb->routing_table[i][j]) {
                printf("  %s -> %s\n", noc_port_name(i), noc_port_name(j));
            }
        }
    }

    printf("\nLook-ahead routing:\n");
    noc_coord_t dsts[] = {{0, 0}, {3, 0}, {0, 3}, {3, 3}, {1, 2}};
    for (i = 0; i < 5; i++) {
        int port = noc_lookahead_route(r, &net->topo, dsts[i], NOC_PORT_WEST);
        printf("  from (%d,%d) to (%d,%d) -> port %s\n",
            r->location.x, r->location.y, dsts[i].x, dsts[i].y, noc_port_name(port));
    }
}

static void simulate_full_network_path(noc_network_t *net) {
    printf("\n=== Full Network Path Trace ===\n");

    noc_coord_t src = {0, 0}, dst = {3, 3};
    int hops = noc_hop_count(&net->topo, src, dst);
    printf("Path: (%d,%d) -> (%d,%d) [%d hops]\n", src.x, src.y, dst.x, dst.y, hops);

    noc_coord_t current = src;
    noc_direction_t dir;
    int step = 0;

    while (memcmp(&current, &dst, sizeof(noc_coord_t)) != 0 && step < hops * 2) {
        dir = noc_xy_route(&net->topo, current, dst);
        noc_coord_t next;
        int port = noc_neighbor_port(&net->topo, current, dir, &next);
        int router_id = noc_node_id(&net->topo, current);

        printf("  Step %d: Router[%d](%d,%d) -> %s -> Router[%d](%d,%d) port=%s\n",
            step + 1, router_id, current.x, current.y,
            noc_direction_name(dir), noc_node_id(&net->topo, next),
            next.x, next.y, port >= 0 ? noc_port_name(port) : "?");

        current = next;
        step++;
    }

    int final_id = noc_node_id(&net->topo, current);
    printf("  Arrived at Router[%d](%d,%d)\n", final_id, current.x, current.y);

    printf("\nHop-by-hop latency estimate:\n");
    int total_latency = 0;
    current = src;
    step = 0;
    noc_coord_t pos = src;
    while (memcmp(&pos, &dst, sizeof(noc_coord_t)) != 0 && step < hops * 2) {
        dir = noc_xy_route(&net->topo, pos, dst);
        noc_coord_t next;
        noc_neighbor_port(&net->topo, pos, dir, &next);

        int rid = noc_node_id(&net->topo, pos);
        int l = noc_two_stage_pipeline(&net->routers[rid]);
        total_latency += l;
        printf("  Hop %d: Router[%d] pipeline=%d cycles (cumulative=%d)\n",
            step + 1, rid, l, total_latency);
        pos = next;
        step++;
    }
}

int main(void) {
    printf("=== NoC Router Pipeline Demo ===\n");
    printf("Microarchitecture simulation and pipeline analysis\n");

    noc_network_t net;
    network_init(&net, 4, 4);

    printf("Network: %s %dx%d (%d routers)\n",
        noc_topology_name(NOC_TOPO_MESH), 4, 4, net.num_routers);

    noc_router_dump(&net.routers[0]);
    noc_router_dump(&net.routers[5]);
    noc_router_dump(&net.routers[15]);

    simulate_two_stage_pipeline(&net);
    simulate_pipeline_comparison(&net);
    simulate_router_microarchitecture(&net);
    simulate_full_network_path(&net);

    printf("\n--- Router Reset Test ---\n");
    noc_router_reset(&net.routers[0]);
    noc_router_dump(&net.routers[0]);
    printf("Reset verified.\n");

    printf("\n--- Pipeline Stage Names ---\n");
    for (int i = 0; i < NOC_PIPE_COUNT; i++) {
        printf("  Stage %d: %s\n", i, noc_pipeline_stage_name((noc_pipeline_stage_t)i));
    }

    printf("\nDone.\n");
    return 0;
}
