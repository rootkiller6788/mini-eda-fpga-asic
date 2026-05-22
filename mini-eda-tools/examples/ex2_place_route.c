#include "place_route.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== EDA Place & Route Example ===\n\n");

    place_instance_t inst;
    pr_create_place(&inst, 20);
    pr_set_die(&inst, 100.0, 100.0, 0.5, 1.8, 6);

    for (int i = 0; i < 20; i++) {
        pr_add_place_cell(&inst, i, 8.0 + (i % 3) * 2.0,
                          4.0 + (i / 5) * 0.5, (i > 15));
    }

    pr_set_fixed_cell(&inst, 0, 10.0, 10.0);
    pr_set_fixed_cell(&inst, 1, 90.0, 90.0);

    for (int i = 2; i < 18; i++) {
        pr_add_place_net(&inst, i, i + 1, 1.0);
    }
    pr_add_place_net(&inst, 0, 2, 2.0);
    pr_add_place_net(&inst, 5, 10, 1.5);
    pr_add_place_net(&inst, 8, 15, 1.0);

    place_result_t pr;
    inst.params.algo = PLACE_QUADRATIC;
    inst.params.max_iter = 50;

    printf("Running global placement (quadratic)...\n");
    pr_run_global_place(&inst, &pr);
    pr_print_place_result(&pr);

    printf("\nLegalizing...\n");
    pr_legalize(&inst);
    printf("After legalization HPWL: %.2f um\n", pr_calc_hpwl(&inst));

    printf("\nDetailed placement...\n");
    pr_detailed_place(&inst, &pr);
    printf("After detailed HPWL: %.2f um\n", pr_calc_hpwl(&inst));

    printf("\nCreating congestion map...\n");
    congestion_map_t cmap;
    pr_create_congestion_map(&inst, &cmap);
    pr_print_congestion_map(&cmap);

    printf("\nRunning maze routing...\n");
    route_result_t rr;
    pr_route_maze(&inst, &rr);
    pr_print_route_result(&rr);

    printf("\nAdding redundant vias...\n");
    for (int i = 0; i < rr.num_nets && i < 5; i++) {
        pr_add_redundant_via(&rr, i);
    }
    printf("After redundant vias: vias=%d\n", pr_total_vias(&rr));

    printf("\nExporting DEF...\n");
    pr_export_def(&inst, "build/placed.def");
    printf("Placed DEF written to build/placed.def\n");

    pr_free_congestion_map(&cmap);
    for (int i = 0; i < rr.num_nets; i++) {
        free(rr.nets[i].seg_x);
        free(rr.nets[i].seg_y);
        free(rr.nets[i].seg_layer);
    }
    free(rr.nets);
    pr_free_place(&inst);

    return 0;
}
