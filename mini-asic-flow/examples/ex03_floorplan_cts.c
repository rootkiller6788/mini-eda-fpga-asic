#include "floorplan_cts.h"
#include <stdio.h>

int main(void)
{
    floorplan_t fp;
    clock_tree_t ct;
    int i;

    floorplan_init(&fp, 800.0, 600.0, 9);

    fp.cell_utilization = 0.72;
    floorplan_create_pg_grid(&fp, 2.0, 40.0, 4, 0.75);

    {
        macro_block_t m;
        memset(&m, 0, sizeof(m));
        m.id = 1;
        strncpy(m.name, "SRAM_2MB", sizeof(m.name) - 1);
        m.type           = MACRO_MEMORY;
        m.width_um       = 200.0;
        m.height_um      = 150.0;
        m.x_um           = 10.0;
        m.y_um           = 440.0;
        m.keepout_margin_um = 5.0;
        m.is_locked      = 1;
        floorplan_place_macro(&fp, &m);
    }
    {
        macro_block_t m;
        memset(&m, 0, sizeof(m));
        m.id = 2;
        strncpy(m.name, "SRAM_4MB", sizeof(m.name) - 1);
        m.type           = MACRO_MEMORY;
        m.width_um       = 300.0;
        m.height_um      = 200.0;
        m.x_um           = 220.0;
        m.y_um           = 390.0;
        m.keepout_margin_um = 5.0;
        m.is_locked      = 1;
        floorplan_place_macro(&fp, &m);
    }
    {
        macro_block_t m;
        memset(&m, 0, sizeof(m));
        m.id = 3;
        strncpy(m.name, "PLL_2G", sizeof(m.name) - 1);
        m.type           = MACRO_PLL;
        m.width_um       = 80.0;
        m.height_um      = 60.0;
        m.x_um           = 700.0;
        m.y_um           = 530.0;
        m.keepout_margin_um = 10.0;
        m.is_locked      = 1;
        floorplan_place_macro(&fp, &m);
    }

    floorplan_auto_place_io_ring(&fp, 60.0);
    fp.io_pads[0].type      = IO_POWER;
    fp.io_pads[1].type      = IO_GROUND;
    fp.io_pads[2].type      = IO_CLOCK;

    printf("Macro overlap check: %d\n", floorplan_check_macro_overlap(&fp));
    floorplan_report(&fp);

    printf("\n=== Clock Tree Synthesis ===\n\n");

    clock_tree_init(&ct, CLK_H_TREE);
    clock_tree_add_domain(&ct, "CLK_MAIN", 0.5, 10.0);
    clock_tree_add_domain(&ct, "CLK_PERIPH", 2.0, 50.0);
    clock_tree_add_domain(&ct, "CLK_MEM", 1.0, 20.0);

    clock_tree_build_h_tree(&ct, 0, 400.0, 300.0, 700.0, 500.0, 5);
    clock_tree_build_h_tree(&ct, 1, 100.0, 100.0, 200.0, 150.0, 3);

    clock_tree_build_clock_mesh(&ct, 2, 10.0, 440.0, 800.0, 590.0, 4, 6);

    clock_tree_insert_buffers(&ct, 0, DRIVE_X16, 4);
    clock_tree_insert_buffers(&ct, 1, DRIVE_X8, 2);
    clock_tree_insert_buffers(&ct, 2, DRIVE_X4, 3);

    for (i = 0; i < ct.domain_count; i++) {
        double gs, ls;
        clock_tree_compute_skew(&ct, i, &gs, &ls);
        printf("  %s: global skew=%.1f ps  local skew=%.1f ps  sinks=%d\n",
               ct.domains[i].name, gs, ls, ct.domains[i].sink_count);
    }

    clock_tree_balance_skew(&ct, 0, 5.0);
    clock_tree_report(&ct);

    printf("\nCTS Buffer depth for 1000 sinks, FO4=16: %d\n",
           cts_optimal_buffer_depth(1000, 16.0));

    printf("Chip area: %.1f um2  Utilization: %.1f%%\n",
           floorplan_chip_area(&fp), floorplan_utilization(&fp) * 100.0);

    return 0;
}
