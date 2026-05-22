#include "noc_perf.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void perf_init(PerfStats *ps) {
    memset(ps, 0, sizeof(*ps));
}

int perf_inject(PerfStats *ps, int src, int dst, int size, int cycle) {
    if (ps->packet_count >= MAX_PACKETS) return -1;
    Packet *p = &ps->packets[ps->packet_count];
    p->id = ps->packet_count; p->src = src; p->dst = dst; p->size = size;
    p->injected_cycle = cycle; p->delivered_cycle = -1; p->hops = 0; p->delivered = false;
    return ps->packet_count++;
}

bool perf_deliver(PerfStats *ps, uint32_t id, int cycle, int hops) {
    if (id >= (uint32_t)ps->packet_count) return false;
    Packet *p = &ps->packets[id];
    if (p->delivered) return false;
    p->delivered = true; p->delivered_cycle = cycle; p->hops = hops;
    ps->delivered_count++;
    int lat = cycle - p->injected_cycle;
    ps->total_latency += lat; ps->total_hops += hops;
    ps->avg_latency = ps->delivered_count > 0 ? ps->total_latency / ps->delivered_count : 0;
    ps->avg_hops = ps->delivered_count > 0 ? ps->total_hops / ps->delivered_count : 0;
    return true;
}

void perf_simulate(PerfStats *ps, int total_cycles, int link_width) {
    if (total_cycles <= 0) return;
    int total_flits = 0;
    for (int i = 0; i < ps->packet_count; i++) total_flits += ps->packets[i].size;
    ps->throughput = (double)total_flits / total_cycles * link_width;
}

void perf_latency_throughput(PerfStats *ps, double *avg_lat, double *throughput) {
    *avg_lat = ps->avg_latency; *throughput = ps->throughput;
}

void perf_saturation_point(PerfStats *ps, double *sat_injection_rate) {
    *sat_injection_rate = ps->throughput > 0 ? ps->throughput * 0.75 : 0;
    (void)ps;
}

void perf_report(PerfStats *ps) {
    printf("=== NoC Performance Report ===\n");
    printf("  Packets: %d injected, %d delivered, %d dropped\n", ps->packet_count, ps->delivered_count, ps->dropped_count);
    printf("  Avg latency: %.2f cycles\n", ps->avg_latency);
    printf("  Avg hops: %.2f\n", ps->avg_hops);
    printf("  Throughput: %.4f flits/cycle\n", ps->throughput);
}
