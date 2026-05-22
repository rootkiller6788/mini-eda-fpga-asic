#include "noc_performance.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static noc_flit_event_t g_event_buf[4096];
static int g_event_count = 0;

void noc_perf_init(noc_perf_stats_t *stats) {
    memset(stats, 0, sizeof(*stats));
    stats->min_latency = 1e12;
}

void noc_perf_record_packet(noc_perf_stats_t *stats, const noc_packet_trace_t *trace) {
    double latency = (double)(trace->delivery_cycle - trace->injection_cycle);
    stats->total_latency_cycles += latency;
    stats->total_packets += 1.0;
    stats->total_hops     += (double)trace->hop_count;
    if (latency < stats->min_latency) stats->min_latency = latency;
    if (latency > stats->max_latency) stats->max_latency = latency;
}

void noc_perf_compute(noc_perf_stats_t *stats, double cycle_time_ns, int flit_width) {
    if (stats->total_packets > 0) {
        stats->avg_latency = stats->total_latency_cycles / stats->total_packets;
    }
    if (stats->total_cycles > 0 && cycle_time_ns > 0) {
        double total_time_s = (double)stats->total_cycles * cycle_time_ns * 1e-9;
        double total_bits  = stats->total_flits * (double)flit_width;
        stats->throughput_gbps = (total_time_s > 0) ? (total_bits / total_time_s / 1e9) : 0.0;
    }
    if (stats->total_cycles > 0) {
        stats->injection_rate = stats->total_packets / (double)stats->total_cycles;
    }
}

void noc_perf_reset(noc_perf_stats_t *stats) {
    noc_perf_init(stats);
}

int noc_traffic_generate_dst(noc_traffic_pattern_t pattern, int src, int num_nodes,
                              noc_topology_type_t topo, int width) {
    (void)topo;
    switch (pattern) {
        case NOC_TRAFFIC_UNIFORM:
            return rand() % num_nodes;
        case NOC_TRAFFIC_TRANSPOSE: {
            int x = src % width, y = src / width;
            return y * width + x;
        }
        case NOC_TRAFFIC_BIT_COMPLEMENT:
            return (~src) & (num_nodes - 1);
        case NOC_TRAFFIC_TORNADO: {
            int x = src % width, y = src / width;
            int nx = (x + width / 2 - 1 + width) % width;
            int ny = y;
            return ny * width + nx;
        }
        case NOC_TRAFFIC_NEIGHBOR:
            return (src + 1) % num_nodes;
        case NOC_TRAFFIC_HOTSPOT: {
            int hotspot = num_nodes / 2;
            return (rand() % 100 < 20) ? hotspot : (rand() % num_nodes);
        }
        default:
            return src;
    }
}

int noc_traffic_hotspot_node(int num_nodes, int hotspot_count) {
    (void)hotspot_count;
    return num_nodes / 2;
}

void noc_injection_sweep(const noc_topology_t *topo, noc_traffic_pattern_t pattern,
                          double cycle_time_ns, int flit_width, int num_sweep_points,
                          double *injection_rates, noc_perf_stats_t *results) {
    int i;
    for (i = 0; i < num_sweep_points; i++) {
        noc_perf_init(&results[i]);
        results[i].injection_rate = injection_rates[i];
        results[i].total_cycles   = 10000;
        results[i].total_packets  = injection_rates[i] * 10000.0;
        results[i].total_flits    = results[i].total_packets * 5.0;
        results[i].total_hops     = results[i].total_packets *
            (double)(topo->width + topo->height) / 2.0;
        results[i].total_latency_cycles = results[i].total_hops * 2.0;
        noc_perf_compute(&results[i], cycle_time_ns, flit_width);
    }
}

void noc_event_queue_init(void) {
    g_event_count = 0;
}

void noc_event_queue_push(const noc_flit_event_t *evt, int max_events, int *count,
                          noc_flit_event_t *buf) {
    (void)max_events;
    if (buf) buf[*count] = *evt;
    (*count)++;
}

void noc_event_queue_pop(noc_flit_event_t *out, int *count, noc_flit_event_t *buf) {
    if (*count > 0 && buf) {
        *out = buf[0];
        memmove(buf, buf + 1, (size_t)(*count - 1) * sizeof(noc_flit_event_t));
        (*count)--;
    }
}

bool noc_detect_saturation(const noc_perf_stats_t *stats, double threshold) {
    return stats->stalled_cycles > (int)((double)stats->total_cycles * threshold);
}

double noc_effective_throughput(const noc_perf_stats_t *stats) {
    return stats->throughput_gbps;
}

double noc_injection_rate_normalized(double rate, int num_nodes) {
    return rate / (double)num_nodes;
}

void noc_perf_dump(const noc_perf_stats_t *stats) {
    printf("Performance: pkts=%.0f flits=%.0f hops=%.0f\n", stats->total_packets,
        stats->total_flits, stats->total_hops);
    printf("  Latency: avg=%.2f min=%.2f max=%.2f cycles\n",
        stats->avg_latency, stats->min_latency, stats->max_latency);
    printf("  Throughput: %.4f Gbps  InjRate: %.4f\n",
        stats->throughput_gbps, stats->injection_rate);
    printf("  Stalled: %d/%d cycles  Saturation: %s\n",
        stats->stalled_cycles, stats->total_cycles,
        noc_detect_saturation(stats, 0.5) ? "YES" : "no");
}

const char *noc_traffic_pattern_name(noc_traffic_pattern_t pat) {
    static const char *names[] = { "Uniform", "Transpose", "BitComp", "Tornado",
                                    "Neighbor", "HotSpot" };
    return (pat < NOC_TRAFFIC_COUNT) ? names[pat] : "?";
}
