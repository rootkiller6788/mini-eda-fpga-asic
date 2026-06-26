#ifndef AXI_PROTOCOL_H
#define AXI_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    AXI_ADDR_WIDTH = 32,
    AXI_DATA_WIDTH = 64,
    AXI_ID_WIDTH   = 8,
    AXI_LEN_WIDTH  = 8,
    AXI_SIZE_WIDTH = 3
};

typedef enum {
    AXI_BURST_FIXED = 0,
    AXI_BURST_INCR  = 1,
    AXI_BURST_WRAP  = 2,
    AXI_BURST_RESERVED = 3
} axi_burst_type_t;

typedef enum {
    AXI_RESP_OKAY   = 0,
    AXI_RESP_EXOKAY = 1,
    AXI_RESP_SLVERR = 2,
    AXI_RESP_DECERR = 3
} axi_resp_t;

typedef enum {
    AXI_QOS_NONE      = 0,
    AXI_QOS_LOW       = 1,
    AXI_QOS_MEDIUM    = 8,
    AXI_QOS_HIGH      = 12,
    AXI_QOS_CRITICAL  = 15
} axi_qos_t;

typedef struct {
    uint64_t addr;
    uint8_t  len;
    uint8_t  size;
    uint8_t  burst;
    uint8_t  id;
    bool     valid;
    bool     ready;
} axi_aw_channel_t;

typedef struct {
    uint64_t data;
    uint8_t  strb;
    uint8_t  last;
    uint8_t  id;
    bool     valid;
    bool     ready;
} axi_w_channel_t;

typedef struct {
    uint8_t  id;
    uint8_t  resp;
    bool     valid;
    bool     ready;
} axi_b_channel_t;

typedef struct {
    uint64_t addr;
    uint8_t  len;
    uint8_t  size;
    uint8_t  burst;
    uint8_t  id;
    bool     valid;
    bool     ready;
} axi_ar_channel_t;

typedef struct {
    uint64_t data;
    uint8_t  id;
    uint8_t  resp;
    uint8_t  last;
    bool     valid;
    bool     ready;
} axi_r_channel_t;

void axi_aw_init(axi_aw_channel_t *ch);
void axi_w_init(axi_w_channel_t *ch);
void axi_b_init(axi_b_channel_t *ch);
void axi_ar_init(axi_ar_channel_t *ch);
void axi_r_init(axi_r_channel_t *ch);

bool axi_handshake(bool *valid, bool *ready);
bool axi_aw_handshake(axi_aw_channel_t *ch);
bool axi_w_handshake(axi_w_channel_t *ch);
bool axi_b_handshake(axi_b_channel_t *ch);
bool axi_ar_handshake(axi_ar_channel_t *ch);
bool axi_r_handshake(axi_r_channel_t *ch);

const char *axi_burst_name(axi_burst_type_t burst);
const char *axi_resp_name(axi_resp_t resp);
const char *axi_qos_name(axi_qos_t qos);

int  axi_outstanding_count(void);
bool axi_has_outstanding(int max_allowed);
void axi_inc_outstanding(void);
void axi_dec_outstanding(void);

bool axi_is_memory_mapped_transaction(const void *t);

void axi_aw_dump(const axi_aw_channel_t *ch);
void axi_w_dump(const axi_w_channel_t *ch);
void axi_b_dump(const axi_b_channel_t *ch);
void axi_ar_dump(const axi_ar_channel_t *ch);
void axi_r_dump(const axi_r_channel_t *ch);

#ifdef __cplusplus
}
#endif

#endif
