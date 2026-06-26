/**
 * noc_flowctrl.h ? L2/L3: NoC Flow Control Mechanisms
 *
 * Implements credit-based flow control, wormhole switching,
 * and buffer management.
 *
 * Standards (L4):
 *   - Dally, "Virtual-Channel Flow Control" (IEEE TPDS 1992)
 *     ? Credit-based flow control eliminates need for round-trip signaling
 *   - Kermani & Kleinrock, "Virtual Cut-Through" (Computer Networks 1979)
 *
 * Course Mapping:
 *   - MIT 6.004: pipelining, flow control
 *   - Stanford CS 144: flow control, sliding window (analogy)
 *   - ETH 263-3501: parallel programming ? communication patterns
 */

#ifndef NOC_FLOWCTRL_H
#define NOC_FLOWCTRL_H

#include <stdint.h>
#include <stddef.h>
#include "noc_router.h"

typedef enum {
    FC_CREDIT_BASED = 0,
    FC_ON_OFF       = 1,
    FC_ACK_NACK     = 2,
    FC_COUNT
} noc_fc_mode_t;

typedef struct noc_credit_channel {
    int32_t credits_available;
    int32_t max_credits;
    int32_t credits_returned;
    int32_t flits_sent;
    int32_t stall_cycles;
} noc_credit_channel_t;

typedef struct noc_buffer_pool {
    int32_t  total_slots;
    int32_t  used_slots;
    int32_t  per_vc_min;
    int32_t  per_vc_max;
    int32_t  num_vcs;
    int32_t  vc_allocated[NOC_MAX_VC];
} noc_buffer_pool_t;

typedef struct noc_wormhole_packet {
    int32_t   packet_id;
    int32_t   num_flits;
    int32_t   flits_sent;
    int32_t   flits_acked;
    int32_t   state;
    int32_t   src_id;
    int32_t   dst_id;
    uint32_t  start_cycle;
    uint32_t  end_cycle;
} noc_wormhole_packet_t;

#define NOC_MAX_ACTIVE_PACKETS 128

typedef struct noc_flow_controller {
    noc_fc_mode_t          mode;
    noc_credit_channel_t   credits[NOC_MAX_PORTS][NOC_MAX_VC];
    noc_buffer_pool_t      buffer_pool;
    noc_wormhole_packet_t  active_packets[NOC_MAX_ACTIVE_PACKETS];
    int32_t                num_active_packets;
    uint64_t               total_flits_sent;
    uint64_t               total_credits_consumed;
    uint64_t               total_stall_cycles;
    uint64_t               packets_completed;
    double                 avg_packet_latency;
} noc_flow_controller_t;

void noc_fc_init(noc_flow_controller_t *fc, noc_fc_mode_t mode,
                 int32_t num_ports, int32_t num_vcs,
                 int32_t credits_per_vc);
void noc_fc_reset(noc_flow_controller_t *fc);
int noc_fc_can_send(const noc_flow_controller_t *fc,
                    int32_t port, int32_t vc);
int noc_fc_send_flit(noc_flow_controller_t *fc,
                     int32_t port, int32_t vc, const noc_flit_t *flit);
void noc_fc_return_credit(noc_flow_controller_t *fc,
                          int32_t port, int32_t vc);
int noc_fc_start_packet(noc_flow_controller_t *fc,
                        int32_t src_id, int32_t dst_id, int32_t num_flits);
int noc_fc_flit_sent(noc_flow_controller_t *fc, int32_t packet_id);
int32_t noc_fc_packet_latency(const noc_flow_controller_t *fc,
                              int32_t packet_id);
void noc_buffer_pool_init(noc_buffer_pool_t *pool,
                          int32_t total_slots, int32_t num_vcs,
                          int32_t per_vc_min);
int32_t noc_buffer_pool_alloc(noc_buffer_pool_t *pool,
                              int32_t vc_id, int32_t requested);
void noc_buffer_pool_free(noc_buffer_pool_t *pool,
                          int32_t vc_id, int32_t count);
int32_t noc_buffer_pool_available(const noc_buffer_pool_t *pool);
double noc_fc_critical_gap(int32_t credits_per_vc,
                           int32_t router_latency,
                           int32_t num_hops);
void noc_fc_print_stats(const noc_flow_controller_t *fc);
double noc_fc_avg_latency(const noc_flow_controller_t *fc);
double noc_fc_stall_rate(const noc_flow_controller_t *fc);

#endif /* NOC_FLOWCTRL_H */
