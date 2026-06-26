#include "interposer_tech.h"
#include "thermal_power.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    interposer_t ip;
    thermal_power_model_t tm;
    die_geometry_t compute_die, hbm_die, io_die;

    printf("=== Interposer Design Example ===\n\n");

    interposer_init(&ip, INTERPOSER_SILICON, 30.0, 25.0);
    printf("[1] Created silicon interposer: %.1f x %.1f mm\n",
           ip.spec.width_mm, ip.spec.height_mm);

    memset(&compute_die, 0, sizeof(compute_die));
    compute_die.x_um = 2000.0;
    compute_die.y_um = 5000.0;
    compute_die.width_um = 12000.0;
    compute_die.height_um = 10000.0;
    compute_die.thickness_um = 100.0;
    compute_die.power_w = 150.0;
    strncpy(compute_die.name, "Compute-GPU", sizeof(compute_die.name) - 1);

    memset(&hbm_die, 0, sizeof(hbm_die));
    hbm_die.x_um = 16000.0;
    hbm_die.y_um = 5000.0;
    hbm_die.width_um = 8000.0;
    hbm_die.height_um = 6000.0;
    hbm_die.thickness_um = 100.0;
    hbm_die.power_w = 7.5;
    strncpy(hbm_die.name, "HBM3-8Hi", sizeof(hbm_die.name) - 1);

    memset(&io_die, 0, sizeof(io_die));
    io_die.x_um = 2000.0;
    io_die.y_um = 17000.0;
    io_die.width_um = 26000.0;
    io_die.height_um = 4000.0;
    io_die.thickness_um = 100.0;
    io_die.power_w = 25.0;
    strncpy(io_die.name, "IO-Die", sizeof(io_die.name) - 1);

    interposer_place_die(&ip, &compute_die);
    interposer_place_die(&ip, &hbm_die);
    interposer_place_die(&ip, &io_die);
    printf("[2] Placed %u dies on interposer\n", ip.num_dies);

    interposer_route_die_to_die(&ip, 0, 1, 1024, 2);
    printf("[3] Routed HBM->Compute: 1024 signals on RDL layer 2\n");

    interposer_route_die_to_die(&ip, 0, 2, 256, 0);
    printf("[4] Routed Compute->IO: 256 signals on RDL layer 0\n");

    microbump_array_t mb;
    memset(&mb, 0, sizeof(mb));
    mb.x_um = 2000.0;
    mb.y_um = 5000.0;
    mb.die_src = 0;
    mb.die_dst = 1;
    mb.signal_count = 1024;
    mb.pitch_um = 55.0;
    mb.bump_type = MICROBUMP_CU_PILLAR;
    interposer_add_microbump(&ip, &mb);
    printf("[5] Added microbump array: 1024 signals @ %.0f um pitch\n",
           mb.pitch_um);

    tsv_site_t tsv;
    memset(&tsv, 0, sizeof(tsv));
    tsv.x_um = 15000.0;
    tsv.y_um = 10000.0;
    tsv.capacitance_ff = 50.0;
    tsv.resistance_mohm = 100.0;
    tsv.rdl_layer = 0;
    interposer_add_tsv(&ip, &tsv);
    printf("[6] Added TSV at (%.0f, %.0f) um\n", tsv.x_um, tsv.y_um);

    double total_current = 200.0;
    double ir_drop = interposer_calc_ir_drop(&ip, total_current);
    printf("[7] IR drop: %.2f mV @ %.0f A\n", ir_drop, total_current);

    double warpage = interposer_calc_warpage(&ip, 50.0);
    printf("[8] Estimated warpage: %.2f um (delta T=50K)\n", warpage);

    int drc = interposer_verify_drc(&ip);
    printf("[9] DRC check: %s\n", drc == 0 ? "PASS" : "FAIL");

    printf("\n=== Interposer Summary ===\n");
    interposer_print_summary(&ip);

    printf("\n=== Thermal/Power Analysis ===\n");
    tp_init(&tm, ip.spec.width_mm * ip.spec.height_mm, 45.0);
    tp_set_thermal_resistance(&tm, 0.15, 0.05);
    double t_junction = tp_calc_junction_temp(&tm, ip.total_power_w);
    printf("  T_junction: %.1f C\n", t_junction);

    hotspot_t hs;
    memset(&hs, 0, sizeof(hs));
    hs.x_mm = 10.0;
    hs.y_mm = 10.0;
    hs.width_mm = 2.0;
    hs.height_mm = 2.0;
    hs.power_density_w_per_mm2 = 3.0;
    tp_add_hotspot(&tm, &hs);
    tp_calc_junction_temp(&tm, ip.total_power_w);
    tp_mitigate_hotspots(&tm);
    printf("  Hotspot mitigated (spread to %.1fx%.1f mm)\n",
           tm.hotspots[0].width_mm, tm.hotspots[0].height_mm);

    printf("\n=== Done ===\n");
    return 0;
}
