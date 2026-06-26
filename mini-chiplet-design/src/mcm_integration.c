#include "mcm_integration.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void mcm_init(mcm_module_t *mcm) {
    if (!mcm) return;
    memset(mcm, 0, sizeof(*mcm));
    mcm->hbm_cfg.num_banks = HBM_BANKS_PER_GROUP;
    mcm->hbm_cfg.num_bank_groups = HBM_BANK_GROUPS;
    mcm->hbm_cfg.row_size = HBM_ROW_BITS;
    mcm->hbm_cfg.col_size = HBM_COL_BITS;
    mcm->hbm_cfg.capacity_gb = 16.0;
    mcm->hbm_cfg.bandwidth_gbps = 307.2;
    mcm->hbm_cfg.t_ck_ps = 625.0;
    mcm->hbm_cfg.t_rcd_ns = 14.0;
    mcm->hbm_cfg.t_rp_ns = 14.0;
    mcm->hbm_cfg.t_ras_ns = 32.0;
    mcm->hbm_cfg.t_rc_ns = 46.0;
    mcm->hbm_cfg.precharge_policy = 1;
    mcm->hbm_cfg.refresh_interval_us = 3900;
}

int mcm_add_die(mcm_module_t *mcm, const mcm_die_t *die) {
    if (!mcm || !die) return -1;
    if (mcm->num_dies >= MCM_MAX_DIES) return -2;
    memcpy(&mcm->dies[mcm->num_dies], die, sizeof(mcm_die_t));
    mcm->dies[mcm->num_dies].id = mcm->num_dies;
    mcm->num_dies++;
    mcm->total_power_w += die->power_w;
    return 0;
}

int mcm_add_phy(mcm_module_t *mcm, uint32_t die_id, const phy_macro_t *phy) {
    if (!mcm || !phy) return -1;
    if (die_id >= mcm->num_dies) return -2;
    mcm_die_t *die = &mcm->dies[die_id];
    if (die->num_phys >= 8) return -3;
    memcpy(&die->phys[die->num_phys], phy, sizeof(phy_macro_t));
    die->phys[die->num_phys].phy_id = die->num_phys;
    die->num_phys++;
    return 0;
}

int mcm_connect_phys(mcm_module_t *mcm,
                     uint32_t die_a, uint32_t phy_a,
                     uint32_t die_b, uint32_t phy_b) {
    if (!mcm) return -1;
    if (die_a >= mcm->num_dies || die_b >= mcm->num_dies) return -2;
    mcm_die_t *da = &mcm->dies[die_a];
    mcm_die_t *db = &mcm->dies[die_b];
    if (phy_a >= da->num_phys || phy_b >= db->num_phys) return -3;

    phy_macro_t *pa = &da->phys[phy_a];
    phy_macro_t *pb = &db->phys[phy_b];

    pa->connected_die_id = die_b;
    pa->connected_phy_id = phy_b;
    pb->connected_die_id = die_a;
    pb->connected_phy_id = phy_a;

    routing_channel_t *rc = &mcm->routes[mcm->num_routing_channels];
    memset(rc, 0, sizeof(*rc));
    rc->id = mcm->num_routing_channels;
    rc->l1_um = pa->x_um;
    rc->l2_um = pa->y_um;
    rc->l3_um = 0.0;
    rc->width_um = 2.0;
    rc->spacing_um = 2.0;
    rc->layer = 0;
    double dx = pb->x_um - pa->x_um;
    double dy = pb->y_um - pa->y_um;
    rc->total_length_um = sqrt(dx * dx + dy * dy);
    rc->estimated_delay_ps = rc->total_length_um * 6.5;
    rc->estimated_loss_db = rc->total_length_um * 0.001;
    mcm->num_routing_channels++;
    return 0;
}

int mcm_configure_hbm(mcm_module_t *mcm, const hbm_config_t *cfg) {
    if (!mcm || !cfg) return -1;
    memcpy(&mcm->hbm_cfg, cfg, sizeof(hbm_config_t));
    mcm->hbm_integrated = 1;

    double channel_bw = cfg->bandwidth_gbps / (double)HBM_PSEUDO_CHANNELS;
    for (uint32_t i = 0; i < HBM_PSEUDO_CHANNELS; i++) {
        mcm->pseudo_channels[i].id = i;
        mcm->pseudo_channels[i].base_channel = i / 2;
        mcm->pseudo_channels[i].num_channels = 1;
        mcm->pseudo_channels[i].bandwidth_gbps = channel_bw;
        mcm->pseudo_channels[i].latency_ns = 10.0 + (double)(i % 4) * 1.5;
        mcm->pseudo_channels[i].dws_present = 1;
        mcm->pseudo_channels[i].au_hw_ecc = 1;
    }
    return 0;
}

int mcm_route_hbm_to_compute(mcm_module_t *mcm,
                              uint32_t hbm_die_id, uint32_t compute_die_id) {
    if (!mcm) return -1;
    if (hbm_die_id >= mcm->num_dies || compute_die_id >= mcm->num_dies) return -2;

    mcm_die_t *hbm   = &mcm->dies[hbm_die_id];
    mcm_die_t *comp  = &mcm->dies[compute_die_id];

    double cx = comp->x_um + comp->width_um / 2.0;
    double cy = comp->y_um + comp->height_um / 2.0;
    double hx = hbm->x_um + hbm->width_um / 2.0;
    double hy = hbm->y_um + hbm->height_um / 2.0;

    for (uint32_t i = 0; i < HBM_PSEUDO_CHANNELS && i < 1024; i++) {
        routing_channel_t *rc = &mcm->routes[mcm->num_routing_channels];
        memset(rc, 0, sizeof(*rc));
        rc->id = mcm->num_routing_channels;
        rc->l1_um = hx + (double)(i % 4) * 5.0;
        rc->l2_um = hy;
        rc->width_um = 2.0;
        rc->spacing_um = 2.0;
        rc->layer = 2;
        double dx = cx + (double)(i % 4) * 5.0 - rc->l1_um;
        double dy = cy - rc->l2_um;
        rc->total_length_um = sqrt(dx * dx + dy * dy);
        rc->estimated_delay_ps = rc->total_length_um * 6.5;
        rc->estimated_loss_db = rc->total_length_um * 0.001;
        mcm->num_routing_channels++;
    }
    return 0;
}

double mcm_calc_total_bandwidth(const mcm_module_t *mcm) {
    if (!mcm) return 0.0;
    double bw = 0.0;
    for (uint32_t i = 0; i < mcm->num_dies; i++) {
        for (uint32_t j = 0; j < mcm->dies[i].num_phys; j++) {
            if (mcm->dies[i].phys[j].connected_die_id != UINT32_MAX)
                bw += mcm->dies[i].phys[j].data_rate_gbps *
                      (double)mcm->dies[i].phys[j].data_width;
        }
    }
    return bw;
}

double mcm_calc_hbm_efficiency(const mcm_module_t *mcm) {
    if (!mcm || !mcm->hbm_integrated) return 0.0;
    double total_cycles = mcm->hbm_cfg.t_rc_ns * 1000.0 / mcm->hbm_cfg.t_ck_ps;
    double busy_cycles = mcm->hbm_cfg.t_ras_ns * 1000.0 / mcm->hbm_cfg.t_ck_ps;
    return (busy_cycles / total_cycles) * 0.85;
}

double mcm_calc_power(const mcm_module_t *mcm) {
    if (!mcm) return 0.0;
    double p = 0.0;
    for (uint32_t i = 0; i < mcm->num_dies; i++)
        p += mcm->dies[i].power_w;
    if (mcm->hbm_integrated)
        p += 7.5;
    for (uint32_t i = 0; i < mcm->num_routing_channels; i++)
        p += mcm->routes[i].total_length_um * 1e-6 * 0.5;
    return p;
}

int mcm_validate_connectivity(const mcm_module_t *mcm) {
    if (!mcm) return -1;
    if (mcm->num_dies == 0) return 1;

    for (uint32_t i = 0; i < mcm->num_dies; i++) {
        uint32_t connected = 0;
        for (uint32_t j = 0; j < mcm->dies[i].num_phys; j++) {
            if (mcm->dies[i].phys[j].connected_die_id != UINT32_MAX)
                connected++;
        }
        if (connected == 0 && mcm->num_dies > 1)
            return (int)(i + 100);
    }
    return 0;
}

void mcm_optimize_phy_placement(mcm_module_t *mcm) {
    if (!mcm || mcm->num_dies < 2) return;

    for (uint32_t i = 0; i < mcm->num_dies; i++) {
        mcm_die_t *die = &mcm->dies[i];
        for (uint32_t j = 0; j < die->num_phys; j++) {
            double edge_x = die->x_um + die->width_um;
            double edge_y = die->y_um + ((double)j + 0.5) *
                            (die->height_um / (double)die->num_phys);
            die->phys[j].x_um = edge_x;
            die->phys[j].y_um = edge_y;
            die->phys[j].placed = 1;
        }
    }
}

void mcm_print_floorplan(const mcm_module_t *mcm) {
    if (!mcm) return;
    printf("MCM Floorplan:\n");
    printf("  Dies: %u\n", mcm->num_dies);
    for (uint32_t i = 0; i < mcm->num_dies; i++) {
        const char *types[] = {"Compute", "HBM", "IO", "SerDes",
                               "Accel", "CXL"};
        printf("  [%u] %s at (%.0f, %.0f) %.0fx%.0f um, %.1f W\n",
               i,
               types[mcm->dies[i].type < 6 ? mcm->dies[i].type : 0],
               mcm->dies[i].x_um, mcm->dies[i].y_um,
               mcm->dies[i].width_um, mcm->dies[i].height_um,
               mcm->dies[i].power_w);
        for (uint32_t j = 0; j < mcm->dies[i].num_phys; j++) {
            printf("    PHY[%u] at (%.0f,%.0f) %u-bit %.1f Gbps -> die[%u].PHY[%u]\n",
                   j, mcm->dies[i].phys[j].x_um, mcm->dies[i].phys[j].y_um,
                   mcm->dies[i].phys[j].data_width,
                   mcm->dies[i].phys[j].data_rate_gbps,
                   mcm->dies[i].phys[j].connected_die_id,
                   mcm->dies[i].phys[j].connected_phy_id);
        }
    }
    printf("  Routing channels: %u\n", mcm->num_routing_channels);
    printf("  Total power: %.2f W\n", mcm->total_power_w);
    printf("  HBM integrated: %s\n", mcm->hbm_integrated ? "yes" : "no");
}

void mcm_print_hbm_stats(const mcm_module_t *mcm) {
    if (!mcm || !mcm->hbm_integrated) return;
    printf("HBM Stats:\n");
    printf("  Capacity: %.1f GB\n", mcm->hbm_cfg.capacity_gb);
    printf("  Bandwidth: %.1f GB/s\n", mcm->hbm_cfg.bandwidth_gbps / 8.0);
    printf("  tCK: %.1f ps\n", mcm->hbm_cfg.t_ck_ps);
    printf("  tRCD: %.1f ns\n", mcm->hbm_cfg.t_rcd_ns);
    printf("  tRP: %.1f ns\n", mcm->hbm_cfg.t_rp_ns);
    printf("  tRAS: %.1f ns\n", mcm->hbm_cfg.t_ras_ns);
    printf("  tRC: %.1f ns\n", mcm->hbm_cfg.t_rc_ns);
    printf("  Bank groups: %u x %u banks\n",
           mcm->hbm_cfg.num_bank_groups, mcm->hbm_cfg.num_banks);
    printf("  Pseudo channels: %u\n", HBM_PSEUDO_CHANNELS);
    printf("  Efficiency: %.1f%%\n", mcm_calc_hbm_efficiency(mcm) * 100.0);
}

double hbm_read_bandwidth(const hbm_config_t *cfg, double freq_mhz) {
    if (!cfg) return 0.0;
    double transactions_per_ns = freq_mhz * 2.0 / 1000.0;
    double read_ratio = 0.7;
    return transactions_per_ns * (double)HBM_INTERFACE_WIDTH *
           read_ratio / 8.0;
}

double hbm_write_bandwidth(const hbm_config_t *cfg, double freq_mhz) {
    if (!cfg) return 0.0;
    double transactions_per_ns = freq_mhz * 2.0 / 1000.0;
    double write_ratio = 0.3;
    return transactions_per_ns * (double)HBM_INTERFACE_WIDTH *
           write_ratio / 8.0;
}

double hbm_power_estimate(const hbm_config_t *cfg, double utilization_pct) {
    if (!cfg) return 0.0;
    double base_power = 3.5;
    double dynamic_power = 4.0 * (utilization_pct / 100.0);
    return base_power + dynamic_power;
}
