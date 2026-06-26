#include "standard_cell.h"
#include "floorplan.h"
#include "power_grid.h"
#include "clock_tree.h"
#include "phys_verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    printf("====== ASIC Flow Demo ======\n\n");
    srand((unsigned)time(NULL));

    printf("--- Standard Cell Library ---\n");
    StdCellLib lib;
    cell_lib_init(&lib, "Nangate45", 45, 1.1);
    cell_lib_add_cell(&lib, "INV_X1", GATE_INV, 0.8, 1.8, 0);
    cell_lib_add_cell(&lib, "NAND2_X1", GATE_NAND2, 1.2, 1.8, 0);
    cell_lib_add_cell(&lib, "NOR2_X1", GATE_NOR2, 1.2, 1.8, 0);
    cell_lib_add_cell(&lib, "DFF_X1", GATE_DFF, 2.0, 1.8, 0);
    cell_lib_print(&lib);

    printf("\n--- Floorplan ---\n");
    Floorplan fp;
    floorplan_init(&fp, 100.0, 100.0);
    floorplan_add_module(&fp, "core", 50, 60, 3000);
    floorplan_add_module(&fp, "cache", 30, 30, 900);
    floorplan_add_module(&fp, "io_ring", 20, 20, 400);
    floorplan_anneal(&fp, 500.0, 0.95, 500);
    floorplan_print(&fp);

    printf("\n--- Power Grid ---\n");
    PowerGrid pg;
    pgrid_init(&pg, 1.1);
    pgrid_add_node(&pg, 0, 0, 0.5, true);
    pgrid_add_node(&pg, 50, 0, 0.3, false);
    pgrid_add_node(&pg, 50, 50, 0.2, false);
    pgrid_add_strap(&pg, 0, 1, 0.1, 0);
    pgrid_add_strap(&pg, 0, 2, 0.15, 0);
    pgrid_analyze_ir(&pg);
    pgrid_report(&pg);

    printf("\n--- Clock Tree ---\n");
    ClockTree ct;
    cts_init(&ct, 10.0);
    cts_add_sink(&ct, 10, 10, 0.1);
    cts_add_sink(&ct, 90, 10, 0.1);
    cts_add_sink(&ct, 10, 90, 0.1);
    cts_add_sink(&ct, 90, 90, 0.1);
    cts_build_htree(&ct, 50, 50, 80);
    cts_insert_buffers(&ct);
    cts_skew_analysis(&ct);
    cts_print(&ct);

    printf("\n--- DRC ---\n");
    DrcChecker drc;
    drc_init(&drc);
    drc_add_rule(&drc, DRC_WIDTH, "M1", 0.1, 10.0);
    drc_add_rule(&drc, DRC_SPACING, "M1", 0.1, 10.0);
    drc_check_width(&drc, "M1", 0.09, 10, 20);
    drc_check_spacing(&drc, "M1", 0.05, 30, 40);
    drc_report(&drc);

    return 0;
}
