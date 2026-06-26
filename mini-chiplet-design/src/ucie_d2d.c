#include "ucie_d2d.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define UCIE_DESKEW_MAX_PS 500

static uint32_t crc32_byte(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    for (int i = 0; i < 8; i++) {
        if (crc & 1)
            crc = (crc >> 1) ^ UCIE_CRC_POLY;
        else
            crc >>= 1;
    }
    return crc;
}

static uint32_t crc32_buffer(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        crc = crc32_byte(crc, data[i]);
    return crc ^ 0xFFFFFFFF;
}

void ucie_init(ucie_link_t *link, uint32_t num_lanes, uint32_t gt_speed) {
    if (!link) return;
    memset(link, 0, sizeof(*link));
    if (num_lanes > UCIE_MAX_LANES) num_lanes = UCIE_MAX_LANES;
    link->num_lanes = num_lanes;
    link->gt_per_sec = gt_speed;
    link->state = UCIE_LINK_DOWN;
    link->link_latency_ps = 0.0;
    link->phy_cfg.voltage_mv = 800.0;
    link->phy_cfg.pre_emphasis_db = 3.0;
    link->phy_cfg.de_emphasis_db = 2.5;
    link->phy_cfg.termination_ohm = 50.0;
    link->phy_cfg.equalization_enabled = 1;

    for (uint32_t i = 0; i < num_lanes; i++) {
        link->lanes[i].lane_id = (uint8_t)i;
        link->lanes[i].ber = 0.0;
        link->lanes[i].eye_height_mv = 200.0;
        link->lanes[i].eye_width_ps = 25.0;
        link->lanes[i].jitter_ps_rms = 1.0;
        link->lanes[i].skew_ps = 0;
        link->lanes[i].locked = 0;
    }
}

int ucie_phy_init(ucie_link_t *link, const ucie_phy_config_t *cfg) {
    if (!link || !cfg) return -1;
    memcpy(&link->phy_cfg, cfg, sizeof(ucie_phy_config_t));
    for (uint32_t i = 0; i < link->num_lanes; i++)
        link->lanes[i].locked = 0;
    return 0;
}

static void training_pattern_fill(uint8_t *buf, uint32_t len) {
    uint32_t lfsr = 0xACE1;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t bit = (uint8_t)(((lfsr >> 0) ^ (lfsr >> 2) ^
                                 (lfsr >> 3) ^ (lfsr >> 5)) & 1);
        lfsr = (lfsr >> 1) | ((uint32_t)bit << 15);
        buf[i] = (uint8_t)(lfsr & 0xFF);
    }
}

int ucie_link_train(ucie_link_t *link) {
    if (!link || link->num_lanes == 0) return -1;

    link->state = UCIE_LINK_TRAINING;
    uint8_t pattern[UCIE_TRAINING_SEQ_LEN];
    training_pattern_fill(pattern, UCIE_TRAINING_SEQ_LEN);

    uint32_t locked_lanes = 0;
    for (uint32_t i = 0; i < link->num_lanes; i++) {
        double simulated_skew = (double)((int)i - (int)(link->num_lanes / 2)) * 5.0;
        int skew_ok = (fabs(simulated_skew) < 120.0);
        if (skew_ok) {
            link->lanes[i].skew_ps = (int32_t)simulated_skew;
            link->lanes[i].locked = 1;
            link->lanes[i].eye_height_mv = 250.0 + (double)(i % 3) * 10.0;
            link->lanes[i].eye_width_ps = 30.0 - (double)(i % 5) * 0.5;
            locked_lanes++;
        }
    }

    link->link_latency_ps = link->num_lanes > 1 ? 850.0 : 1200.0;
    link->state = (locked_lanes == link->num_lanes)
                  ? UCIE_LINK_ACTIVE : UCIE_LINK_ERROR;
    return link->state == UCIE_LINK_ACTIVE ? 0 : -1;
}

int ucie_lane_deskew(ucie_link_t *link) {
    if (!link) return -1;
    double skew_sum = 0.0;
    double ref_skew = (double)link->lanes[0].skew_ps;
    for (uint32_t i = 1; i < link->num_lanes; i++) {
        double diff = (double)link->lanes[i].skew_ps - ref_skew;
        if (fabs(diff) > UCIE_DESKEW_MAX_PS) {
            link->lanes[i].skew_ps = (int32_t)(ref_skew +
                (diff > 0 ? UCIE_DESKEW_MAX_PS : -UCIE_DESKEW_MAX_PS));
        }
        skew_sum += (double)link->lanes[i].skew_ps;
    }
    return (fabs(skew_sum / (double)(link->num_lanes - 1) - ref_skew) < 50.0) ? 0 : -1;
}

uint32_t ucie_flit_crc32(const ucie_flit_t *flit) {
    uint8_t buf[sizeof(ucie_flit_t)];
    memcpy(buf, flit, sizeof(*flit));
    memset(buf + offsetof(ucie_flit_t, crc), 0, sizeof(uint32_t));
    return crc32_buffer(buf, sizeof(*flit));
}

void ucie_flit_pack(ucie_flit_t *flit, ucie_protocol_t proto,
                    const uint8_t *data, uint16_t len) {
    if (!flit) return;
    memset(flit, 0, sizeof(*flit));
    flit->protocol_id = (uint8_t)proto;
    if (len > sizeof(flit->payload)) len = (uint16_t)sizeof(flit->payload);
    flit->payload_len = len;
    if (data && len > 0) memcpy(flit->payload, data, len);
    flit->crc = ucie_flit_crc32(flit);
}

int ucie_flit_verify(const ucie_flit_t *flit) {
    if (!flit) return 0;
    uint32_t computed = ucie_flit_crc32(flit);
    return (computed == flit->crc) ? 1 : 0;
}

int ucie_send_flit(ucie_link_t *link, const ucie_flit_t *flit) {
    if (!link || !flit) return -1;
    if (link->state != UCIE_LINK_ACTIVE) return -2;
    link->total_flits_sent++;
    return 0;
}

int ucie_recv_flit(ucie_link_t *link, ucie_flit_t *flit) {
    if (!link || !flit) return -1;
    if (link->state != UCIE_LINK_ACTIVE) return -2;
    link->total_flits_recv++;
    memcpy(flit, flit, 0);
    if (!ucie_flit_verify(flit)) {
        link->crc_errors++;
        link->retry_count++;
        link->state = UCIE_LINK_RECOVERY;
        return -3;
    }
    return 0;
}

void ucie_set_callbacks(ucie_link_t *link, const ucie_callbacks_t *cb) {
    if (!link) return;
    (void)cb;
}

double ucie_calc_bandwidth(const ucie_link_t *link) {
    if (!link || link->num_lanes == 0) return 0.0;
    double raw_gbps = (double)link->gt_per_sec * (double)link->num_lanes;
    double efficiency = 0.92;
    return raw_gbps * efficiency;
}

double ucie_measure_ber(const ucie_link_t *link) {
    if (!link || link->num_lanes == 0) return 1.0;
    double total_ber = 0.0;
    for (uint32_t i = 0; i < link->num_lanes; i++)
        total_ber += link->lanes[i].ber;
    return total_ber / (double)link->num_lanes;
}

void ucie_link_recovery(ucie_link_t *link) {
    if (!link) return;
    if (link->state == UCIE_LINK_RECOVERY || link->state == UCIE_LINK_ERROR) {
        link->state = UCIE_LINK_TRAINING;
        ucie_link_train(link);
    }
}

void ucie_reset(ucie_link_t *link) {
    if (!link) return;
    link->state = UCIE_LINK_DOWN;
    link->total_flits_sent = 0;
    link->total_flits_recv = 0;
    link->crc_errors = 0;
    link->retry_count = 0;
    for (uint32_t i = 0; i < link->num_lanes; i++) {
        link->lanes[i].ber = 0.0;
        link->lanes[i].locked = 0;
        link->lanes[i].skew_ps = 0;
    }
}

void ucie_print_link_status(const ucie_link_t *link) {
    if (!link) return;
    const char *state_str[] = {"DOWN", "TRAINING", "ACTIVE", "ERROR", "RECOVERY"};
    printf("UCIe Link Status:\n");
    printf("  State: %s\n", state_str[link->state]);
    printf("  Lanes: %u @ %u GT/s\n", link->num_lanes, link->gt_per_sec);
    printf("  Bandwidth: %.2f GB/s\n", ucie_calc_bandwidth(link) / 8.0);
    printf("  Latency: %.1f ps\n", link->link_latency_ps);
    printf("  Flits: sent=%llu recv=%llu\n",
           (unsigned long long)link->total_flits_sent,
           (unsigned long long)link->total_flits_recv);
    printf("  Errors: CRC=%llu retries=%llu\n",
           (unsigned long long)link->crc_errors,
           (unsigned long long)link->retry_count);
    for (uint32_t i = 0; i < link->num_lanes; i++) {
        printf("  Lane %u: lock=%u BER=%.1e eye=%.1fmV/%.1fps skew=%d\n",
               i, link->lanes[i].locked, link->lanes[i].ber,
               link->lanes[i].eye_height_mv, link->lanes[i].eye_width_ps,
               link->lanes[i].skew_ps);
    }
}

void ucie_dump_flit(const ucie_flit_t *flit) {
    if (!flit) return;
    printf("UCIe Flit: proto=%u len=%u seq=%u CRC=0x%08X\n",
           flit->protocol_id, flit->payload_len, flit->seq_num, flit->crc);
}
