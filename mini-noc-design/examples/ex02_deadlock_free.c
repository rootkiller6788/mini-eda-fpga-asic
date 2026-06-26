/**
 * ex02_deadlock_free.c ? L6 Canonical: Deadlock-Free Routing Verification
 *
 * Verifies that various turn-model routing algorithms produce
 * deadlock-free Channel Dependency Graphs on a 4?4 mesh.
 *
 * Demonstrates:
 *   - Glass & Ni Turn Model (1992)
 *   - Duato's deadlock-freedom theorem (1993)
 *   - CDG cycle detection
 *   - Kahn's topological sort
 */

#include <stdio.h>
#include <stdlib.h>
#include "noc_topology.h"
#include "noc_routing.h"
#include "noc_deadlock.h"

static const char *algo_names[] = {
    "XY", "YX", "West-First", "North-Last",
    "Negative-First", "Odd-Even", "Adaptive", "Randomized"
};

int main(void) {
    printf("???????????????????????????????????????????\n");
    printf("  Example 02: Deadlock-Free Routing Check\n");
    printf("???????????????????????????????????????????\n\n");

    int32_t k = 4;

    printf("??? Turn Model Restriction Table ???\n\n");
    printf("  Algorithm         ES EN WS WN SE SW NE NW 180  Status\n");
    printf("  ????????????????? ?????????????????????????  ??????\n");

    noc_route_algo_t algo;
    for (algo = 0; algo < ROUTE_COUNT; algo++) {
        const noc_turn_table_t *tbl = noc_turn_model_get(algo);
        if (!tbl) continue;

        printf("  %-16s  ", algo_names[algo]);
        noc_turn_t t;
        for (t = 0; t < TURN_180; t++) {
            printf("%c ", tbl->allowed[t] ? '?' : '?');
        }
        printf(" %c  ", tbl->allowed[TURN_180] ? '?' : '?');

        /* Build CDG and check for cycles */
        noc_channel_dep_graph_t cdg = noc_cdg_build(algo, k);
        int has_cycle = noc_cdg_has_cycle(&cdg);
        printf("%s", has_cycle ? "? DEADLOCK" : "? DF");

        /* Verify with channel numbering theorem */
        int numbering_ok = noc_channel_numbering_exists(&cdg);
        printf(" [%s]\n", numbering_ok ? "numbered" : "no numbering");
    }

    printf("\n??? Channel Dependency Graph Analysis ???\n\n");

    /* Compare XY vs West-First CDG */
    noc_channel_dep_graph_t cdg_xy = noc_cdg_build(ROUTE_XY, k);
    noc_channel_dep_graph_t cdg_wf = noc_cdg_build(ROUTE_WEST_FIRST, k);

    printf("  XY CDG:  %d edges, %d channels\n", cdg_xy.num_edges, cdg_xy.num_channels);
    printf("  WF CDG:  %d edges, %d channels\n", cdg_wf.num_edges, cdg_wf.num_channels);

    /* Topological sort XY CDG */
    int32_t sorted[256];
    int32_t sorted_count = noc_cdg_topological_sort(&cdg_xy, sorted, 256);
    printf("\n  XY Topological Sort: %d channels sorted %s\n",
           sorted_count,
           (sorted_count >= 0) ? "?" : "? (cycle)");

    /* Verify Dally & Seitz theorem:
     * A routing algorithm is deadlock-free iff there exists a
     * numbering of channels such that every route traverses
     * channels in strictly increasing order. */
    printf("\n??? Dally & Seitz Theorem Verification ???\n\n");
    printf("  Theorem (IEEE TC 1987):\n");
    printf("  \"A routing algorithm is deadlock-free iff there\n");
    printf("   exists a numbering of channels such that every\n");
    printf("   route traverses channels in strictly increasing order.\"\n\n");

    printf("  Verification for XY routing on %dx%d mesh:\n", k, k);
    printf("  - Channel numbering exists: %s\n",
           noc_channel_numbering_exists(&cdg_xy) ? "YES ?" : "NO");
    printf("  - CDG is a DAG (topologically sortable): %s\n",
           (sorted_count >= 0) ? "YES ?" : "NO");

    printf("\n??? Duato's Theorem Extension ???\n\n");
    printf("  Duato (IEEE TPDS 1993):\n");
    printf("  \"Adaptive routing is deadlock-free if there exists\n");
    printf("   a connected deadlock-free routing subfunction that\n");
    printf("   acts as an escape path.\"\n\n");

    /* For West-First, verify it's deadlock-free and check escape */
    int wf_df = noc_turn_model_is_deadlock_free(noc_turn_model_get(ROUTE_WEST_FIRST), k);
    printf("  West-First deadlock-free: %s\n", wf_df ? "YES ?" : "NO");

    return 0;
}
