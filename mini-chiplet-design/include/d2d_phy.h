#ifndef D2D_PHY_H
#define D2D_PHY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define D2D_MAX_LANES           64
#define D2D_MAX_PRBS_LENGTH     31
#define D2D_EYE_SAMPLE_POINTS   128
#define D2D_TRAINING_PATTERN    0xAA55F00F
#define D2D_DESKEW_MAX_PS       500
#define D2D_JITTER_TOLERANCE_PS 5

typedef enum {
    D2D_CLK_FORWARDED = 0,
    D2D_CLK_SOURCE_SYNC,
    D2D_CLK_EMBEDDED_SERDES,
    D2D_CLK_PLL_SYNCHRONIZED
} d2d_clocking_mode_t;

typedef enum {
    D2D_EQ_NONE = 0,
    D2D_EQ_CTLE,
    D2D_EQ_DFE_3TAP,
    D2D_EQ_DFE_5TAP,
    D2D_EQ_CTLE_DFE
} d2d_equalization_t;

typedef enum {
    PRBS7  = 0,
    PRBS9  = 1,
    PRBS15 = 2,
    PRBS23 = 3,
    PRBS31 = 4
} prbs_pattern_t;

typedef struct {
    double   voltage_swing_mv;
    double   pre_emphasis_db;
    double   de_emphasis_db;
    double   slew_rate_v_per_ps;
    d2d_equalization_t eq_mode;
    double   ctle_boost_db;
    double   dfe_taps[5];
} d2d_tx_config_t;

typedef struct {
    double   vref_mv;
    double   termination_ohm;
    d2d_equalization_t eq_mode;
    double   ctle_boost_db;
    double   dfe_taps[5];
    double   cdr_bandwidth_mhz;
    uint8_t  adaptive_eq;
} d2d_rx_config_t;

typedef struct {
    double   clock_freq_mhz;
    d2d_clocking_mode_t clk_mode;
    uint32_t data_rate_mbps;
    uint32_t num_lanes;
    uint8_t  ddr_mode;
    double   skew_tolerance_ps;
} d2d_config_t;

typedef struct {
    double eye_height_mv;
    double eye_width_ps;
    double eye_amplitude_mv;
    double rj_rms_ps;
    double dj_pp_ps;
    double tj_pp_ps;
    double ber;
    double snr_db;
} eye_diagram_t;

typedef struct {
    uint32_t  lane_id;
    double    skew_ps;
    double    ber;
    double    margin_mv;
    double    margin_ps;
    uint8_t   deskewed;
    uint8_t   eye_open;
} d2d_lane_state_t;

typedef struct {
    d2d_config_t   cfg;
    d2d_tx_config_t tx_cfg;
    d2d_rx_config_t rx_cfg;
    d2d_lane_state_t lanes[D2D_MAX_LANES];
    uint32_t       prbs_error_count;
    uint64_t       bit_count;
    uint8_t        tx_trained;
    uint8_t        rx_trained;
} d2d_phy_t;

typedef struct {
    uint32_t      seq_length;
    uint32_t      seed;
    double        (*sample)(void *ctx, uint32_t lane, double time_ps);
    void          *sample_ctx;
    void          (*delay_adjust)(void *ctx, uint32_t lane, double delay_ps);
    void          *delay_ctx;
    void          (*eq_adjust)(void *ctx, uint32_t lane,
                               const d2d_tx_config_t *tx,
                               const d2d_rx_config_t *rx);
    void          *eq_ctx;
} d2d_measurement_ctx_t;

void d2d_phy_init(d2d_phy_t *phy, const d2d_config_t *cfg);
void d2d_tx_config(d2d_phy_t *phy, const d2d_tx_config_t *cfg);
void d2d_rx_config(d2d_phy_t *phy, const d2d_rx_config_t *cfg);

int  d2d_training_sequence(d2d_phy_t *phy, d2d_measurement_ctx_t *ctx);
int  d2d_deskew_lanes(d2d_phy_t *phy, d2d_measurement_ctx_t *ctx);
int  d2d_equalization_train(d2d_phy_t *phy, d2d_measurement_ctx_t *ctx);
int  d2d_cdr_lock(d2d_phy_t *phy, d2d_measurement_ctx_t *ctx);

eye_diagram_t d2d_measure_eye(d2d_phy_t *phy, uint32_t lane,
                               d2d_measurement_ctx_t *ctx);
double d2d_measure_ber(d2d_phy_t *phy, uint32_t lane,
                        d2d_measurement_ctx_t *ctx, uint64_t bits);
double d2d_measure_jitter(d2d_phy_t *phy, uint32_t lane,
                           d2d_measurement_ctx_t *ctx);

uint32_t prbs_generate(prbs_pattern_t pattern, uint32_t seed, uint32_t length,
                       uint8_t *buffer);
uint32_t prbs_verify(prbs_pattern_t pattern, const uint8_t *buffer,
                     uint32_t length, uint32_t *error_count);
int  prbs_self_test(d2d_phy_t *phy, prbs_pattern_t pattern, uint64_t bit_count);

void eye_diagram_print(const eye_diagram_t *eye);
void d2d_phy_print_status(const d2d_phy_t *phy);
void d2d_phy_print_lane(const d2d_phy_t *phy, uint32_t lane_id);

double serdes_link_budget(double loss_db, double noise_floor_dbm,
                           double tx_power_dbm);
double serdes_ber_estimate(double snr_db, uint32_t modulation_order);

#ifdef __cplusplus
}
#endif

#endif
