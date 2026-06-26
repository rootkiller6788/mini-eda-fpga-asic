/**
 * ex03_perf_model.c ? L6 Canonical: NoC Performance Analysis
 *
 * Demonstrates analytical performance modeling:
 *   - Zero-load latency computation
 *   - Saturation throughput estimation
 *   - Hot-spot throughput degradation
 *   - Throughput-latency curve via rate sweep
 */

#include <stdio.h>
#include <stdlib.h>
#include "noc_topology.h"
#include "noc_perf.h"
#include "noc_routing.h"

int main(void) {
    printf("???????????????????????????????????????????\n");
    printf("  Example 03: NoC Performance Analysis\n");
    printf("???????????????????????????????????????????\n\n");

    /* ??? Analytical Models ??????????????????????????????????????? */

    printf("??? Analytical Models ???\n\n");

    int32_t mesh_sizes[] = {2, 4, 8, 16};
    int32_t ns;
    for (ns = 0; ns < 4; ns++) {
        int32_t k = mesh_sizes[ns];
        noc_topology_t *mesh = noc_topo_create_mesh_2d(k);
        if (!mesh) continue;

        int32_t avg_h = (int32_t)noc_topo_avg_distance(mesh);
        double d0 = noc_zero_load_latency(avg_h, 5, 128, 1e9);
        double thr = noc_ideal_throughput(avg_h, 128, 1e9);
        int32_t bisect = noc_topo_bisection_width(mesh);

        printf("  %2d?%2d mesh (N=%3d):  Diam=%d, Avg H=%.1f, "
               "BisectBW=%d, D0=%.1f cyc, ?_ideal=%.3f flit/node/cyc\n",
               k, k, mesh->num_nodes, noc_topo_diameter(mesh),
               noc_topo_avg_distance(mesh), bisect, d0, thr);

        noc_topo_free(mesh);
    }

    /* ??? Hot-spot Analysis ??????????????????????????????????????? */

    printf("\n--- Hot-Spot Throughput Degradation ---\n\n");
    printf("  Dally (IEEE TC 1990): Tree saturation model\n");
    printf("  Theta_sat(h) = Theta_sat(0) / (1 + h * (N - 1) / 2)\n\n");

    double base = noc_ideal_throughput(4, 128, 1e9);
    printf("  Base throughput (uniform, 4x4 mesh): Theta_0 = %.3f\n", base);
    printf("  +----------+----------+-------------+\n");
    printf("  | Hotspot%% | Theta(h) | Degradation |\n");
    printf("  +----------+----------+-------------+\n");

    double h_vals[] = {0.01, 0.05, 0.10, 0.20, 0.50};
    int32_t hi;
    for (hi = 0; hi < 5; hi++) {
        double h = h_vals[hi];
        double thr_h = noc_hotspot_throughput(base, h, 16);
        double degradation = (base - thr_h) / base * 100.0;
        printf("  | %6.0f%%   | %8.3f | %7.1f%%    |\n",
               h * 100, thr_h, degradation);
    }
    printf("  +----------+----------+-------------+\n");

    /* ??? Rate Sweep ?????????????????????????????????????????????? */
    printf("\n??? Throughput-Latency Sweep (4?4 mesh) ???\n\n");
    printf("  Running sweep with 5 rate points...\n");

    double rates[5], latencies[5], throughputs[5];
    noc_simulator_sweep_rate(rates, latencies, throughputs, 5,
                             4, TRAFFIC_UNIFORM, 4, ROUTE_XY,
                             10, 50);

    printf("  +----------+----------+--------------+\n");
    printf("  | Inj Rate | Latency  | Acc.Thr.     |\n");
    printf("  +----------+----------+--------------+\n");
    int32_t p;
    for (p = 0; p < 5; p++) {
        printf("  | %8.4f | %8.2f | %12.4f |\n",
               rates[p], latencies[p], throughputs[p]);
    }
    printf("  +----------+----------+--------------+\n");

    /* --- Jain's Fairness --- */
    printf("\n--- Jain's Fairness Index ---\n\n");

    double perfect[] = {0.5, 0.5, 0.5, 0.5};
    double uneven[]  = {0.9, 0.3, 0.1, 0.05};
    double unfair[]  = {1.0, 0.0, 0.0, 0.0};

    printf("  Perfectly balanced:  J = %.4f\n", noc_jain_fairness(perfect, 4));
    printf("  Uneven:              J = %.4f\n", noc_jain_fairness(uneven, 4));
    printf("  Completely unfair:   J = %.4f\n", noc_jain_fairness(unfair, 4));

    printf("\n  Jain et al. (1984): J ? [1/n, 1.0]\n");
    printf("  Perfect fairness = 1.0, Minimum = 1/n = 0.25\n");

    return 0;
}
