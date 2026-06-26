/**
 * mini-eda-tools Static Timing Analysis Demo
 * 演示静态时序分析
 */
#include "static_timing.h"
#include <stdio.h>

int main(void) {
    printf("====== Static Timing Analysis Demo ======\n\n");

    StaGraph g;
    sta_init(&g, 10.0, "clk");

    sta_add_node(&g, "in",  0.0, false, false);
    sta_add_node(&g, "n1",  0.2, false, false);
    sta_add_node(&g, "n2",  0.3, false, false);
    sta_add_node(&g, "n3",  0.2, false, false);
    sta_add_node(&g, "ff1", 0.5, true,  false);
    sta_add_node(&g, "n4",  0.1, false, false);
    sta_add_node(&g, "n5",  0.4, false, false);
    sta_add_node(&g, "out", 0.0, false, false);

    g.nodes[0].is_input = true;
    g.nodes[7].is_output = true;

    sta_add_edge(&g, 0, 1, 0.5, 0.0, 0.0);
    sta_add_edge(&g, 1, 2, 0.8, 0.0, 0.0);
    sta_add_edge(&g, 2, 3, 0.6, 0.0, 0.0);
    sta_add_edge(&g, 3, 4, 0.4, 0.2, 0.1);
    sta_add_edge(&g, 4, 5, 0.3, 0.0, 0.0);
    sta_add_edge(&g, 5, 6, 1.2, 0.0, 0.0);
    sta_add_edge(&g, 6, 7, 0.2, 0.0, 0.0);

    printf("Computing arrival times...\n");
    sta_compute_arrival(&g);
    printf("Computing required times...\n");
    sta_compute_required(&g);
    printf("Computing slacks...\n");
    sta_compute_slack(&g);

    sta_report(&g);

    printf("\n--- Critical Paths ---\n");
    TimingPath paths[4];
    int n_paths = sta_find_critical_paths(&g, paths, 4);
    for (int i = 0; i < n_paths; i++) {
        sta_report_path(&paths[i], &g);
    }

    printf("\nMeets timing: %s\n", sta_meets_timing(&g) ? "YES" : "NO");

    return 0;
}
