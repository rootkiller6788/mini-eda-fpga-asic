#include "d2d_phy.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

void d2d_phy_init(d2d_phy_t *phy, const d2d_config_t *cfg) {
    if (!phy) return;
    memset(phy, 0, sizeof(*phy));
    if (cfg) {
        memcpy(&phy->cfg, cfg, sizeof(d2d_config_t));
    } else {
        phy->cfg.clock_freq_mhz = 4000.0;
        phy->cfg.clk_mode = D2D_CLK_FORWARDED;
        phy->cfg.data_rate_mbps = 32000;
        phy->cfg.num_lanes = 16;
        phy->cfg.ddr_mode = 1;
        phy->cfg.skew_tolerance_ps = 100.0;
    }

    if (phy->cfg.num_lanes > D2D_MAX_LANES)
        phy->cfg.num_lanes = D2D_MAX_LANES;

    for (uint32_t i = 0; i < phy->cfg.num_lanes; i++) {
        phy->lanes[i].lane_id = i;
        phy->lanes[i].skew_ps = (double)((int)i - 8) * 5.0;
        phy->lanes[i].ber = 1e-15;
        phy->lanes[i].margin_mv = 100.0;
        phy->lanes[i].margin_ps = 10.0;
        phy->lanes[i].deskewed = 0;
        phy->lanes[i].eye_open = 0;
    }

    phy->tx_cfg.voltage_swing_mv = 800.0;
    phy->tx_cfg.pre_emphasis_db = 3.0;
    phy->tx_cfg.de_emphasis_db = 2.5;
    phy->tx_cfg.slew_rate_v_per_ps = 0.02;
    phy->tx_cfg.eq_mode = D2D_EQ_CTLE;
    phy->tx_cfg.ctle_boost_db = 3.0;
    memset(phy->tx_cfg.dfe_taps, 0, sizeof(phy->tx_cfg.dfe_taps));

    phy->rx_cfg.vref_mv = 600.0;
    phy->rx_cfg.termination_ohm = 50.0;
    phy->rx_cfg.eq_mode = D2D_EQ_CTLE_DFE;
    phy->rx_cfg.ctle_boost_db = 3.0;
    memset(phy->rx_cfg.dfe_taps, 0, sizeof(phy->rx_cfg.dfe_taps));
    phy->rx_cfg.cdr_bandwidth_mhz = 10.0;
    phy->rx_cfg.adaptive_eq = 1;
}

void d2d_tx_config(d2d_phy_t *phy, const d2d_tx_config_t *cfg) {
    if (!phy || !cfg) return;
    memcpy(&phy->tx_cfg, cfg, sizeof(d2d_tx_config_t));
}

void d2d_rx_config(d2d_phy_t *phy, const d2d_rx_config_t *cfg) {
    if (!phy || !cfg) return;
    memcpy(&phy->rx_cfg, cfg, sizeof(d2d_rx_config_t));
}

int d2d_training_sequence(d2d_phy_t *phy, d2d_measurement_ctx_t *ctx) {
    if (!phy) return -1;
    uint8_t pattern[256];
    for (int i = 0; i < 256; i++)
        pattern[i] = (uint8_t)((D2D_TRAINING_PATTERN >> ((i % 32))) & 0xFF);

    uint32_t trained_lanes = 0;
    for (uint32_t i = 0; i < phy->cfg.num_lanes; i++) {
        double samples[256];
        if (ctx && ctx->sample) {
            for (int j = 0; j < 256; j++)
                samples[j] = ctx->sample(ctx->sample_ctx, i, (double)j * 10.0);
        } else {
            for (int j = 0; j < 256; j++)
                samples[j] = (double)pattern[j] * 0.5 + 0.25 +
                             ((double)rand() / RAND_MAX - 0.5) * 0.05;
        }

        double success = 0.0;
        for (int j = 0; j < 256; j++) {
            double expected = (pattern[j] & 1) ? 0.75 : 0.25;
            if (fabs(samples[j] - expected) < 0.3) success += 1.0;
        }
        if (success / 256.0 > 0.95) {
            phy->lanes[i].eye_open = 1;
            trained_lanes++;
        }
    }

    phy->tx_trained = (trained_lanes == phy->cfg.num_lanes);
    return phy->tx_trained ? 0 : -1;
}

int d2d_deskew_lanes(d2d_phy_t *phy, d2d_measurement_ctx_t *ctx) {
    if (!phy || phy->cfg.num_lanes == 0) return -1;

    double ref_skew = phy->lanes[0].skew_ps;
    for (uint32_t i = 1; i < phy->cfg.num_lanes; i++) {
        double diff = phy->lanes[i].skew_ps - ref_skew;
        if (fabs(diff) > phy->cfg.skew_tolerance_ps) {
            double adj = (diff > 0) ? phy->cfg.skew_tolerance_ps
                                    : -phy->cfg.skew_tolerance_ps;
            if (ctx && ctx->delay_adjust)
                ctx->delay_adjust(ctx->delay_ctx, i, adj);
            phy->lanes[i].skew_ps = ref_skew + adj;
        }
        phy->lanes[i].deskewed = 1;
    }
    phy->lanes[0].deskewed = 1;
    return 0;
}

int d2d_equalization_train(d2d_phy_t *phy, d2d_measurement_ctx_t *ctx) {
    if (!phy) return -1;

    for (uint32_t i = 0; i < phy->cfg.num_lanes; i++) {
        double best_margin = phy->lanes[i].margin_mv;
        d2d_tx_config_t best_tx = phy->tx_cfg;

        for (int db = -2; db <= 6; db += 2) {
            d2d_tx_config_t test_tx = phy->tx_cfg;
            test_tx.pre_emphasis_db += (double)db;
            if (test_tx.pre_emphasis_db < 0.0) test_tx.pre_emphasis_db = 0.0;
            if (test_tx.pre_emphasis_db > 10.0) test_tx.pre_emphasis_db = 10.0;

            if (ctx && ctx->eq_adjust)
                ctx->eq_adjust(ctx->eq_ctx, i, &test_tx, &phy->rx_cfg);

            double margin = 100.0 + (double)(5 - abs(db)) * 10.0;
            if (margin > best_margin) {
                best_margin = margin;
                best_tx = test_tx;
            }
        }

        phy->tx_cfg = best_tx;
        phy->lanes[i].margin_mv = best_margin;
    }
    phy->rx_trained = 1;
    return 0;
}

int d2d_cdr_lock(d2d_phy_t *phy, d2d_measurement_ctx_t *ctx) {
    if (!phy) return -1;
    (void)ctx;
    uint32_t locked = 0;
    for (uint32_t i = 0; i < phy->cfg.num_lanes; i++) {
        if (phy->lanes[i].eye_open) locked++;
    }
    return (locked == phy->cfg.num_lanes) ? 0 : -1;
}

eye_diagram_t d2d_measure_eye(d2d_phy_t *phy, uint32_t lane,
                               d2d_measurement_ctx_t *ctx) {
    eye_diagram_t eye;
    memset(&eye, 0, sizeof(eye));

    if (!phy || lane >= phy->cfg.num_lanes) return eye;

    double ui_ps = 1e12 / (double)phy->cfg.data_rate_mbps;
    if (ctx && ctx->sample) {
        double max_height = -1e9, min_height = 1e9;

        for (int s = 0; s < D2D_EYE_SAMPLE_POINTS; s++) {
            double t = (double)s * ui_ps / (double)D2D_EYE_SAMPLE_POINTS;
            double v = ctx->sample(ctx->sample_ctx, lane, t);
            if (v > max_height) max_height = v;
            if (v < min_height) min_height = v;
        }

        eye.eye_height_mv = (max_height - min_height) * 1000.0;
        eye.eye_amplitude_mv = eye.eye_height_mv / 2.0;
        eye.eye_width_ps = ui_ps * 0.6;
        eye.rj_rms_ps = 1.2;
        eye.dj_pp_ps = 4.0;
        eye.tj_pp_ps = eye.rj_rms_ps * 14.0 + eye.dj_pp_ps;
        eye.snr_db = 20.0 * log10(eye.eye_height_mv / 20.0);
    } else {
        eye.eye_height_mv = 250.0;
        eye.eye_amplitude_mv = 125.0;
        eye.eye_width_ps = ui_ps * 0.8;
        eye.rj_rms_ps = 1.0;
        eye.dj_pp_ps = 3.0;
        eye.tj_pp_ps = 14.0 + 3.0;
        eye.snr_db = 22.0;
    }

    double total_jitter = eye.rj_rms_ps * 14.0 + eye.dj_pp_ps;
    double margin = (ui_ps - total_jitter) / (2.0 * ui_ps);
    eye.ber = 0.5 * erfc(margin * 7.0);
    return eye;
}

double d2d_measure_ber(d2d_phy_t *phy, uint32_t lane,
                        d2d_measurement_ctx_t *ctx, uint64_t bits) {
    (void)bits;
    if (!phy || lane >= phy->cfg.num_lanes) return 1.0;

    eye_diagram_t eye = d2d_measure_eye(phy, lane, ctx);
    double margin = (eye.eye_height_mv / 2.0) / (eye.rj_rms_ps * 7.0 + 1.0);
    double ber = 0.5 * erfc(margin / sqrt(2.0));
    phy->lanes[lane].ber = ber;
    return ber;
}

double d2d_measure_jitter(d2d_phy_t *phy, uint32_t lane,
                           d2d_measurement_ctx_t *ctx) {
    if (!phy || lane >= phy->cfg.num_lanes) return 0.0;
    eye_diagram_t eye = d2d_measure_eye(phy, lane, ctx);
    return eye.rj_rms_ps * 14.0 + eye.dj_pp_ps;
}

static uint32_t prbs_lfsr_advance(uint32_t state, uint32_t taps) {
    uint8_t parity = 0;
    uint32_t tmp = state & taps;
    while (tmp) {
        parity ^= (tmp & 1);
        tmp >>= 1;
    }
    return ((state << 1) | parity) & ((1u << 31) - 1);
}

uint32_t prbs_generate(prbs_pattern_t pattern, uint32_t seed, uint32_t length,
                       uint8_t *buffer) {
    if (!buffer || length == 0) return 0;

    uint32_t taps;
    switch (pattern) {
    case PRBS7:  taps = 0x40; break;
    case PRBS9:  taps = 0x100; break;
    case PRBS15: taps = 0x4000; break;
    case PRBS23: taps = 0x400000; break;
    case PRBS31: taps = 0x40000000; break;
    default:     taps = 0x4000; break;
    }

    uint32_t lfsr = (seed == 0) ? 0x7E : seed;
    uint32_t generated = 0;
    for (uint32_t i = 0; i < length && generated < length; i++) {
        uint8_t byte_val = 0;
        for (int b = 0; b < 8 && generated < length; b++) {
            uint8_t bit = (uint8_t)(lfsr & 1);
            byte_val |= (bit << b);
            lfsr = prbs_lfsr_advance(lfsr, taps);
            generated++;
        }
        buffer[i] = byte_val;
    }
    return generated;
}

uint32_t prbs_verify(prbs_pattern_t pattern, const uint8_t *buffer,
                     uint32_t length, uint32_t *error_count) {
    if (!buffer || length == 0) return 0;

    uint32_t taps;
    switch (pattern) {
    case PRBS7:  taps = 0x40; break;
    case PRBS9:  taps = 0x100; break;
    case PRBS15: taps = 0x4000; break;
    case PRBS23: taps = 0x400000; break;
    case PRBS31: taps = 0x40000000; break;
    default:     taps = 0x4000; break;
    }

    uint32_t lfsr = 0x7E;
    uint32_t pos = 0, errors = 0;
    for (uint32_t i = 0; i < length && pos < length; i++) {
        for (int b = 0; b < 8 && pos < length; b++) {
            uint8_t expected = (uint8_t)(lfsr & 1);
            uint8_t received = (buffer[i] >> b) & 1;
            if (expected != received) errors++;
            lfsr = prbs_lfsr_advance(lfsr, taps);
            pos++;
        }
    }

    if (error_count) *error_count = errors;
    return pos;
}

int prbs_self_test(d2d_phy_t *phy, prbs_pattern_t pattern, uint64_t bit_count) {
    if (!phy) return -1;

    uint8_t *tx_buf = (uint8_t *)malloc((size_t)(bit_count / 8 + 1));
    if (!tx_buf) return -1;

    uint32_t generated = prbs_generate(pattern, 0x7E,
                                       (uint32_t)(bit_count / 8 + 1), tx_buf);
    uint32_t errors = 0;
    prbs_verify(pattern, tx_buf, generated, &errors);
    phy->prbs_error_count = errors;
    phy->bit_count += generated;
    free(tx_buf);
    return errors == 0 ? 0 : -1;
}

void eye_diagram_print(const eye_diagram_t *eye) {
    if (!eye) return;
    printf("Eye Diagram:\n");
    printf("  Height: %.1f mV\n", eye->eye_height_mv);
    printf("  Width:  %.1f ps\n", eye->eye_width_ps);
    printf("  Amplitude: %.1f mV\n", eye->eye_amplitude_mv);
    printf("  RJ RMS: %.1f ps\n", eye->rj_rms_ps);
    printf("  DJ P-P: %.1f ps\n", eye->dj_pp_ps);
    printf("  TJ P-P: %.1f ps\n", eye->tj_pp_ps);
    printf("  SNR: %.1f dB\n", eye->snr_db);
    printf("  BER: %.2e\n", eye->ber);
}

void d2d_phy_print_status(const d2d_phy_t *phy) {
    if (!phy) return;
    printf("D2D PHY Status:\n");
    printf("  Clock: %.0f MHz, mode: %d\n",
           phy->cfg.clock_freq_mhz, phy->cfg.clk_mode);
    printf("  Data rate: %u Mbps\n", phy->cfg.data_rate_mbps);
    printf("  Lanes: %u %s\n", phy->cfg.num_lanes,
           phy->cfg.ddr_mode ? "(DDR)" : "(SDR)");
    printf("  TX trained: %s\n", phy->tx_trained ? "yes" : "no");
    printf("  RX trained: %s\n", phy->rx_trained ? "yes" : "no");
    printf("  TX swing: %.0f mV, pre: %.1f dB, de: %.1f dB\n",
           phy->tx_cfg.voltage_swing_mv,
           phy->tx_cfg.pre_emphasis_db,
           phy->tx_cfg.de_emphasis_db);
    printf("  PRBS errors: %u\n", phy->prbs_error_count);
}

void d2d_phy_print_lane(const d2d_phy_t *phy, uint32_t lane_id) {
    if (!phy || lane_id >= phy->cfg.num_lanes) return;
    const d2d_lane_state_t *l = &phy->lanes[lane_id];
    printf("D2D Lane %u:\n", lane_id);
    printf("  Skew: %.1f ps\n", l->skew_ps);
    printf("  BER: %.2e\n", l->ber);
    printf("  Margin: %.1f mV / %.1f ps\n", l->margin_mv, l->margin_ps);
    printf("  Deskewed: %s\n", l->deskewed ? "yes" : "no");
    printf("  Eye open: %s\n", l->eye_open ? "yes" : "no");
}

double serdes_link_budget(double loss_db, double noise_floor_dbm,
                           double tx_power_dbm) {
    double rx_power_dbm = tx_power_dbm - loss_db;
    double snr_db = rx_power_dbm - noise_floor_dbm;
    return snr_db;
}

double serdes_ber_estimate(double snr_db, uint32_t modulation_order) {
    double snr_linear = pow(10.0, snr_db / 10.0);
    double bits_per_symbol = log2((double)modulation_order);
    double snr_per_bit = snr_linear / bits_per_symbol;
    return 0.5 * erfc(sqrt(snr_per_bit / 2.0));
}


