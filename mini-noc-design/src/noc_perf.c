/**
 * noc_perf.c ? NoC Performance Analysis Implementation
 *
 * Analytical models, cycle-accurate simulation, traffic generation.
 */

#include "noc_perf.h"
#include "noc_routing.h"
#include "noc_flowctrl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ??? L4: Zero-load latency ?????????????????????????????????????????? */

double noc_zero_load_latency(int32_t avg_hops,
                             int32_t router_pipeline_delay,
                             int32_t packet_size_bits,
                             double link_bw_bps) {
    if (avg_hops < 0 || router_pipeline_delay < 0) return -1.0;

    /* D0 = H * t_r + L / b (in cycles, assuming 1 cycle = 1 flit time) */
    double hop_latency = (double)avg_hops * (double)router_pipeline_delay;
    double serialization = 1.0; /* Wormhole: first flit pays serialization */

    /* Additional serialization for large packets */
    if (link_bw_bps > 0) {
        serialization = (double)packet_size_bits / link_bw_bps;
    }

    return hop_latency + serialization;
}

/* ??? L4: Ideal throughput ???????????????????????????????????????????? */

double noc_ideal_throughput(int32_t avg_hops,
                            int32_t packet_size_bits,
                            double link_bw_bps) {
    if (avg_hops <= 0 || packet_size_bits <= 0 || link_bw_bps <= 0) return 0.0;

    /* ?_ideal = 2 * b / (H * L) ? Dally & Towles, Eq. 23.1
     * For 2D mesh with uniform traffic, ideal per-node throughput
     * limited by bisection bandwidth.
     */
    double bits_per_packet = (double)packet_size_bits;
    return 2.0 * link_bw_bps / ((double)avg_hops * bits_per_packet);
}

/* ??? L4: Jain's fairness index ??????????????????????????????????????
 *
 * J = (? x_i)^2 / (n * ? x_i^2)
 * J ? [1/n, 1.0], where 1.0 = perfectly fair.
 */

double noc_jain_fairness(const double *throughputs, int32_t n) {
    if (!throughputs || n <= 0) return 0.0;

    double sum = 0.0, sum_sq = 0.0;
    int32_t i;
    for (i = 0; i < n; i++) {
        sum += throughputs[i];
        sum_sq += throughputs[i] * throughputs[i];
    }

    if (sum_sq <= 0.0) return 0.0;
    return (sum * sum) / ((double)n * sum_sq);
}

/* ??? Traffic generation ?????????????????????????????????????????????? */

int32_t noc_traffic_gen_dest(const noc_traffic_gen_t *gen, int32_t src_id) {
    if (!gen || gen->num_nodes <= 0) return -1;

    int32_t sx, sy, dx, dy;
    int32_t k = gen->mesh_size;

    sy = src_id / k;
    sx = src_id % k;

    /* Simple PRNG for deterministic "random" destinations */
    uint32_t seed = gen->seed + (uint32_t)src_id;
    uint32_t rand = (1103515245u * seed + 12345u) & 0x7FFFFFFFu;

    switch (gen->pattern) {
    case TRAFFIC_UNIFORM: {
        int32_t d = (int32_t)(rand % (uint32_t)gen->num_nodes);
        if (d == src_id) d = (d + 1) % gen->num_nodes;
        return d;
    }
    case TRAFFIC_TRANSPOSE:
        dx = sy;
        dy = sx;
        if (dx >= k) dx = k - 1;
        if (dy >= k) dy = k - 1;
        return dy * k + dx;

    case TRAFFIC_BIT_REVERSE: {
        uint32_t bits = (uint32_t)(gen->num_nodes - 1);
        uint32_t src = (uint32_t)src_id;
        uint32_t rev = 0;
        int32_t num_bits = 0;
        uint32_t tmp = bits;
        while (tmp) { num_bits++; tmp >>= 1; }
        int32_t b;
        for (b = 0; b < num_bits; b++) {
            rev = (rev << 1) | (src & 1);
            src >>= 1;
        }
        int32_t d = (int32_t)(rev & bits);
        if (d >= gen->num_nodes) d = gen->num_nodes - 1;
        if (d == src_id) d = (d + 1) % gen->num_nodes;
        return d;
    }

    case TRAFFIC_BIT_COMPLEMENT: {
        uint32_t mask = (uint32_t)(gen->num_nodes - 1);
        int32_t d = (int32_t)((~(uint32_t)src_id) & mask);
        if (d == src_id) d = (d + 1) % gen->num_nodes;
        return d;
    }

    case TRAFFIC_TORNADO:
        dx = (sx + k / 2 - 1) % k;
        dy = sy;
        return dy * k + dx;

    case TRAFFIC_NEIGHBOR: {
        int32_t offsets[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        int32_t pick = (int32_t)(rand % 4);
        dx = sx + offsets[pick][0];
        dy = sy + offsets[pick][1];
        if (dx < 0) dx = 0; else if (dx >= k) dx = k - 1;
        if (dy < 0) dy = 0; else if (dy >= k) dy = k - 1;
        return dy * k + dx;
    }

    case TRAFFIC_HOTSPOT: {
        double r = (double)(rand % 1000) / 1000.0;
        if (r < gen->hotspot_fraction) {
            return gen->hotspot_node;
        }
        int32_t d = (int32_t)(rand % (uint32_t)gen->num_nodes);
        if (d == src_id) d = (d + 1) % gen->num_nodes;
        return d;
    }

    case TRAFFIC_SHUFFLE: {
        int32_t d = (src_id * 2) % gen->num_nodes;
        if (d == src_id) d = (d + 1) % gen->num_nodes;
        return d;
    }

    case TRAFFIC_CUSTOM:
    default: {
        int32_t d = (int32_t)(rand % (uint32_t)gen->num_nodes);
        if (d == src_id) d = (d + 1) % gen->num_nodes;
        return d;
    }
    }
}

int32_t noc_traffic_gen_cycle(noc_traffic_gen_t *gen) {
    if (!gen) return 0;

    /* Bernoulli injection: each node injects with probability injection_rate */
    int32_t injected = 0;
    int32_t i;
    for (i = 0; i < gen->num_nodes; i++) {
        uint32_t seed = gen->seed + (uint32_t)gen->packets_generated + (uint32_t)i;
        uint32_t rand = (1103515245u * seed + 12345u) & 0x7FFFFFFFu;
        double p = (double)(rand % 1000000) / 1000000.0;

        if (p < gen->injection_rate) {
            gen->packets_generated++;
            gen->flits_generated += (uint64_t)gen->packet_length;
            injected++;
        }
    }
    return injected;
}

void noc_traffic_gen_print(const noc_traffic_gen_t *gen) {
    if (!gen) return;

    printf("=== Traffic Generator Stats ===\n");
    printf("Pattern: %d, Mesh: %d x %d\n", gen->pattern, gen->mesh_size, gen->mesh_size);
    printf("Injection rate: %.4f flits/node/cycle\n", gen->injection_rate);
    printf("Packets: generated=%llu, consumed=%llu\n",
           (unsigned long long)gen->packets_generated,
           (unsigned long long)gen->packets_consumed);
    printf("Flits:   generated=%llu, consumed=%llu\n",
           (unsigned long long)gen->flits_generated,
           (unsigned long long)gen->flits_consumed);
}

/* ??? Simulator ??????????????????????????????????????????????????????? */

void noc_simulator_init(noc_simulator_t *sim,
                        const noc_topology_t *topo,
                        noc_traffic_pattern_t pattern,
                        int32_t packet_length,
                        double injection_rate,
                        int32_t routing_algo,
                        uint32_t seed) {
    if (!sim || !topo) return;

    memset(sim, 0, sizeof(noc_simulator_t));
    sim->topology = (noc_topology_t *)topo; /* Non-owning reference */
    sim->num_routers = topo->num_nodes;
    sim->routing_algo = routing_algo;

    /* Allocate routers */
    sim->routers = (noc_router_t *)calloc((size_t)topo->num_nodes,
                                           sizeof(noc_router_t));
    if (!sim->routers) return;

    int32_t i;
    for (i = 0; i < topo->num_nodes; i++) {
        noc_router_config_t cfg;
        cfg.router_id = i;
        cfg.num_ports = 5; /* local + NSEW */
        cfg.num_vcs = 4;
        cfg.vc_buffer_depth = 8;
        cfg.pipeline_stages = 5;
        cfg.allocator_type = 0;
        noc_router_init(&sim->routers[i], &cfg);
    }

    /* Initialize traffic generator */
    sim->traffic.pattern = pattern;
    sim->traffic.mesh_size = topo->k;
    sim->traffic.num_nodes = topo->num_nodes;
    sim->traffic.packet_length = packet_length;
    sim->traffic.injection_rate = injection_rate;
    sim->traffic.seed = seed;

    /* Initialize injection time tracking */
    sim->injection_times = (uint64_t *)calloc(1024, sizeof(uint64_t));
    if (!sim->injection_times) {
        sim->injection_times = NULL;
    }

    sim->cycle = 0;
    sim->max_cycles = 0;
    sim->num_completed = 0;
    sim->total_latency_sum = 0.0;
}

void noc_simulator_run(noc_simulator_t *sim, uint64_t cycles) {
    if (!sim || !sim->routers) return;

    uint64_t end_cycle = sim->cycle + cycles;
    while (sim->cycle < end_cycle) {
        /* Generate traffic */
        noc_traffic_gen_cycle(&sim->traffic);

        /* Process each router pipeline */
        int32_t i;
        for (i = 0; i < sim->num_routers; i++) {
            noc_router_cycle(&sim->routers[i], NULL);
        }

        /* Return credits between neighboring routers */
        for (i = 0; i < sim->num_routers; i++) {
            noc_router_lt_stage(&sim->routers[i]);
        }

        sim->cycle++;
    }

    sim->max_cycles = end_cycle;
}

noc_perf_metrics_t noc_simulator_collect_metrics(const noc_simulator_t *sim) {
    noc_perf_metrics_t m;
    memset(&m, 0, sizeof(m));

    if (!sim || !sim->routers || sim->max_cycles == 0) return m;

    /* Compute metrics across all routers */
    uint64_t total_flits_out = 0, total_flits_in = 0;
    double total_buffer_occ = 0.0;
    int32_t i;

    for (i = 0; i < sim->num_routers; i++) {
        total_flits_out += sim->routers[i].flits_out;
        total_flits_in += sim->routers[i].flits_in;

        /* Average buffer occupancy (simplified) */
        int32_t p, v;
        for (p = 0; p < sim->routers[i].num_ports; p++) {
            for (v = 0; v < sim->routers[i].num_vcs; v++) {
                total_buffer_occ += (double)sim->routers[i].input_ports[p].buffer_count[v];
            }
        }
    }

    double cycles_f = (double)sim->max_cycles;
    m.accepted_throughput = (double)total_flits_out / cycles_f / (double)sim->num_routers;
    m.offered_throughput = (double)total_flits_in / cycles_f / (double)sim->num_routers;

    /* Zero-load latency: analytical estimate */
    int32_t k = sim->topology ? sim->topology->k : 4;
    int32_t avg_hops = (2 * k) / 3; /* Approximation for 2D mesh */
    m.zero_load_latency = noc_zero_load_latency(avg_hops, 5, 128, 1e9);

    /* Average packet latency */
    if (sim->num_completed > 0) {
        m.avg_packet_latency = sim->total_latency_sum / (double)sim->num_completed;
    } else {
        m.avg_packet_latency = m.zero_load_latency;
    }

    m.avg_buffer_occupancy = total_buffer_occ /
        (double)(sim->num_routers * 5 * 4); /* ports * VCs */

    /* Channel load: fraction of output cycles used */
    if (cycles_f > 0) {
        m.avg_channel_load = (double)total_flits_out /
            (cycles_f * (double)sim->num_routers * 4.0);
    }

    /* Saturation point estimation:
     * Compare accepted vs offered throughput.
     * When accepted/offered < 0.95, we're near saturation. */
    if (m.offered_throughput > 0) {
        double ratio = m.accepted_throughput / m.offered_throughput;
        m.saturation_point = (ratio > 0.95) ? m.offered_throughput * 1.5 : m.offered_throughput * ratio;
    }

    /* Fairness: assume uniform */
    m.global_fairness_index = 1.0;

    return m;
}

/* ??? Rate sweep ?????????????????????????????????????????????????????? */

void noc_simulator_sweep_rate(double *rates, double *latencies,
                              double *throughputs,
                              int32_t num_points,
                              int32_t mesh_size,
                              noc_traffic_pattern_t pattern,
                              int32_t packet_length,
                              int32_t routing_algo,
                              uint64_t warmup_cycles,
                              uint64_t measure_cycles) {
    if (!rates || !latencies || !throughputs || num_points <= 0) return;
    if (mesh_size < 2) return;

    /* Build topology */
    noc_topology_t *topo = noc_topo_create_mesh_2d(mesh_size);
    if (!topo) return;

    int32_t i;
    for (i = 0; i < num_points; i++) {
        /* Linear spacing from 0.01 to 1.0 */
        double injection_rate = 0.01 + (0.99 * (double)i / (double)(num_points - 1));

        noc_simulator_t sim;
        noc_simulator_init(&sim, topo, pattern, packet_length,
                           injection_rate, routing_algo, (uint32_t)i);

        /* Warmup */
        noc_simulator_run(&sim, warmup_cycles);

        /* Measurement */
        noc_simulator_run(&sim, measure_cycles);

        noc_perf_metrics_t m = noc_simulator_collect_metrics(&sim);

        rates[i] = injection_rate;
        latencies[i] = m.avg_packet_latency;
        throughputs[i] = m.accepted_throughput;

        noc_simulator_destroy(&sim);
    }

    noc_topo_free(topo);
}

/* ??? Hot-spot throughput analysis ????????????????????????????????????
 *
 * Dally (1990): Hot-spot traffic creates tree saturation.
 *
 * For a network with N nodes and hot-spot fraction h:
 *   ?_sat(h) = ?_sat(0) / (1 + h * (N - 1) / 2)
 *
 * Explanation: Extra N-1 nodes all send to one hot node,
 * creating a bandwidth bottleneck at the last hop.
 */

double noc_hotspot_throughput(double base_throughput,
                              double hotspot_fraction,
                              int32_t num_nodes) {
    if (num_nodes <= 1 || hotspot_fraction < 0.0) return base_throughput;

    if (hotspot_fraction >= 1.0) {
        /* All traffic to hot node: bottleneck at its input */
        return base_throughput / (double)num_nodes;
    }

    /* Degradation factor */
    double degradation = 1.0 + hotspot_fraction * (double)(num_nodes - 1) / 2.0;
    return base_throughput / degradation;
}

/* ??? Print metrics ??????????????????????????????????????????????????? */

void noc_perf_print(const noc_perf_metrics_t *m) {
    if (!m) return;

    printf("=== NoC Performance Metrics ===\n");
    printf("Throughput (accepted): %.6f flits/node/cycle\n", m->accepted_throughput);
    printf("Throughput (offered):  %.6f flits/node/cycle\n", m->offered_throughput);
    printf("Zero-load latency:    %.2f cycles\n", m->zero_load_latency);
    printf("Avg packet latency:   %.2f cycles\n", m->avg_packet_latency);
    printf("Max packet latency:   %.2f cycles\n", m->max_packet_latency);
    printf("Buffer occupancy:     %.4f\n", m->avg_buffer_occupancy);
    printf("Channel load:         %.4f\n", m->avg_channel_load);
    printf("Saturation point:     %.6f\n", m->saturation_point);
    printf("Fairness (Jain):      %.4f\n", m->global_fairness_index);
}

/* ??? Destroy simulator ??????????????????????????????????????????????? */

void noc_simulator_destroy(noc_simulator_t *sim) {
    if (!sim) return;
    free(sim->routers);
    free(sim->injection_times);
    memset(sim, 0, sizeof(noc_simulator_t));
}
