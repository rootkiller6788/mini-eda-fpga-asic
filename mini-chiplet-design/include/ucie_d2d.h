#ifndef UCIE_D2D_H
#define UCIE_D2D_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define UCIE_MAX_LANES          64
#define UCIE_GT_MODE_16         0
#define UCIE_GT_MODE_24         1
#define UCIE_GT_MODE_32         2
#define UCIE_FLIT_SIZE          256
#define UCIE_CRC_POLY           0x1EDC6F41
#define UCIE_BER_THRESHOLD      1e-27
#define UCIE_MAX_LATENCY_PS     2000
#define UCIE_TRAINING_SEQ_LEN   128

typedef enum {
    UCIE_LINK_DOWN = 0,
    UCIE_LINK_TRAINING,
    UCIE_LINK_ACTIVE,
    UCIE_LINK_ERROR,
    UCIE_LINK_RECOVERY
} ucie_link_state_t;

typedef enum {
    UCIE_PROTO_RAW = 0,
    UCIE_PROTO_CXL_MEM,
    UCIE_PROTO_CXL_CACHE,
    UCIE_PROTO_PCIE,
    UCIE_PROTO_STREAMING
} ucie_protocol_t;

typedef struct {
    uint32_t crc;
    uint32_t seq_num;
    uint64_t timestamp;
    uint8_t  flit_type;
    uint8_t  protocol_id;
    uint16_t payload_len;
    uint8_t  payload[UCIE_FLIT_SIZE / 8];
} ucie_flit_t;

typedef struct {
    double voltage_mv;
    double pre_emphasis_db;
    double de_emphasis_db;
    double termination_ohm;
    uint8_t equalization_enabled;
} ucie_phy_config_t;

typedef struct {
    uint8_t  lane_id;
    double   ber;
    double   eye_height_mv;
    double   eye_width_ps;
    double   jitter_ps_rms;
    int32_t  skew_ps;
    uint8_t  locked;
} ucie_lane_status_t;

typedef struct {
    uint32_t         num_lanes;
    uint32_t         gt_per_sec;
    ucie_link_state_t state;
    ucie_phy_config_t phy_cfg;
    ucie_lane_status_t lanes[UCIE_MAX_LANES];
    double           link_latency_ps;
    uint64_t         total_flits_sent;
    uint64_t         total_flits_recv;
    uint64_t         crc_errors;
    uint64_t         retry_count;
} ucie_link_t;

typedef struct {
    void  (*on_link_up)(ucie_link_t *link);
    void  (*on_link_down)(ucie_link_t *link);
    void  (*on_flit_recv)(ucie_link_t *link, const ucie_flit_t *flit);
    void  (*on_error)(ucie_link_t *link, uint32_t error_code);
    void  *user_data;
} ucie_callbacks_t;

void ucie_init(ucie_link_t *link, uint32_t num_lanes, uint32_t gt_speed);
int  ucie_phy_init(ucie_link_t *link, const ucie_phy_config_t *cfg);
int  ucie_link_train(ucie_link_t *link);
int  ucie_send_flit(ucie_link_t *link, const ucie_flit_t *flit);
int  ucie_recv_flit(ucie_link_t *link, ucie_flit_t *flit);
void ucie_set_callbacks(ucie_link_t *link, const ucie_callbacks_t *cb);
double ucie_calc_bandwidth(const ucie_link_t *link);
double ucie_measure_ber(const ucie_link_t *link);
int  ucie_lane_deskew(ucie_link_t *link);
void ucie_link_recovery(ucie_link_t *link);
void ucie_reset(ucie_link_t *link);

uint32_t ucie_flit_crc32(const ucie_flit_t *flit);
void     ucie_flit_pack(ucie_flit_t *flit, ucie_protocol_t proto,
                        const uint8_t *data, uint16_t len);
int      ucie_flit_verify(const ucie_flit_t *flit);

void ucie_print_link_status(const ucie_link_t *link);
void ucie_dump_flit(const ucie_flit_t *flit);

#ifdef __cplusplus
}
#endif

#endif
