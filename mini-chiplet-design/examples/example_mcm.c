#include "mcm_integration.h"
#include "interposer_tech.h"
#include "thermal_power.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    mcm_module_t mcm;
    mcm_die_t compute, hbm_stack, io_die;

    printf("=== MCM Integration Example ===\n\n");

    mcm_init(&mcm);
    printf("[1] Initialized MCM module\n");

    memset(&compute, 0, sizeof(compute));
    compute.type = MCM_DIE_COMPUTE;
    strncpy(compute.name, "Compute-Tile", sizeof(compute.name) - 1);
    compute.x_um = 5000.0;
    compute.y_um = 5000.0;
    compute.width_um = 10000.0;
    compute.height_um = 10000.0;
    compute.power_w = 180.0;
    mcm_add_die(&mcm, &compute);

    memset(&hbm_stack, 0, sizeof(hbm_stack));
    hbm_stack.type = MCM_DIE_HBM;
    strncpy(hbm_stack.name, "HBM3-8Hi", sizeof(hbm_stack.name) - 1);
    hbm_stack.x_um = 17000.0;
    hbm_stack.y_um = 5000.0;
    hbm_stack.width_um = 7000.0;
    hbm_stack.height_um = 5000.0;
    hbm_stack.power_w = 7.5;
    mcm_add_die(&mcm, &hbm_stack);

    memset(&io_die, 0, sizeof(io_die));
    io_die.type = MCM_DIE_IO;
    strncpy(io_die.name, "IO-Tile", sizeof(io_die.name) - 1);
    io_die.x_um = 5000.0;
    io_die.y_um = 17000.0;
    io_die.width_um = 19000.0;
    io_die.height_um = 3000.0;
    io_die.power_w = 20.0;
    mcm_add_die(&mcm, &io_die);

    printf("[2] Added 3 dies: %s, %s, %s\n",
           compute.name, hbm_stack.name, io_die.name);

    hbm_config_t hbm_cfg;
    hbm_cfg.num_banks = HBM_BANKS_PER_GROUP;
    hbm_cfg.num_bank_groups = HBM_BANK_GROUPS;
    hbm_cfg.row_size = HBM_ROW_BITS;
    hbm_cfg.col_size = HBM_COL_BITS;
    hbm_cfg.capacity_gb = 24.0;
    hbm_cfg.bandwidth_gbps = 819.2;
    hbm_cfg.t_ck_ps = 500.0;
    hbm_cfg.t_rcd_ns = 14.0;
    hbm_cfg.t_rp_ns = 14.0;
    hbm_cfg.t_ras_ns = 32.0;
    hbm_cfg.t_rc_ns = 46.0;
    hbm_cfg.precharge_policy = 1;
    hbm_cfg.refresh_interval_us = 3900;
    mcm_configure_hbm(&mcm, &hbm_cfg);
    printf("[3] Configured HBM3: %.0f GB, %.1f GB/s\n",
           hbm_cfg.capacity_gb, hbm_cfg.bandwidth_gbps / 8.0);

    phy_macro_t comp_hbm_phy;
    memset(&comp_hbm_phy, 0, sizeof(comp_hbm_phy));
    comp_hbm_phy.x_um = 15000.0;
    comp_hbm_phy.y_um = 7000.0;
    comp_hbm_phy.data_width = 1024;
    comp_hbm_phy.data_rate_gbps = 6.4;
    mcm_add_phy(&mcm, 0, &comp_hbm_phy);

    phy_macro_t hbm_phy;
    memset(&hbm_phy, 0, sizeof(hbm_phy));
    hbm_phy.x_um = 17000.0;
    hbm_phy.y_um = 7500.0;
    hbm_phy.data_width = 1024;
    hbm_phy.data_rate_gbps = 6.4;
    mcm_add_phy(&mcm, 1, &hbm_phy);

    mcm_connect_phys(&mcm, 0, 0, 1, 0);
    printf("[4] Connected Compute<->HBM: 1024-bit PHY @ 6.4 Gbps\n");

    mcm_route_hbm_to_compute(&mcm, 1, 0);
    printf("[5] Routed HBM pseudo-channels to Compute die\n");

    mcm_optimize_phy_placement(&mcm);
    printf("[6] Optimized PHY placement\n");

    double total_bw = mcm_calc_total_bandwidth(&mcm);
    double efficiency = mcm_calc_hbm_efficiency(&mcm);
    double power = mcm_calc_power(&mcm);
    int valid = mcm_validate_connectivity(&mcm);

    printf("[7] Total bandwidth: %.1f GB/s\n", total_bw / 8.0);
    printf("[8] HBM efficiency: %.1f%%\n", efficiency * 100.0);
    printf("[9] Total power: %.1f W\n", power);
    printf("[10] Connectivity: %s\n", valid == 0 ? "VALID" : "INVALID");

    printf("\n=== HBM Bandwidth Analysis ===\n");
    double read_bw = hbm_read_bandwidth(&mcm.hbm_cfg, 2000.0);
    double write_bw = hbm_write_bandwidth(&mcm.hbm_cfg, 2000.0);
    printf("  Read: %.1f GB/s\n", read_bw);
    printf("  Write: %.1f GB/s\n", write_bw);
    printf("  HBM Power: %.1f W\n", hbm_power_estimate(&mcm.hbm_cfg, 70.0));

    printf("\n=== MCM Floorplan ===\n");
    mcm_print_floorplan(&mcm);

    printf("\n=== HBM Configuration ===\n");
    mcm_print_hbm_stats(&mcm);

    printf("\n=== Done ===\n");
    return 0;
}
