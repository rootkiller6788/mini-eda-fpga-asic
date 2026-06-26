/**
 * noc_flowctrl.c ? NoC Flow Control Implementation
 *
 * Credit-based flow control, wormhole switching, buffer management.
 */

#include "noc_flowctrl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ??? Initialization ?????????????????????????????????????????????????? */

void noc_fc_init(noc_flow_controller_t *fc, noc_fc_mode_t mode,
                 int32_t num_ports, int32_t num_vcs,
                 int32_t credits_per_vc) {
    if (!fc) return;

    memset(fc, 0, sizeof(noc_flow_controller_t));
    fc->mode = mode;

    int32_t p, v;
    for (p = 0; p < num_ports; p++) {
        for (v = 0; v < num_vcs; v++) {
            fc->credits[p][v].credits_available = credits_per_vc;
            fc->credits[p][v].max_credits = credits_per_vc;
        }
    }
}

/* ??? Reset ??????????????????????????????????????????????????????????? */

void noc_fc_reset(noc_flow_controller_t *fc) {
    if (!fc) return;
    noc_fc_mode_t mode = fc->mode;
    int32_t credits = fc->credits[0][0].max_credits;
    noc_fc_init(fc, mode, NOC_MAX_PORTS, NOC_MAX_VC, credits);
}

/* ??? Credit check ???????????????????????????????????????????????????? */

int noc_fc_can_send(const noc_flow_controller_t *fc,
                    int32_t port, int32_t vc) {
    if (!fc) return 0;
    if (port < 0 || port >= NOC_MAX_PORTS) return 0;
    if (vc < 0 || vc >= NOC_MAX_VC) return 0;

    return (fc->credits[port][vc].credits_available > 0) ? 1 : 0;
}

/* ??? Send flit (consume credit) ?????????????????????????????????????? */

int noc_fc_send_flit(noc_flow_controller_t *fc,
                     int32_t port, int32_t vc, const noc_flit_t *flit) {
    (void)flit;

    if (!fc) return -1;
    if (port < 0 || port >= NOC_MAX_PORTS) return -1;
    if (vc < 0 || vc >= NOC_MAX_VC) return -1;

    noc_credit_channel_t *ch = &fc->credits[port][vc];

    if (ch->credits_available <= 0) {
        ch->stall_cycles++;
        fc->total_stall_cycles++;
        return -1;
    }

    ch->credits_available--;
    ch->flits_sent++;
    fc->total_flits_sent++;
    fc->total_credits_consumed++;

    return 0;
}

/* ??? Return credit ??????????????????????????????????????????????????? */

void noc_fc_return_credit(noc_flow_controller_t *fc,
                          int32_t port, int32_t vc) {
    if (!fc) return;
    if (port < 0 || port >= NOC_MAX_PORTS) return;
    if (vc < 0 || vc >= NOC_MAX_VC) return;

    noc_credit_channel_t *ch = &fc->credits[port][vc];

    if (ch->credits_available < ch->max_credits) {
        ch->credits_available++;
        ch->credits_returned++;
    }
}

/* ??? Packet management ??????????????????????????????????????????????? */

int noc_fc_start_packet(noc_flow_controller_t *fc,
                        int32_t src_id, int32_t dst_id, int32_t num_flits) {
    if (!fc) return -1;
    if (fc->num_active_packets >= NOC_MAX_ACTIVE_PACKETS) return -1;

    int32_t pid = fc->num_active_packets;
    noc_wormhole_packet_t *pkt = &fc->active_packets[pid];

    pkt->packet_id = pid;
    pkt->num_flits = num_flits;
    pkt->flits_sent = 0;
    pkt->flits_acked = 0;
    pkt->state = 0; /* waiting */
    pkt->src_id = src_id;
    pkt->dst_id = dst_id;
    pkt->start_cycle = 0;
    pkt->end_cycle = 0;

    fc->num_active_packets++;
    return pid;
}

int noc_fc_flit_sent(noc_flow_controller_t *fc, int32_t packet_id) {
    if (!fc || packet_id < 0 || packet_id >= fc->num_active_packets) return -1;

    noc_wormhole_packet_t *pkt = &fc->active_packets[packet_id];
    pkt->flits_sent++;
    pkt->state = 1; /* transmitting */

    if (pkt->flits_sent >= pkt->num_flits) {
        pkt->state = 2; /* done */
        pkt->end_cycle = 0; /* set externally */
        fc->packets_completed++;
        return 1; /* complete */
    }

    return 0; /* still in flight */
}

int32_t noc_fc_packet_latency(const noc_flow_controller_t *fc,
                              int32_t packet_id) {
    if (!fc || packet_id < 0 || packet_id >= fc->num_active_packets) return -1;
    if (fc->active_packets[packet_id].state != 2) return -1;

    return (int32_t)(fc->active_packets[packet_id].end_cycle -
                     fc->active_packets[packet_id].start_cycle);
}

/* ??? Buffer pool ????????????????????????????????????????????????????? */

void noc_buffer_pool_init(noc_buffer_pool_t *pool,
                          int32_t total_slots, int32_t num_vcs,
                          int32_t per_vc_min) {
    if (!pool) return;

    memset(pool, 0, sizeof(noc_buffer_pool_t));
    pool->total_slots = total_slots;
    pool->used_slots = 0;
    pool->num_vcs = num_vcs;
    pool->per_vc_min = per_vc_min;
    pool->per_vc_max = total_slots - (num_vcs - 1) * per_vc_min; /* rest */
}

int32_t noc_buffer_pool_alloc(noc_buffer_pool_t *pool,
                              int32_t vc_id, int32_t requested) {
    if (!pool || vc_id < 0 || vc_id >= pool->num_vcs) return -1;

    int32_t available = noc_buffer_pool_available(pool);
    int32_t current = pool->vc_allocated[vc_id];
    int32_t max_extra = pool->per_vc_max - current;
    if (max_extra < 0) max_extra = 0;

    int32_t grant = (requested < available) ? requested : available;
    if (grant > max_extra) grant = max_extra;
    if (grant < 0) grant = 0;

    pool->vc_allocated[vc_id] += grant;
    pool->used_slots += grant;

    return grant;
}

void noc_buffer_pool_free(noc_buffer_pool_t *pool,
                          int32_t vc_id, int32_t count) {
    if (!pool || vc_id < 0 || vc_id >= pool->num_vcs) return;
    if (count > pool->vc_allocated[vc_id]) {
        count = pool->vc_allocated[vc_id];
    }

    pool->vc_allocated[vc_id] -= count;
    pool->used_slots -= count;
    if (pool->used_slots < 0) pool->used_slots = 0;
}

int32_t noc_buffer_pool_available(const noc_buffer_pool_t *pool) {
    if (!pool) return 0;
    return pool->total_slots - pool->used_slots;
}

/* ??? Analytical: Critical inter-flit gap ??????????????????????????????
 *
 * Based on Dally (1992):
 *   t_gap = t_credit_round_trip / credits_per_vc
 *
 * Where:
 *   t_credit_round_trip = 2 * router_latency * num_hops
 *   (round trip: flit travels H hops, credit returns H hops)
 *
 * If t_gap > 1, the link is under-subscribed (bottleneck is credits).
 * If t_gap < 1, the link can sustain continuous flit injection.
 */

double noc_fc_critical_gap(int32_t credits_per_vc,
                           int32_t router_latency,
                           int32_t num_hops) {
    if (credits_per_vc <= 0) return 1e9; /* infinite gap: guaranteed stall */

    double round_trip = 2.0 * (double)router_latency * (double)num_hops;
    return round_trip / (double)credits_per_vc;
}

/* ??? Statistics ?????????????????????????????????????????????????????? */

void noc_fc_print_stats(const noc_flow_controller_t *fc) {
    if (!fc) {
        printf("Flow Controller: NULL\n");
        return;
    }

    printf("=== Flow Control Stats ===\n");
    printf("Mode: %d, Packets completed: %llu\n",
           fc->mode, (unsigned long long)fc->packets_completed);
    printf("Total flits sent: %llu, credits consumed: %llu\n",
           (unsigned long long)fc->total_flits_sent,
           (unsigned long long)fc->total_credits_consumed);
    printf("Total stall cycles: %llu, Stall rate: %.4f\n",
           (unsigned long long)fc->total_stall_cycles,
           noc_fc_stall_rate(fc));
    printf("Avg packet latency: %.2f cycles\n", noc_fc_avg_latency(fc));
}

double noc_fc_avg_latency(const noc_flow_controller_t *fc) {
    if (!fc || fc->packets_completed == 0) return 0.0;

    double sum = 0.0;
    int32_t count = 0;
    int32_t i;
    for (i = 0; i < fc->num_active_packets; i++) {
        if (fc->active_packets[i].state == 2) {
            sum += (double)(fc->active_packets[i].end_cycle -
                            fc->active_packets[i].start_cycle);
            count++;
        }
    }
    return (count > 0) ? (sum / (double)count) : 0.0;
}

double noc_fc_stall_rate(const noc_flow_controller_t *fc) {
    if (!fc || fc->total_credits_consumed == 0) return 0.0;
    return (double)fc->total_stall_cycles /
           (double)(fc->total_credits_consumed + fc->total_stall_cycles);
}
