#include "std_cells.h"
#include <stdio.h>

int main(void)
{
    std_cell_lib_t lib;
    std_cell_t cell;
    int i;

    std_cell_lib_init(&lib, "7nm_std_7p5t", 7, 240.0, 9, 0.75);

    for (i = 0; i < VT_COUNT; i++) {
        vt_type_t vt = (vt_type_t)i;
        int d;
        for (d = 0; d < 3; d++) {
            drive_strength_t drv = (drive_strength_t)d;
            char name[32];
            snprintf(name, sizeof(name), "%s_BUF_%s",
                     vt_type_name(vt), drive_strength_name(drv));
            std_cell_init(&cell, name, CELL_COMBINATIONAL, vt, drv, 9);

            std_cell_add_timing_arc(&cell, ARC_CELL_RISE,
                                    5.0 + d * 2.0, 6.0 + d * 2.0,
                                    1.0 + d * 0.5, 2.0 + d * 0.5);
            std_cell_add_timing_arc(&cell, ARC_CELL_FALL,
                                    4.0 + d * 2.0, 5.0 + d * 2.0,
                                    1.0 + d * 0.5, 2.0 + d * 0.5);

            cell.leakage_power_nw = (vt == VT_LVT) ? 50.0 + d * 20.0 :
                                    (vt == VT_SVT) ? 10.0 + d * 5.0  : 2.0 + d * 1.0;
            cell.internal_power_uw = 0.5 + d * 0.3;

            std_cell_lib_add_cell(&lib, &cell);
        }
    }

    for (i = 0; i < VT_COUNT; i++) {
        vt_type_t vt = (vt_type_t)i;
        char name[32];
        snprintf(name, sizeof(name), "%s_INV_X1", vt_type_name(vt));
        std_cell_init(&cell, name, CELL_COMBINATIONAL, vt, DRIVE_X1, 9);
        cell.is_inverting = 1;
        cell.input_count  = 1;
        cell.output_count = 1;
        std_cell_add_timing_arc(&cell, ARC_CELL_RISE, 3.0, 4.0, 0.8, 1.5);
        std_cell_add_timing_arc(&cell, ARC_CELL_FALL, 3.5, 4.5, 0.8, 1.5);
        cell.leakage_power_nw = (vt == VT_LVT) ? 30.0 :
                                (vt == VT_SVT) ? 5.0  : 1.0;
        std_cell_lib_add_cell(&lib, &cell);
    }

    {
        std_cell_init(&cell, "SVT_DFF_X2", CELL_SEQUENTIAL, VT_SVT, DRIVE_X2, 9);
        cell.input_count  = 3;
        cell.output_count = 1;
        std_cell_add_timing_arc(&cell, ARC_SETUP_RISE, 10.0, 10.0, 1.2, 2.0);
        std_cell_add_timing_arc(&cell, ARC_HOLD_RISE, -2.0, -2.0, 1.2, 2.0);
        cell.leakage_power_nw = 8.0;
        std_cell_lib_add_cell(&lib, &cell);
    }

    {
        std_cell_init(&cell, "HVT_CG_X4", CELL_CLOCK_GATE, VT_HVT, DRIVE_X4, 9);
        cell.input_count  = 2;
        cell.output_count = 1;
        std_cell_add_timing_arc(&cell, ARC_CELL_RISE, 15.0, 12.0, 1.5, 3.0);
        std_cell_add_timing_arc(&cell, ARC_CELL_FALL, 14.0, 11.0, 1.5, 3.0);
        cell.leakage_power_nw = 0.5;
        std_cell_lib_add_cell(&lib, &cell);
    }

    std_cell_lib_report(&lib);

    printf("\n=== FinFET Drive Current ===\n");
    for (i = 0; i < VT_COUNT; i++) {
        printf("  %s: %.2f nA\n", vt_type_name((vt_type_t)i),
               finfet_drive_current_nA(8.0, 40.0, 7.0, (vt_type_t)i));
    }

    printf("\n=== Capacitance ===\n");
    printf("  C = %.4f fF\n", finfet_capacitance_ff(8.0, 40.0, 7.0));

    printf("\n=== Lookup ===\n");
    {
        const std_cell_t *c = std_cell_lib_find(&lib, "SVT_INV_X1");
        if (c) {
            std_cell_report(c);
            printf("  Delay (typical): %.2f ps\n",
                   std_cell_get_delay(c, ARC_CELL_RISE, 20.0, 5.0));
            printf("  Power (10%% act, 1GHz): %.3f uW\n",
                   std_cell_get_power(c, 0.1, 1000.0));
        }
    }

    return 0;
}
