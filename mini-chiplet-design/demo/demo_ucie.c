#include "ucie_d2d.h"
#include "d2d_phy.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static void demo_link_info(const ucie_link_t *link) {
    printf("    State: ");
    switch (link->state) {
    case UCIE_LINK_DOWN:     printf("DOWN\n"); break;
    case UCIE_LINK_TRAINING: printf("TRAINING\n"); break;
    case UCIE_LINK_ACTIVE:   printf("ACTIVE\n"); break;
    case UCIE_LINK_ERROR:    printf("ERROR\n"); break;
    case UCIE_LINK_RECOVERY: printf("RECOVERY\n"); break;
    default:                 printf("UNKNOWN\n"); break;
    }
}

static void demo_lane_eye(d2d_phy_t *phy, d2d_measurement_ctx_t *ctx,
                          uint32_t lane_id) {
    eye_diagram_t eye = d2d_measure_eye(phy, lane_id, ctx);
    double ui_ps = 1e12 / (double)phy->cfg.data_rate_mbps;
    printf("    Eye[%u]: H=%.1fmV W=%.1fps (%.0f%% UI)",
           lane_id, eye.eye_height_mv, eye.eye_width_ps,
           eye.eye_width_ps / ui_ps * 100.0);
    if (eye.eye_height_mv > 100.0 && eye.eye_width_ps > ui_ps * 0.4)
        printf(" [OPEN]\n");
    else
        printf(" [CLOSED]\n");
}

static void demo_traffic_generate(ucie_link_t *link, uint32_t num_flits) {
    for (uint32_t i = 0; i < num_flits; i++) {
        ucie_flit_t flit;
        uint8_t payload[64];
        for (int j = 0; j < 64; j++)
            payload[j] = (uint8_t)((i * 137 + j * 59) & 0xFF);

        ucie_protocol_t proto = (i % 5 == 0) ? UCIE_PROTO_CXL_MEM :
                                (i % 5 == 1) ? UCIE_PROTO_CXL_CACHE :
                                (i % 5 == 2) ? UCIE_PROTO_PCIE :
                                (i % 5 == 3) ? UCIE_PROTO_STREAMING :
                                               UCIE_PROTO_RAW;
        ucie_flit_pack(&flit, proto, payload, 64);
        flit.seq_num = i;
        ucie_send_flit(link, &flit);
    }
}

static void demo_ber_sweep(d2d_phy_t *phy, d2d_measurement_ctx_t *ctx) {
    printf("  BER Sweep (Lane 0):\n");
    double prev_margin = 120.0;
    for (int step = 0; step < 8; step++) {
        double margin = 120.0 - (double)step * 15.0;
        phy->lanes[0].margin_mv = margin;
        double ber = d2d_measure_ber(phy, 0, ctx,
                                      (uint64_t)1e6 << (uint64_t)step);
        printf("    Margin=%.0fmV => BER=%.2e\n", margin, ber);
        if (ber > 1e-12) break;
        prev_margin = margin;
    }
}

static void demo_prbs_test(d2d_phy_t *phy) {
    printf("  PRBS Self-Test:\n");
    prbs_pattern_t patterns[] = {PRBS7, PRBS9, PRBS15, PRBS23, PRBS31};
    const char *pnames[] = {"PRBS7", "PRBS9", "PRBS15", "PRBS23", "PRBS31"};

    for (int p = 0; p < 5; p++) {
        int result = prbs_self_test(phy, patterns[p], (uint64_t)1e7);
        printf("    %s: %s (errors=%u)\n",
               pnames[p], result == 0 ? "PASS" : "FAIL",
               phy->prbs_error_count);
    }
}

static void demo_config_comparison(void) {
    printf("\n  UCIe Configuration Comparison:\n");
    printf("  %-12s %8s %8s %10s %10s\n",
           "Config", "Lanes", "GT/s", "BW(GB/s)", "Lat(ps)");

    uint32_t configs[][2] = {
        {16, 16}, {16, 24}, {16, 32},
        {32, 16}, {32, 24}, {32, 32},
        {64, 16}, {64, 24}, {64, 32}
    };

    for (int c = 0; c < 9; c++) {
        ucie_link_t tmp;
        ucie_init(&tmp, configs[c][0], configs[c][1]);
        double bw = ucie_calc_bandwidth(&tmp);
        double lat = tmp.link_latency_ps;
        printf("  %-12s %8u %8u %10.1f %10.1f\n",
               c < 3 ? "Standard" : c < 6 ? "Advanced" : "Premium",
               tmp.num_lanes, tmp.gt_per_sec, bw / 8.0, lat);
    }
}

int main(void) {
    printf("==========================================================\n");
    printf("  UCIe Die-to-Die Interface — Full Demo\n");
    printf("==========================================================\n\n");

    printf("[Phase 1] Link Initialization\n");
    printf("---------------------------------------------\n");

    ucie_link_t link_a, link_b;
    ucie_init(&link_a, 24, 32);
    ucie_init(&link_b, 16, 24);

    ucie_phy_config_t phy_a = {800.0, 3.5, 3.0, 50.0, 1};
    ucie_phy_config_t phy_b = {750.0, 3.0, 2.5, 50.0, 1};

    ucie_phy_init(&link_a, &phy_a);
    ucie_phy_init(&link_b, &phy_b);

    printf("  Link A (compute-to-HBM): %u lanes, %u GT/s\n",
           link_a.num_lanes, link_a.gt_per_sec);
    printf("  Link B (compute-to-IO):  %u lanes, %u GT/s\n",
           link_b.num_lanes, link_b.gt_per_sec);

    if (ucie_link_train(&link_a) == 0)
        printf("  Link A training: PASS\n");
    else
        printf("  Link A training: FAIL\n");

    if (ucie_link_train(&link_b) == 0)
        printf("  Link B training: PASS\n");
    else
        printf("  Link B training: FAIL\n");

    ucie_lane_deskew(&link_a);
    ucie_lane_deskew(&link_b);
    printf("  Lane deskew complete (both links)\n");

    printf("\n[Phase 2] Traffic Generation\n");
    printf("---------------------------------------------\n");

    demo_traffic_generate(&link_a, 1000);
    demo_traffic_generate(&link_b, 500);
    printf("  Link A: %llu flits sent\n",
           (unsigned long long)link_a.total_flits_sent);
    printf("  Link B: %llu flits sent\n",
           (unsigned long long)link_b.total_flits_sent);

    printf("\n[Phase 3] D2D PHY Analysis\n");
    printf("---------------------------------------------\n");

    d2d_phy_t phy0;
    d2d_config_t dcfg;
    memset(&dcfg, 0, sizeof(dcfg));
    dcfg.clock_freq_mhz = 4000.0;
    dcfg.clk_mode = D2D_CLK_FORWARDED;
    dcfg.data_rate_mbps = 32000;
    dcfg.num_lanes = 16;
    dcfg.ddr_mode = 1;
    dcfg.skew_tolerance_ps = 100.0;
    d2d_phy_init(&phy0, &dcfg);

    d2d_measurement_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    printf("  TX Configuration:\n");
    printf("    Swing: %.0f mV\n", phy0.tx_cfg.voltage_swing_mv);
    printf("    Pre-emphasis: %.1f dB\n", phy0.tx_cfg.pre_emphasis_db);
    printf("    De-emphasis: %.1f dB\n", phy0.tx_cfg.de_emphasis_db);

    printf("\n  Eye Diagram Analysis (lanes 0, 4, 8, 12):\n");
    demo_lane_eye(&phy0, &ctx, 0);
    demo_lane_eye(&phy0, &ctx, 4);
    demo_lane_eye(&phy0, &ctx, 8);
    demo_lane_eye(&phy0, &ctx, 12);

    printf("\n");
    demo_ber_sweep(&phy0, &ctx);

    printf("\n");
    demo_prbs_test(&phy0);

    printf("\n[Phase 4] Link Recovery Test\n");
    printf("---------------------------------------------\n");

    printf("  Simulating CRC error injection...\n");
    link_a.crc_errors++;
    if (link_a.crc_errors > 0) {
        link_a.state = UCIE_LINK_RECOVERY;
        printf("  Link A entered RECOVERY state\n");
        ucie_link_recovery(&link_a);
        demo_link_info(&link_a);
    }

    printf("\n[Phase 5] Performance Summary\n");
    printf("---------------------------------------------\n");

    double bw_a = ucie_calc_bandwidth(&link_a);
    double bw_b = ucie_calc_bandwidth(&link_b);
    printf("  Link A: %.1f GB/s (%.0f GT/s x %u lanes)\n",
           bw_a / 8.0, (double)link_a.gt_per_sec, link_a.num_lanes);
    printf("  Link B: %.1f GB/s (%.0f GT/s x %u lanes)\n",
           bw_b / 8.0, (double)link_b.gt_per_sec, link_b.num_lanes);
    printf("  Total aggregate: %.1f GB/s\n", (bw_a + bw_b) / 8.0);

    double ber_a = ucie_measure_ber(&link_a);
    double ber_b = ucie_measure_ber(&link_b);
    printf("  BER Link A: %.2e  Link B: %.2e\n", ber_a, ber_b);
    printf("  BER spec: < %.0e\n", UCIE_BER_THRESHOLD);

    printf("\n[Phase 6] Configuration Comparison\n");
    printf("---------------------------------------------\n");
    demo_config_comparison();

    printf("\n[Phase 7] Link Status Dump\n");
    printf("---------------------------------------------\n");
    printf("  Link A:\n");
    ucie_print_link_status(&link_a);
    printf("  Link B:\n");
    ucie_print_link_status(&link_b);

    printf("\n  D2D PHY:\n");
    d2d_phy_print_status(&phy0);

    printf("\n==========================================================\n");
    printf("  UCIe Demo Complete\n");
    printf("==========================================================\n");

    ucie_reset(&link_a);
    ucie_reset(&link_b);
    return 0;
}
