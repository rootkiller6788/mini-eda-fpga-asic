#ifndef NOC_PERF_H
#define NOC_PERF_H
#include <stdbool.h>
#include <stdint.h>

#define MAX_PACKETS 256

typedef struct {
    uint32_t id; int src, dst; int size; /* flits */
    int injected_cycle; int delivered_cycle;
    int hops; bool delivered;
} Packet;

typedef struct {
    Packet packets[MAX_PACKETS]; int packet_count;
    double total_latency; double total_hops;
    int delivered_count; int dropped_count;
    double throughput; /* flits/cycle */
    double avg_latency; double avg_hops;
} PerfStats;

void perf_init(PerfStats *ps);
int  perf_inject(PerfStats *ps, int src, int dst, int size, int cycle);
bool perf_deliver(PerfStats *ps, uint32_t id, int cycle, int hops);
void perf_simulate(PerfStats *ps, int total_cycles, int link_width);
void perf_latency_throughput(PerfStats *ps, double *avg_lat, double *throughput);
void perf_saturation_point(PerfStats *ps, double *sat_injection_rate);
void perf_report(PerfStats *ps);
#endif
