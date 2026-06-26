#ifndef MCM_INTEGRATION_H
#define MCM_INTEGRATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define MCM_MAX_DIES          8
#define MCM_MAX_PHY           32
#define MCM_MAX_IO_BITS       2048
#define HBM_STACK_HEIGHT      8
#define HBM_PSEUDO_CHANNELS   16
#define HBM_INTERFACE_WIDTH   1024
#define HBM_BANK_GROUPS       4
#define HBM_BANKS_PER_GROUP   4
#define HBM_ROW_BITS          14
#define HBM_COL_BITS          6

typedef enum {
    MCM_DIE_COMPUTE = 0,
    MCM_DIE_HBM,
    MCM_DIE_IO,
    MCM_DIE_SERDES,
    MCM_DIE_ACCELERATOR,
    MCM_DIE_CXL_CONTROLLER
} mcm_die_type_t;

typedef struct {
    uint32_t num_banks;
    uint32_t num_bank_groups;
    uint32_t row_size;
    uint32_t col_size;
    double   capacity_gb;
    double   bandwidth_gbps;
    double   t_ck_ps;
    double   t_rcd_ns;
    double   t_rp_ns;
    double   t_ras_ns;
    double   t_rc_ns;
    uint32_t precharge_policy;
    uint32_t refresh_interval_us;
} hbm_config_t;

typedef struct {
    uint32_t     id;
    uint32_t     base_channel;
    uint32_t     num_channels;
    double       bandwidth_gbps;
    double       latency_ns;
    uint8_t      dws_present;
    uint8_t      au_hw_ecc;
} hbm_pseudo_channel_t;

typedef struct {
    uint32_t  phy_id;
    double    x_um;
    double    y_um;
    uint32_t  data_width;
    uint32_t  direction;
    double    data_rate_gbps;
    uint32_t  connected_die_id;
    uint32_t  connected_phy_id;
    uint8_t   placed;
} phy_macro_t;

typedef struct {
    mcm_die_type_t type;
    uint32_t       id;
    char           name[32];
    double         x_um;
    double         y_um;
    double         width_um;
    double         height_um;
    double         power_w;
    uint32_t       num_phys;
    phy_macro_t    phys[8];
} mcm_die_t;

typedef struct {
    uint32_t  id;
    double    l1_um;
    double    l2_um;
    double    l3_um;
    double    width_um;
    double    spacing_um;
    uint32_t  layer;
    double    total_length_um;
    double    estimated_delay_ps;
    double    estimated_loss_db;
} routing_channel_t;

typedef struct {
    uint32_t          num_dies;
    mcm_die_t         dies[MCM_MAX_DIES];
    hbm_config_t      hbm_cfg;
    hbm_pseudo_channel_t pseudo_channels[HBM_PSEUDO_CHANNELS];
    uint32_t          num_routing_channels;
    routing_channel_t routes[1024];
    double            total_power_w;
    double            total_bandwidth_gbps;
    double            interposer_size_mm2;
    uint8_t           hbm_integrated;
    uint8_t           validated;
} mcm_module_t;

void mcm_init(mcm_module_t *mcm);
int  mcm_add_die(mcm_module_t *mcm, const mcm_die_t *die);
int  mcm_add_phy(mcm_module_t *mcm, uint32_t die_id, const phy_macro_t *phy);
int  mcm_connect_phys(mcm_module_t *mcm,
                      uint32_t die_a, uint32_t phy_a,
                      uint32_t die_b, uint32_t phy_b);

int  mcm_configure_hbm(mcm_module_t *mcm, const hbm_config_t *cfg);
int  mcm_route_hbm_to_compute(mcm_module_t *mcm,
                              uint32_t hbm_die_id, uint32_t compute_die_id);
double mcm_calc_total_bandwidth(const mcm_module_t *mcm);
double mcm_calc_hbm_efficiency(const mcm_module_t *mcm);
double mcm_calc_power(const mcm_module_t *mcm);
int  mcm_validate_connectivity(const mcm_module_t *mcm);
void mcm_optimize_phy_placement(mcm_module_t *mcm);
void mcm_print_floorplan(const mcm_module_t *mcm);
void mcm_print_hbm_stats(const mcm_module_t *mcm);

double hbm_read_bandwidth(const hbm_config_t *cfg, double freq_mhz);
double hbm_write_bandwidth(const hbm_config_t *cfg, double freq_mhz);
double hbm_power_estimate(const hbm_config_t *cfg, double utilization_pct);

#ifdef __cplusplus
}
#endif

#endif
