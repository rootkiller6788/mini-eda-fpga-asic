/**
 * bench_throughput.c ? NoC Throughput Benchmark
 *
 * Measures throughput-latency characteristics for different
 * routing algorithms under uniform random traffic.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "noc_topology.h"
#include "noc_perf.h"
#include "noc_routing.h"

int main(void) {
    printf("=== NoC Throughput Benchmark ===\n\n");

    int32_t k = 4;
    noc_topology_t *mesh = noc_topo_create_mesh_2d(k);
    if (!mesh) { printf("ERROR: topology\n"); return 1; }

    noc_route_algo_t algos[] = {ROUTE_XY, ROUTE_WEST_FIRST, ROUTE_ODD_EVEN};
    const char *names[] = {"XY", "West-First", "Odd-Even"};
    int32_t na = 3;

    int32_t ai;
    for (ai = 0; ai < na; ai++) {
        printf("\n??? Algorithm: %s ???\n", names[ai]);

        noc_simulator_t sim;
        noc_simulator_init(&sim, mesh, TRAFFIC_UNIFORM, 4, 0.05,
                           algos[ai], (uint32_t)ai);

        clock_t start = clock();
        noc_simulator_run(&sim, 200);
        clock_t end = clock();

        noc_perf_metrics_t m = noc_simulator_collect_metrics(&sim);
        noc_perf_print(&m);

        double ms = (double)(end - start) * 1000.0 / (double)CLOCKS_PER_SEC;
        printf("  Simulation time: %.2f ms\n", ms);

        noc_simulator_destroy(&sim);
    }

    noc_topo_free(mesh);
    return 0;
}
