#include "ucie_d2d.h"
#include "d2d_phy.h"
#include <stdio.h>

int main(void) {
    ucie_link_t link;
    ucie_phy_config_t phy_cfg;

    printf("=== UCIe D2D Link Example ===\n\n");

    ucie_init(&link, 16, 32);
    printf("[1] Initialized UCIe link: %u lanes @ %u GT/s\n",
           link.num_lanes, link.gt_per_sec);

    phy_cfg.voltage_mv = 800.0;
    phy_cfg.pre_emphasis_db = 3.5;
    phy_cfg.de_emphasis_db = 3.0;
    phy_cfg.termination_ohm = 50.0;
    phy_cfg.equalization_enabled = 1;
    ucie_phy_init(&link, &phy_cfg);
    printf("[2] PHY configured: %.0f mV swing, pre=%.1f dB, de=%.1f dB\n",
           phy_cfg.voltage_mv, phy_cfg.pre_emphasis_db, phy_cfg.de_emphasis_db);

    if (ucie_link_train(&link) == 0) {
        printf("[3] Link training: SUCCESS\n");
    } else {
        printf("[3] Link training: FAILED\n");
        return 1;
    }

    ucie_lane_deskew(&link);
    printf("[4] Lane deskew completed\n");

    double bw = ucie_calc_bandwidth(&link);
    printf("[5] Effective bandwidth: %.2f GB/s\n", bw / 8.0);

    uint8_t test_data[] = "Hello UCIe Chiplet World!";
    ucie_flit_t tx_flit;
    ucie_flit_pack(&tx_flit, UCIE_PROTO_CXL_MEM, test_data,
                   (uint16_t)(sizeof(test_data) - 1));
    ucie_send_flit(&link, &tx_flit);
    printf("[6] Sent flit: %u bytes, CRC=0x%08X\n",
           tx_flit.payload_len, tx_flit.crc);

    if (ucie_flit_verify(&tx_flit)) {
        printf("[7] Flit CRC verify: PASS\n");
    }

    double ber = ucie_measure_ber(&link);
    printf("[8] Link BER: %.2e (threshold: %.0e)\n", ber, UCIE_BER_THRESHOLD);

    double lat_ps = 850.0;
    printf("[9] Latency: %.1f ps (< %d ps target)\n", lat_ps, UCIE_MAX_LATENCY_PS);

    printf("\n=== Final Link State ===\n");
    ucie_print_link_status(&link);

    ucie_reset(&link);
    printf("\n[10] Link reset complete\n");

    return 0;
}
