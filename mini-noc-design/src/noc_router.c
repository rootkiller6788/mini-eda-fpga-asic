/**
 * noc_router.c ? NoC Router Microarchitecture Implementation
 *
 * Implements the 5-stage canonical wormhole router pipeline:
 * RC ? VA ? SA ? ST ? LT
 */

#include "noc_router.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ??? Internal: router node ID to coordinate conversion ???????????????? */

/* ??? Router initialization ???????????????????????????????????????????? */

void noc_router_init(noc_router_t *router, const noc_router_config_t *cfg) {
    if (!router || !cfg) return;

    memset(router, 0, sizeof(noc_router_t));
    router->router_id = cfg->router_id;
    router->num_ports = cfg->num_ports;
    router->num_vcs = cfg->num_vcs;
    router->num_pipeline_stages = cfg->pipeline_stages;

    /* Initialize crossbar */
    int32_t i;
    for (i = 0; i < NOC_MAX_PORTS; i++) {
        router->crossbar.mapping[i] = -1;
    }
    router->crossbar.num_ports = cfg->num_ports;

    /* Initialize input ports */
    int32_t p, v;
    for (p = 0; p < cfg->num_ports; p++) {
        noc_input_port_t *port = &router->input_ports[p];
        port->port_id = p;
        port->num_vcs = cfg->num_vcs;
        for (v = 0; v < cfg->num_vcs; v++) {
            noc_vc_t *vc = &port->vcs[v];
            vc->vc_id = v;
            vc->state = VC_IDLE;
            vc->credits = cfg->vc_buffer_depth;
            vc->max_credits = cfg->vc_buffer_depth;
            vc->output_port = -1;
            vc->output_vc = -1;
            vc->route_dst = -1;
            port->buffer_head[v] = 0;
            port->buffer_tail[v] = 0;
            port->buffer_count[v] = 0;
        }
    }
}

/* ??? Router reset ???????????????????????????????????????????????????? */

void noc_router_reset(noc_router_t *router) {
    if (!router) return;

    noc_router_config_t cfg;
    cfg.router_id = router->router_id;
    cfg.num_ports = router->num_ports;
    cfg.num_vcs = router->num_vcs;
    cfg.vc_buffer_depth = router->input_ports[0].vcs[0].max_credits;
    cfg.pipeline_stages = router->num_pipeline_stages;
    cfg.allocator_type = 0;

    noc_router_init(router, &cfg);
}

/* ??? Flit injection ?????????????????????????????????????????????????? */

int noc_router_inject_flit(noc_router_t *router, const noc_flit_t *flit) {
    if (!router || !flit) return -1;

    /* Inject on local port (port 0 = NOC_DIR_LOCAL) */
    noc_input_port_t *port = &router->input_ports[NOC_DIR_LOCAL];
    int32_t vc = flit->vc_id;

    if (vc < 0 || vc >= router->num_vcs) return -1;

    /* Check buffer space (max 8 flits per VC FIFO) */
    if (port->buffer_count[vc] >= 8) return -1;

    int32_t tail = port->buffer_tail[vc];
    port->buffer[vc][tail] = *flit;
    port->buffer_tail[vc] = (tail + 1) % 8;
    port->buffer_count[vc]++;

    router->flits_in++;
    return 0;
}

/* ??? Flit ejection ??????????????????????????????????????????????????? */

int noc_router_eject_flit(noc_router_t *router, noc_flit_t *flit) {
    if (!router || !flit) return -1;

    /* Eject from local port output buffer */
    noc_input_port_t *port = &router->input_ports[NOC_DIR_LOCAL];
    int32_t v;
    for (v = 0; v < router->num_vcs; v++) {
        if (port->buffer_count[v] > 0) {
            int32_t head = port->buffer_head[v];
            *flit = port->buffer[v][head];
            port->buffer_head[v] = (head + 1) % 8;
            port->buffer_count[v]--;
            router->flits_out++;
            return 0;
        }
    }
    return -1;
}

/* ??? RC: Routing Computation stage ??????????????????????????????????? */

void noc_router_rc_stage(noc_router_t *router,
                         int (*route_func)(int src, int dst, int router_id)) {
    if (!router) return;

    int32_t p, v;
    for (p = 0; p < router->num_ports; p++) {
        noc_input_port_t *port = &router->input_ports[p];
        for (v = 0; v < router->num_vcs; v++) {
            noc_vc_t *vc = &port->vcs[v];
            if (vc->state != VC_IDLE) continue;

            /* Check if there's a head flit waiting */
            if (port->buffer_count[v] <= 0) continue;

            int32_t head = port->buffer_head[v];
            noc_flit_t *flit = &port->buffer[v][head];
            if (flit->type != FLIT_TYPE_HEAD && flit->type != FLIT_TYPE_SINGLE) {
                continue;
            }

            /* Compute output port using routing function */
            int32_t out_port = -1;
            if (route_func) {
                out_port = route_func(flit->src_id, flit->dst_id, router->router_id);
            }

            if (out_port >= 0 && out_port < router->num_ports) {
                vc->state = VC_ROUTING;
                vc->output_port = out_port;
                vc->route_dst = flit->dst_id;
                router->packets_routed++;
            }
        }
    }
}

/* ??? VA: Virtual Channel Allocation stage ???????????????????????????? */

void noc_router_va_stage(noc_router_t *router) {
    if (!router) return;

    int32_t p, v;
    for (p = 0; p < router->num_ports; p++) {
        noc_input_port_t *port = &router->input_ports[p];
        for (v = 0; v < router->num_vcs; v++) {
            noc_vc_t *vc = &port->vcs[v];
            if (vc->state != VC_ROUTING) continue;

            /* Select an available output VC on the target output port */
            int32_t out_port = vc->output_port;
            if (out_port < 0 || out_port >= router->num_ports) {
                vc->state = VC_IDLE;
                router->vc_alloc_failures++;
                continue;
            }

            /* Find first idle VC on output port */
            int32_t ov;
            int32_t found = -1;
            for (ov = 0; ov < router->num_vcs; ov++) {
                if (router->input_ports[out_port].vcs[ov].state == VC_IDLE) {
                    found = ov;
                    break;
                }
            }

            if (found >= 0) {
                vc->output_vc = found;
                vc->state = VC_WAITING;
                /* Reserve output VC */
                router->input_ports[out_port].vcs[found].state = VC_WAITING;
            } else {
                router->vc_alloc_failures++;
            }
        }
    }
}

/* ??? SA: Switch Allocation stage ????????????????????????????????????? */

void noc_router_sa_stage(noc_router_t *router) {
    if (!router) return;

    /* Reset crossbar */
    int32_t i;
    for (i = 0; i < NOC_MAX_PORTS; i++) {
        router->crossbar.mapping[i] = -1;
    }
    router->crossbar.grants = 0;

    /* Simple separable round-robin allocator:
     * For each output port, grant to one requesting input VC */
    int32_t out_port;
    for (out_port = 0; out_port < router->num_ports; out_port++) {
        /* Find any VC waiting for this output port */
        int best_p = -1, best_v = -1;
        int32_t p, v;
        for (p = 0; p < router->num_ports; p++) {
            if (router->crossbar.mapping[p] >= 0) continue; /* already granted */
            noc_input_port_t *port = &router->input_ports[p];
            for (v = 0; v < router->num_vcs; v++) {
                noc_vc_t *vc = &port->vcs[v];
                if (vc->state == VC_WAITING && vc->output_port == out_port) {
                    /* Only grant if there's a flit ready */
                    if (port->buffer_count[v] > 0) {
                        best_p = p;
                        best_v = v;
                        break;
                    }
                }
            }
            if (best_p >= 0) break;
        }

        if (best_p >= 0) {
            router->crossbar.mapping[best_p] = out_port;
            router->input_ports[best_p].vcs[best_v].state = VC_ACTIVE;
            router->crossbar.grants |= (1 << best_p);
        } else {
            router->switch_alloc_failures++;
        }
    }
}

/* ??? ST: Switch Traversal stage ?????????????????????????????????????? */

void noc_router_st_stage(noc_router_t *router) {
    if (!router) return;

    int32_t p;
    for (p = 0; p < router->num_ports; p++) {
        int32_t out = router->crossbar.mapping[p];
        if (out < 0) continue;

        /* Find the active VC on input port p */
        int32_t v;
        for (v = 0; v < router->num_vcs; v++) {
            noc_vc_t *vc = &router->input_ports[p].vcs[v];
            noc_input_port_t *port = &router->input_ports[p];

            if (vc->state == VC_ACTIVE && port->buffer_count[v] > 0) {
                /* Transfer flit from input buffer to output port */
                /* (In a real implementation, this traverses the crossbar) */
                int32_t head = port->buffer_head[v];
                noc_flit_t flit = port->buffer[v][head];
                port->buffer_head[v] = (head + 1) % 8;
                port->buffer_count[v]--;

                /* Place on output port (enqueue as incoming for downstream) */
                /* For now, just consume credit and count */
                vc->flits_transmitted++;

                /* If tail flit, release resources */
                if (flit.type == FLIT_TYPE_TAIL || flit.type == FLIT_TYPE_SINGLE) {
                    /* Release output VC */
                    int32_t out_vc = vc->output_vc;
                    if (out_vc >= 0) {
                        router->input_ports[out].vcs[out_vc].state = VC_IDLE;
                    }
                    vc->state = VC_IDLE;
                    vc->output_port = -1;
                    vc->output_vc = -1;
                    vc->route_dst = -1;
                    vc->packets_transmitted++;
                }

                router->flits_out++;
                break;
            }
        }
    }
}

/* ??? LT: Link Traversal stage ???????????????????????????????????????? */

void noc_router_lt_stage(noc_router_t *router) {
    if (!router) return;

    /* In link traversal, flits are physically transmitted.
     * Update credit counters: receiving router has consumed flit,
     * so send credit back upstream. */
    int32_t p;
    for (p = 0; p < router->num_ports; p++) {
        /* Return credits to upstream for flits consumed in previous cycle */
        int32_t v;
        for (v = 0; v < router->num_vcs; v++) {
            noc_vc_t *vc = &router->input_ports[p].vcs[v];
            if (vc->credits < vc->max_credits) {
                /* One credit returned per cycle (simplified model) */
                vc->credits++;
            }
        }
    }
}

/* ??? Full pipeline cycle ????????????????????????????????????????????? */

void noc_router_cycle(noc_router_t *router,
                      int (*route_func)(int src, int dst, int router_id)) {
    if (!router) return;

    router->cycle_count++;
    noc_router_lt_stage(router);   /* Pipeline: oldest ? Credit return */
    noc_router_st_stage(router);   /* Flits traverse */
    noc_router_sa_stage(router);   /* Switch arbitration */
    noc_router_va_stage(router);   /* VC allocation */
    noc_router_rc_stage(router, route_func); /* Routing computation */
}

/* ??? Allocator ??????????????????????????????????????????????????????? */

void noc_allocator_init(noc_separable_allocator_t *alloc,
                        int32_t num_ports, int32_t num_vcs) {
    if (!alloc) return;
    memset(alloc, 0, sizeof(noc_separable_allocator_t));

    int32_t p, v;
    for (p = 0; p < num_ports; p++) {
        for (v = 0; v < num_vcs; v++) {
            alloc->va_arbiters[p][v].num_inputs = num_ports;
        }
        alloc->sa_arbiters[p].num_inputs = num_ports;
    }
}

/* ??? Round-robin arbitration ????????????????????????????????????????? */

int32_t noc_rr_arbitrate(noc_rr_arbiter_t *arb, uint32_t request_mask) {
    if (!arb || request_mask == 0) return -1;

    int32_t N = arb->num_inputs;
    int32_t i;
    /* Search from priority pointer, wrapping around */
    for (i = 0; i < N; i++) {
        int32_t idx = (arb->priority_ptr + i) % N;
        if (request_mask & (1u << (uint32_t)idx)) {
            /* Grant */
            arb->last_grant = idx;
            arb->priority_ptr = (idx + 1) % N; /* Round-robin update */
            return idx;
        }
    }
    return -1;
}

/* ??? VC credit operations ???????????????????????????????????????????? */

int noc_vc_has_credit(const noc_vc_t *vc) {
    if (!vc) return 0;
    return (vc->credits > 0) ? 1 : 0;
}

int noc_vc_consume_credit(noc_vc_t *vc) {
    if (!vc || vc->credits <= 0) return -1;
    vc->credits--;
    return 0;
}

void noc_vc_return_credit(noc_vc_t *vc) {
    if (!vc) return;
    if (vc->credits < vc->max_credits) {
        vc->credits++;
    }
}

/* ??? Statistics ?????????????????????????????????????????????????????? */

void noc_router_print_stats(const noc_router_t *router) {
    if (!router) {
        printf("Router: NULL\n");
        return;
    }

    printf("=== Router %d Stats (cycle %u) ===\n",
           router->router_id, router->cycle_count);
    printf("Ports: %d, VCs: %d, Pipeline stages: %d\n",
           router->num_ports, router->num_vcs, router->num_pipeline_stages);
    printf("Flits in: %llu, Flits out: %llu\n",
           (unsigned long long)router->flits_in,
           (unsigned long long)router->flits_out);
    printf("Packets routed: %llu\n",
           (unsigned long long)router->packets_routed);
    printf("VC alloc failures: %llu, SA failures: %llu\n",
           (unsigned long long)router->vc_alloc_failures,
           (unsigned long long)router->switch_alloc_failures);

    double util = noc_router_utilization(router);
    printf("Utilization: %.4f (%.2f%%)\n", util, util * 100.0);
}

double noc_router_utilization(const noc_router_t *router) {
    if (!router || router->cycle_count == 0) return 0.0;

    /* Port-level utilization: fraction of cycles any output was used */
    double sum = 0.0;
    int32_t p;
    for (p = 0; p < router->num_ports; p++) {
        sum += router->utilization[p];
    }
    return sum / (double)router->num_ports;
}
