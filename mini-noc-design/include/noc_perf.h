/**
 * noc_perf.h ? L4/L5: NoC Performance Analysis
 *
 * Analytical performance models for Network-on-Chip.
 *
 * Standards/Theorems (L4):
 *   - Little's Law: L = ? * W (avg packets = arrival rate * avg time)
 *   - M/D/1 queue model for router input buffers
 *   - Zero-load latency: D0 = H * t_r + t_s + t_w
 *   - Saturation throughput: ?_sat = 2 * b / (H * L)
 *
 * Algorithms (L5):
 *   - Steady-state network simulation (cycle-accurate)
 *   - Hot-spot traffic pattern analysis
 *   - Throughput/latency curve computation
 *
 * Course Mapping:
 *   - CMU 15-418: Parallel Computer Architecture ? interconnection perf
 *   - Stanford CS 315A: performance modeling
 *   - Berkeley CS 267: HPC interconnect analysis
 *   - Georgia Tech CS 6290: HPCA ? network performance
 */

#ifndef NOC_PERF_H
#define NOC_PERF_H

#include <stdint.h>
#include <stddef.h>
#include "noc_topology.h"
#include "noc_router.h"
#include "noc_flowctrl.h"

typedef enum {
    TRAFFIC_UNIFORM      = 0,
    TRAFFIC_TRANSPOSE    = 1,
    TRAFFIC_BIT_REVERSE  = 2,
    TRAFFIC_BIT_COMPLEMENT = 3,
    TRAFFIC_TORNADO      = 4,
    TRAFFIC_NEIGHBOR     = 5,
    TRAFFIC_HOTSPOT      = 6,
    TRAFFIC_SHUFFLE      = 7,
    TRAFFIC_CUSTOM       = 8,
    TRAFFIC_COUNT
} noc_traffic_pattern_t;

typedef struct noc_traffic_gen {
    noc_traffic_pattern_t pattern;
    int32_t               mesh_size;
    int32_t               num_nodes;
    int32_t               packet_length;
    double                injection_rate;
    double                hotspot_fraction;
    int32_t               hotspot_node;
    uint32_t              seed;
    uint64_t              packets_generated;
    uint64_t              flits_generated;
    uint64_t              packets_consumed;
    uint64_t              flits_consumed;
    double                avg_latency;
} noc_traffic_gen_t;

typedef struct noc_simulator {
    noc_topology_t       *topology;
    noc_router_t         *routers;
    int32_t               num_routers;
    noc_traffic_gen_t     traffic;
    noc_flow_controller_t flow_ctrl;
    uint64_t              cycle;
    uint64_t              max_cycles;
    int32_t               routing_algo;
    uint64_t             *injection_times;
    int32_t               num_completed;
    double                total_latency_sum;
} noc_simulator_t;

typedef struct noc_perf_metrics {
    double accepted_throughput;
    double offered_throughput;
    double zero_load_latency;
    double avg_packet_latency;
    double max_packet_latency;
    double latency_stddev;
    double avg_buffer_occupancy;
    double avg_channel_load;
    double saturation_point;
    double global_fairness_index;
} noc_perf_metrics_t;

double noc_zero_load_latency(int32_t avg_hops,
                             int32_t router_pipeline_delay,
                             int32_t packet_size_bits,
                             double link_bw_bps);
double noc_ideal_throughput(int32_t avg_hops,
                            int32_t packet_size_bits,
                            double link_bw_bps);
double noc_jain_fairness(const double *throughputs, int32_t n);
void noc_simulator_init(noc_simulator_t *sim,
                        const noc_topology_t *topo,
                        noc_traffic_pattern_t pattern,
                        int32_t packet_length,
                        double injection_rate,
                        int32_t routing_algo,
                        uint32_t seed);
void noc_simulator_run(noc_simulator_t *sim, uint64_t cycles);
noc_perf_metrics_t noc_simulator_collect_metrics(const noc_simulator_t *sim);
void noc_simulator_sweep_rate(double *rates, double *latencies,
                              double *throughputs,
                              int32_t num_points,
                              int32_t mesh_size,
                              noc_traffic_pattern_t pattern,
                              int32_t packet_length,
                              int32_t routing_algo,
                              uint64_t warmup_cycles,
                              uint64_t measure_cycles);
void noc_perf_print(const noc_perf_metrics_t *m);
void noc_simulator_destroy(noc_simulator_t *sim);
double noc_hotspot_throughput(double base_throughput,
                              double hotspot_fraction,
                              int32_t num_nodes);
int32_t noc_traffic_gen_dest(const noc_traffic_gen_t *gen, int32_t src_id);
int32_t noc_traffic_gen_cycle(noc_traffic_gen_t *gen);
void noc_traffic_gen_print(const noc_traffic_gen_t *gen);

#endif /* NOC_PERF_H */
