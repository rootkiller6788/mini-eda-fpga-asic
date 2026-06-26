#ifndef NOC_PERFORMANCE_H
#define NOC_PERFORMANCE_H

#include <stdint.h>
#include <stdbool.h>
#include "noc_topology.h"
#include "vc_wormhole.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NOC_TRAFFIC_UNIFORM       = 0,
    NOC_TRAFFIC_TRANSPOSE     = 1,
    NOC_TRAFFIC_BIT_COMPLEMENT = 2,
    NOC_TRAFFIC_TORNADO       = 3,
    NOC_TRAFFIC_NEIGHBOR      = 4,
    NOC_TRAFFIC_HOTSPOT       = 5,
    NOC_TRAFFIC_COUNT
} noc_traffic_pattern_t;

typedef struct {
    int  src_id;
    int  dst_id;
    int  packet_size;
    int  injection_cycle;
    bool delivered;
    int  delivery_cycle;
    int  hop_count;
} noc_packet_trace_t;

typedef struct {
    double total_latency_cycles;
    double total_packets;
    double total_flits;
    double total_hops;
    double avg_latency;
    double max_latency;
    double min_latency;
    double throughput_gbps;
    double injection_rate;
    double saturation_point;
    int    stalled_cycles;
    int    total_cycles;
} noc_perf_stats_t;

void noc_perf_init(noc_perf_stats_t *stats);
void noc_perf_record_packet(noc_perf_stats_t *stats, const noc_packet_trace_t *trace);
void noc_perf_compute(noc_perf_stats_t *stats, double cycle_time_ns, int flit_width);
void noc_perf_reset(noc_perf_stats_t *stats);

int  noc_traffic_generate_dst(noc_traffic_pattern_t pattern, int src, int num_nodes,
                               noc_topology_type_t topo, int width);
int  noc_traffic_hotspot_node(int num_nodes, int hotspot_count);

void noc_injection_sweep(const noc_topology_t *topo, noc_traffic_pattern_t pattern,
                          double cycle_time_ns, int flit_width, int num_sweep_points,
                          double *injection_rates, noc_perf_stats_t *results);

typedef struct {
    int           cycle;
    noc_flit_t    flit;
    int           input_port;
    int           output_port;
    int           router_id;
    bool          stalled;
} noc_flit_event_t;

void noc_event_queue_init(void);
void noc_event_queue_push(const noc_flit_event_t *evt, int max_events, int *count, noc_flit_event_t *buf);
void noc_event_queue_pop(noc_flit_event_t *out, int *count, noc_flit_event_t *buf);

bool noc_detect_saturation(const noc_perf_stats_t *stats, double threshold);
double noc_effective_throughput(const noc_perf_stats_t *stats);
double noc_injection_rate_normalized(double rate, int num_nodes);

void noc_perf_dump(const noc_perf_stats_t *stats);
const char *noc_traffic_pattern_name(noc_traffic_pattern_t pat);

#ifdef __cplusplus
}
#endif

#endif
