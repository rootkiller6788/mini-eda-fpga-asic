#include "noc_topology.h"
#include "noc_performance.h"
#include "vc_wormhole.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_CYCLES      10000
#define MAX_PACKETS     5000
#define MAX_TRACES      5000
#define PACKET_SIZE_MIN 4
#define PACKET_SIZE_MAX 16
#define WARMUP_CYCLES   500
#define MEASURE_CYCLES  8500

typedef struct {
    int src;
    int dst;
    int size;
    int created_cycle;
    int delivery_cycle;
    bool active;
} noc_packet_t;

typedef struct {
    noc_topology_t topo;
    noc_packet_t   packets[MAX_PACKETS];
    int            packet_count;
    noc_packet_trace_t traces[MAX_TRACES];
    int            trace_count;
    noc_perf_stats_t stats;
    double         injection_rate;
    double         cycle_time_ns;
    int            flit_width;
    noc_traffic_pattern_t pattern;
    int            cycle;
    unsigned int   seed;
} noc_sim_state_t;

static void sim_init(noc_sim_state_t *s, int w, int h, noc_topology_type_t topo_type,
                      noc_traffic_pattern_t pat, double inj_rate) {
    memset(s, 0, sizeof(*s));
    noc_topology_init(&s->topo, topo_type, w, h);
    noc_perf_init(&s->stats);
    s->injection_rate = inj_rate;
    s->cycle_time_ns  = 1.0;
    s->flit_width     = 128;
    s->pattern        = pat;
    s->cycle          = 0;
    s->seed           = (unsigned int)time(NULL);
}

static unsigned int sim_rand(unsigned int *seed) {
    *seed = *seed * 1103515245u + 12345u;
    return *seed;
}

static bool sim_should_inject(noc_sim_state_t *s) {
    double r = (double)sim_rand((unsigned int *)&s->seed) / (double)0xFFFFFFFFu;
    return r < s->injection_rate;
}

static void sim_inject_packet(noc_sim_state_t *s) {
    if (s->packet_count >= MAX_PACKETS) return;

    noc_packet_t *p = &s->packets[s->packet_count];
    p->src = sim_rand((unsigned int *)&s->seed) % (unsigned int)s->topo.num_routers;
    p->dst = noc_traffic_generate_dst(s->pattern, p->src,
        s->topo.num_routers, s->topo.type, s->topo.width);
    p->size = PACKET_SIZE_MIN + (int)(sim_rand((unsigned int *)&s->seed) % (unsigned int)(PACKET_SIZE_MAX - PACKET_SIZE_MIN + 1));
    p->created_cycle = s->cycle;
    p->delivery_cycle = -1;
    p->active = true;
    s->packet_count++;
}

static void sim_deliver_packet(noc_sim_state_t *s, int pid) {
    noc_packet_t *p = &s->packets[pid];
    p->delivery_cycle = s->cycle;
    p->active = false;

    if (s->trace_count < MAX_TRACES && s->cycle >= WARMUP_CYCLES) {
        noc_coord_t src_c, dst_c;
        noc_coord_from_id(&s->topo, p->src, &src_c);
        noc_coord_from_id(&s->topo, p->dst, &dst_c);

        noc_packet_trace_t *t = &s->traces[s->trace_count++];
        t->src_id           = p->src;
        t->dst_id           = p->dst;
        t->packet_size      = p->size;
        t->injection_cycle  = p->created_cycle;
        t->delivered        = true;
        t->delivery_cycle   = p->delivery_cycle;
        t->hop_count        = noc_manhattan_distance(src_c, dst_c);

        noc_perf_record_packet(&s->stats, t);
    }
}

static void sim_cycle(noc_sim_state_t *s) {
    int i;
    s->cycle++;

    if (s->cycle >= WARMUP_CYCLES && s->cycle < WARMUP_CYCLES + MEASURE_CYCLES) {
        s->stats.total_cycles++;
        if (sim_should_inject(s)) sim_inject_packet(s);
    }

    for (i = 0; i < s->packet_count; i++) {
        noc_packet_t *p = &s->packets[i];
        if (!p->active) continue;

        noc_coord_t src_c, dst_c;
        noc_coord_from_id(&s->topo, p->src, &src_c);
        noc_coord_from_id(&s->topo, p->dst, &dst_c);

        int dist = noc_manhattan_distance(src_c, dst_c);
        int progress = s->cycle - p->created_cycle;

        if (progress >= dist * 3 + p->size) {
            sim_deliver_packet(s, i);
            s->stats.total_flits += (double)p->size;
        }
    }
}

static void sim_finalize(noc_sim_state_t *s) {
    s->stats.total_cycles = MEASURE_CYCLES;
    noc_perf_compute(&s->stats, s->cycle_time_ns, s->flit_width);

    int delivered = 0;
    for (int i = 0; i < s->packet_count; i++) {
        if (!s->packets[i].active) delivered++;
    }

    printf("\nPending at end: %d\n", s->packet_count - delivered);
}

int main(void) {
    printf("=== NoC Performance Simulation Demo ===\n");
    printf("Synthetic traffic sweep across injection rates\n\n");

    const int num_rates = 10;
    double rates[] = {0.01, 0.05, 0.10, 0.15, 0.20, 0.25, 0.30, 0.35, 0.40, 0.50};
    noc_traffic_pattern_t patterns[] = {
        NOC_TRAFFIC_UNIFORM,
        NOC_TRAFFIC_TRANSPOSE,
        NOC_TRAFFIC_BIT_COMPLEMENT
    };
    const char *pat_names[] = {"Uniform", "Transpose", "BitComplement"};
    int num_patterns = 3;
    int w = 4, h = 4;

    printf("%-14s %-12s %12s %12s %12s %12s\n",
        "Pattern", "InjRate", "AvgLatency", "Throughput", "Pkts", "Hops");

    for (int p = 0; p < num_patterns; p++) {
        for (int r = 0; r < num_rates; r++) {
            noc_sim_state_t sim;
            sim_init(&sim, w, h, NOC_TOPO_MESH, patterns[p], rates[r]);

            while (sim.cycle < WARMUP_CYCLES + MEASURE_CYCLES) {
                sim_cycle(&sim);
            }

            sim_finalize(&sim);
            noc_perf_compute(&sim.stats, 1.0, 128);

            printf("%-14s %12.3f %12.2f %12.4f %12.0f %12.0f\n",
                pat_names[p], rates[r],
                sim.stats.avg_latency,
                sim.stats.throughput_gbps,
                sim.stats.total_packets,
                sim.stats.total_hops);
        }
        printf("\n");
    }

    printf("--- Saturation point estimation ---\n");
    for (int p = 0; p < num_patterns; p++) {
        double prev_tput = 0;
        for (int r = 0; r < num_rates; r++) {
            noc_sim_state_t sim;
            sim_init(&sim, w, h, NOC_TOPO_MESH, patterns[p], rates[r]);
            while (sim.cycle < WARMUP_CYCLES + MEASURE_CYCLES) sim_cycle(&sim);
            sim_finalize(&sim);
            noc_perf_compute(&sim.stats, 1.0, 128);

            if (r > 0 && sim.stats.throughput_gbps < prev_tput * 0.95) {
                printf("  %s: saturation at injection rate ~%.3f\n",
                    pat_names[p], rates[r]);
                break;
            }
            prev_tput = sim.stats.throughput_gbps;
        }
    }

    printf("\n--- Detailed stats for Uniform Random at 0.15 ---\n");
    {
        noc_sim_state_t sim;
        sim_init(&sim, w, h, NOC_TOPO_MESH, NOC_TRAFFIC_UNIFORM, 0.15);
        while (sim.cycle < WARMUP_CYCLES + MEASURE_CYCLES) sim_cycle(&sim);
        sim_finalize(&sim);
        noc_perf_compute(&sim.stats, 1.0, 128);
        noc_perf_dump(&sim.stats);
        printf("  Traces recorded: %d\n", sim.trace_count);
    }

    printf("\n--- Topology comparison (4x4) ---\n");
    noc_topology_type_t topos[] = {NOC_TOPO_MESH, NOC_TOPO_TORUS};
    const char *topo_names[] = {"Mesh", "Torus"};
    for (int t = 0; t < 2; t++) {
        noc_sim_state_t sim;
        sim_init(&sim, w, h, topos[t], NOC_TRAFFIC_UNIFORM, 0.20);
        while (sim.cycle < WARMUP_CYCLES + MEASURE_CYCLES) sim_cycle(&sim);
        sim_finalize(&sim);
        noc_perf_compute(&sim.stats, 1.0, 128);
        printf("  %s: avg_lat=%.2f tput=%.4f Gbps inj=%.3f\n",
            topo_names[t], sim.stats.avg_latency,
            sim.stats.throughput_gbps, sim.stats.injection_rate);
    }

    printf("\nDone.\n");
    return 0;
}
