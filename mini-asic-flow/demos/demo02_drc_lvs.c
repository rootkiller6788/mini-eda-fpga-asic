#include "drc_lvs.h"
#include "rtl_to_gds.h"
#include "floorplan_cts.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static void populate_drc_rules(drc_lvs_engine_t *eng, int tech_nm)
{
    double min_w = tech_nm <= 7 ? 40.0 : 64.0;
    double min_s = tech_nm <= 7 ? 40.0 : 64.0;
    double min_enc = tech_nm <= 7 ? 20.0 : 30.0;
    int layer;
    for (layer = 1; layer <= 13; layer++) {
        drc_add_rule(eng, DRC_SPACING, layer, min_w, min_s,
                     layer <= 2 ? DRC_SEVERITY_CRITICAL : DRC_SEVERITY_ERROR);
        drc_add_rule(eng, DRC_WIDTH, layer, min_w, 0.0,
                     DRC_SEVERITY_ERROR);
        if (layer <= 11) {
            drc_add_rule(eng, DRC_ENCLOSURE, layer, 0.0, 0.0,
                         DRC_SEVERITY_ERROR);
            eng->rules[eng->rule_count - 1].min_enclosure_nm = min_enc;
        }
    }
    for (layer = 1; layer <= 2; layer++) {
        drc_add_rule(eng, DRC_AREA, layer, 0.0, 0.0,
                     DRC_SEVERITY_WARNING);
        eng->rules[eng->rule_count - 1].min_area_nm2 = 1000.0;
    }
    for (layer = 3; layer <= 13; layer++) {
        drc_add_rule(eng, DRC_DENSITY, layer, 0.0, 0.0,
                     DRC_SEVERITY_WARNING);
        eng->rules[eng->rule_count - 1].max_density_pct = 70.0;
    }
}

static void run_spacing_checks(drc_lvs_engine_t *eng)
{
    int i;
    double shapes[10][4] = {
        {100.0, 100.0, 200.0, 300.0},
        {210.0, 100.0, 310.0, 300.0},
        {400.0, 200.0, 500.0, 400.0},
        {450.0, 150.0, 550.0, 350.0},
        {600.0, 500.0, 700.0, 600.0},
        {620.0, 510.0, 720.0, 610.0},
        {50.0,  500.0, 150.0, 600.0},
        {300.0, 350.0, 400.0, 450.0},
        {320.0, 370.0, 420.0, 470.0},
        {800.0, 100.0, 900.0, 200.0},
    };
    for (i = 0; i < 9; i++) {
        int j;
        for (j = i + 1; j < 10; j++) {
            drc_check_spacing(eng, 1,
                              shapes[i][0], shapes[i][1],
                              shapes[i][2], shapes[i][3],
                              shapes[j][0], shapes[j][1],
                              shapes[j][2], shapes[j][3]);
        }
    }
}

static void run_width_checks(drc_lvs_engine_t *eng)
{
    int i;
    double wires[8][4] = {
        {100.0, 400.0, 135.0, 500.0},
        {200.0, 200.0, 236.0, 300.0},
        {500.0, 600.0, 540.0, 700.0},
        {700.0, 300.0, 738.0, 400.0},
        {300.0, 100.0, 340.0, 200.0},
        {400.0, 500.0, 442.0, 600.0},
        {600.0, 100.0, 646.0, 200.0},
        {800.0, 600.0, 852.0, 700.0},
    };
    for (i = 0; i < 8; i++) {
        drc_check_width(eng, 1, wires[i][0], wires[i][1],
                        wires[i][2], wires[i][3]);
    }
}

static void run_enclosure_checks(drc_lvs_engine_t *eng)
{
    int i;
    double vias[6][8] = {
        {100.0, 100.0, 50.0, 50.0, 90.0, 90.0, 70.0, 70.0},
        {300.0, 300.0, 60.0, 60.0, 280.0, 280.0, 100.0, 100.0},
        {500.0, 100.0, 50.0, 50.0, 495.0, 95.0,  60.0, 60.0},
        {200.0, 500.0, 80.0, 80.0, 185.0, 485.0, 110.0, 110.0},
        {600.0, 400.0, 45.0, 45.0, 590.0, 390.0, 65.0, 65.0},
        {750.0, 550.0, 55.0, 55.0, 740.0, 540.0, 75.0, 75.0},
    };
    for (i = 0; i < 6; i++) {
        drc_check_enclosure(eng, 1, 2,
                            vias[i][0], vias[i][1], vias[i][2], vias[i][3],
                            vias[i][4], vias[i][5], vias[i][6], vias[i][7]);
    }
}

static void run_density_checks(drc_lvs_engine_t *eng)
{
    drc_check_density(eng, 3, 1000.0, 1000.0, 750000.0);
    drc_check_density(eng, 5, 1000.0, 1000.0, 600000.0);
    drc_check_density(eng, 8, 1000.0, 1000.0, 550000.0);
    drc_check_density(eng, 11, 1000.0, 1000.0, 780000.0);
}

static void build_schematic_netlist(drc_lvs_engine_t *eng)
{
    const char *devs[] = {"MN1","MN2","MN3","MP1","MP2","R1","C1","MN4","MN5","MP3"};
    const char *types[] = {"NMOS","NMOS","NMOS","PMOS","PMOS","RES","CAP","NMOS","NMOS","PMOS"};
    double ws[] = {120.0, 150.0, 100.0, 200.0, 180.0, 1.0, 1.0, 80.0, 90.0, 220.0};
    double ls[] = {20.0, 20.0, 18.0, 20.0, 20.0, 1.0, 1.0, 16.0, 16.0, 20.0};
    int fs[] = {4, 4, 2, 6, 6, 1, 1, 2, 2, 8};
    int i;
    for (i = 0; i < 10; i++) {
        lvs_add_schematic_device(eng, devs[i], types[i], ws[i], ls[i], fs[i]);
    }
    lvs_add_schematic_net(eng, "VDD", 4);
    lvs_add_schematic_net(eng, "VSS", 4);
    lvs_add_schematic_net(eng, "IN1", 2);
    lvs_add_schematic_net(eng, "IN2", 2);
    lvs_add_schematic_net(eng, "OUT", 3);
    lvs_add_schematic_net(eng, "NET_A", 2);
    lvs_add_schematic_net(eng, "NET_B", 1);
}

static void build_layout_netlist(drc_lvs_engine_t *eng)
{
    const char *devs[] = {"MN1","MN2","MN3","MP1","MP2","R1","C1","MN4","MN5","MP3"};
    const char *types[] = {"NMOS","NMOS","NMOS","PMOS","PMOS","RES","CAP","NMOS","NMOS","PMOS"};
    double ws[] = {120.0, 150.0, 100.0, 200.0, 182.0, 1.0, 1.0, 80.0, 90.0, 220.0};
    double ls[] = {20.0, 20.0, 18.0, 20.0, 20.0, 1.0, 1.0, 16.0, 16.0, 20.0};
    int fs[] = {4, 4, 2, 6, 6, 1, 1, 2, 2, 8};
    int i;
    for (i = 0; i < 10; i++) {
        lvs_add_layout_device(eng, devs[i], types[i], ws[i], ls[i], fs[i]);
    }
    lvs_add_layout_net(eng, "VDD", 4);
    lvs_add_layout_net(eng, "VSS", 4);
    lvs_add_layout_net(eng, "IN1", 2);
    lvs_add_layout_net(eng, "IN2", 2);
    lvs_add_layout_net(eng, "OUT", 3);
    lvs_add_layout_net(eng, "NET_A", 2);
}

static void run_antenna_checks(drc_lvs_engine_t *eng)
{
    int i;
    double gate_areas[] = {5000.0, 12000.0, 8000.0, 3000.0, 15000.0,
                           2000.0, 6000.0, 9500.0, 4000.0, 11000.0};
    double max_ratios[] = {400.0, 400.0, 400.0, 400.0, 400.0,
                           400.0, 400.0, 400.0, 400.0, 400.0};
    for (i = 0; i < 10; i++) {
        antenna_check(eng, i + 1, gate_areas[i], max_ratios[i]);
    }
}

static void run_erc_checks(drc_lvs_engine_t *eng)
{
    erc_check_well_ties(eng, 1, -1);
    erc_check_well_ties(eng, 4, -1);
    erc_check_floating(eng, 7);
    erc_check_floating(eng, 8);
    erc_check_hot_well(eng, 3);
    erc_check_hot_well(eng, 9);
}

static void attempt_fixes(drc_lvs_engine_t *eng)
{
    int i;
    for (i = 0; i < eng->violation_count; i++) {
        if (!eng->violations[i].is_fixed) {
            switch (eng->violations[i].type) {
            case DRC_SPACING:
                drc_fix_push_cell(eng, eng->violations[i].id, 10.0, 0.0);
                break;
            case DRC_WIDTH:
                drc_fix_widen_wire(eng, eng->violations[i].id, 48.0);
                break;
            case DRC_ENCLOSURE:
                drc_fix_reduce_spacing(eng, eng->violations[i].id, 1.2);
                break;
            default:
                drc_fix_push_cell(eng, eng->violations[i].id, 5.0, 5.0);
                break;
            }
        }
    }
    for (i = 0; i < eng->antenna_check_count; i++) {
        if (eng->antenna_checks[i].is_violation) {
            if (i % 2 == 0) {
                antenna_fix_add_diode(eng, eng->antenna_checks[i].id);
            } else {
                antenna_fix_metal_jump(eng, eng->antenna_checks[i].id, 6);
            }
        }
    }
}

int main(void)
{
    drc_lvs_engine_t eng;
    int tech_nm = 5;
    int lvs_errors;

    printf("===========================================\n");
    printf("  DRC / LVS / Antenna / ERC Demo  (%d nm)\n", tech_nm);
    printf("===========================================\n\n");

    drc_lvs_init(&eng);

    printf("--- DRC Rule Setup (%d nm) ---\n", tech_nm);
    populate_drc_rules(&eng, tech_nm);
    printf("  Total rules: %d\n\n", eng.rule_count);

    printf("--- DRC: Spacing Checks ---\n");
    run_spacing_checks(&eng);
    printf("  Spacing violations found: %d\n", eng.violation_count);

    printf("\n--- DRC: Width Checks ---\n");
    run_width_checks(&eng);
    printf("  Total violations (spacing+width): %d\n", eng.violation_count);

    printf("\n--- DRC: Enclosure Checks ---\n");
    run_enclosure_checks(&eng);
    printf("  Total violations: %d\n", eng.violation_count);

    printf("\n--- DRC: Density Checks ---\n");
    run_density_checks(&eng);
    printf("  Total violations: %d\n", eng.violation_count);

    printf("\n--- DRC Summary ---\n");
    printf("  DRC errors: %d\n", eng.total_drc_errors);

    printf("\n--- LVS: Netlist Build ---\n");
    build_schematic_netlist(&eng);
    build_layout_netlist(&eng);
    printf("  Schematic: %d devices, %d nets\n",
           eng.schematic_device_count, eng.schematic_net_count);
    printf("  Layout:    %d devices, %d nets\n",
           eng.layout_device_count, eng.layout_net_count);

    printf("\n--- LVS: Compare ---\n");
    lvs_errors = lvs_compare(&eng);
    printf("  LVS mismatches: %d\n", lvs_errors);
    lvs_report_mismatches(&eng);

    printf("\n--- Antenna Checks ---\n");
    run_antenna_checks(&eng);
    printf("  Antenna violations: %d\n", eng.total_antenna_errors);

    printf("\n--- ERC Checks ---\n");
    run_erc_checks(&eng);
    printf("  ERC violations: %d\n", eng.total_erc_violations);

    printf("\n===========================================\n");
    printf("  Before Fix\n");
    printf("===========================================\n");
    drc_lvs_report(&eng);

    printf("\n--- Applying Fixes ---\n");
    attempt_fixes(&eng);

    drc_lvs_clean(&eng);

    printf("\n--- Re-run DRC After Fixes ---\n");
    run_spacing_checks(&eng);
    run_width_checks(&eng);
    run_enclosure_checks(&eng);
    run_density_checks(&eng);

    printf("\n===========================================\n");
    printf("  After Fix\n");
    printf("===========================================\n");
    drc_lvs_report(&eng);

    printf("\n--- Rect Geometry Helpers ---\n");
    {
        double d = drc_rect_distance(0, 0, 100, 100, 120, 0, 220, 100);
        printf("  Distance (spaced 20nm apart): %.1f nm\n", d);
        d = drc_rect_distance(0, 0, 100, 100, 90, 0, 190, 100);
        printf("  Distance (overlapping):       %.1f nm\n", d);
    }
    {
        int ov = drc_rect_overlap(0, 0, 100, 100, 120, 0, 220, 100);
        printf("  Overlap (spaced):   %s\n", ov ? "YES" : "NO");
        ov = drc_rect_overlap(0, 0, 100, 100, 50, 50, 150, 150);
        printf("  Overlap (covering): %s\n", ov ? "YES" : "NO");
    }

    printf("\n===========================================\n");
    printf("  DRC/LVS Sign-off: %s\n",
           drc_lvs_signoff(&eng) ? "PASS" : "FAIL - fix remaining violations");
    printf("===========================================\n");

    return 0;
}
