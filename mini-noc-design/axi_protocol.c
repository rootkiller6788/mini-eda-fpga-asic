#include "axi_protocol.h"
#include <stdio.h>
#include <string.h>

static int g_axi_outstanding = 0;

void axi_aw_init(axi_aw_channel_t *ch) {
    memset(ch, 0, sizeof(*ch));
    ch->size = 3;
    ch->burst = AXI_BURST_INCR;
}

void axi_w_init(axi_w_channel_t *ch) {
    memset(ch, 0, sizeof(*ch));
    ch->strb = 0xFF;
}

void axi_b_init(axi_b_channel_t *ch) {
    memset(ch, 0, sizeof(*ch));
}

void axi_ar_init(axi_ar_channel_t *ch) {
    memset(ch, 0, sizeof(*ch));
    ch->size = 3;
    ch->burst = AXI_BURST_INCR;
}

void axi_r_init(axi_r_channel_t *ch) {
    memset(ch, 0, sizeof(*ch));
}

bool axi_handshake(bool *valid, bool *ready) {
    bool fired = *valid && *ready;
    if (fired) { *valid = false; *ready = false; }
    return fired;
}

bool axi_aw_handshake(axi_aw_channel_t *ch) { return axi_handshake(&ch->valid, &ch->ready); }
bool axi_w_handshake(axi_w_channel_t *ch)   { return axi_handshake(&ch->valid, &ch->ready); }
bool axi_b_handshake(axi_b_channel_t *ch)   { return axi_handshake(&ch->valid, &ch->ready); }
bool axi_ar_handshake(axi_ar_channel_t *ch) { return axi_handshake(&ch->valid, &ch->ready); }
bool axi_r_handshake(axi_r_channel_t *ch)   { return axi_handshake(&ch->valid, &ch->ready); }

const char *axi_burst_name(axi_burst_type_t burst) {
    static const char *names[] = { "FIXED", "INCR", "WRAP", "Reserved" };
    return (burst <= AXI_BURST_RESERVED) ? names[burst] : "?";
}

const char *axi_resp_name(axi_resp_t resp) {
    static const char *names[] = { "OKAY", "EXOKAY", "SLVERR", "DECERR" };
    return (resp <= AXI_RESP_DECERR) ? names[resp] : "?";
}

const char *axi_qos_name(axi_qos_t qos) {
    switch (qos) {
        case AXI_QOS_LOW:      return "LOW";
        case AXI_QOS_MEDIUM:   return "MEDIUM";
        case AXI_QOS_HIGH:     return "HIGH";
        case AXI_QOS_CRITICAL: return "CRITICAL";
        default:               return "NONE";
    }
}

int axi_outstanding_count(void) { return g_axi_outstanding; }

bool axi_has_outstanding(int max_allowed) { return g_axi_outstanding < max_allowed; }

void axi_inc_outstanding(void) { g_axi_outstanding++; }
void axi_dec_outstanding(void) { if (g_axi_outstanding > 0) g_axi_outstanding--; }

bool axi_is_memory_mapped_transaction(const void *t) {
    (void)t;
    return true;
}

void axi_aw_dump(const axi_aw_channel_t *ch) {
    printf("AW: addr=0x%016llX len=%u size=%u burst=%s id=%u valid=%d ready=%d\n",
        (unsigned long long)ch->addr, ch->len, ch->size,
        axi_burst_name(ch->burst), ch->id, ch->valid, ch->ready);
}

void axi_w_dump(const axi_w_channel_t *ch) {
    printf("W:  data=0x%016llX strb=0x%02X last=%u id=%u valid=%d ready=%d\n",
        (unsigned long long)ch->data, ch->strb, ch->last, ch->id, ch->valid, ch->ready);
}

void axi_b_dump(const axi_b_channel_t *ch) {
    printf("B:  id=%u resp=%s valid=%d ready=%d\n",
        ch->id, axi_resp_name(ch->resp), ch->valid, ch->ready);
}

void axi_ar_dump(const axi_ar_channel_t *ch) {
    printf("AR: addr=0x%016llX len=%u size=%u burst=%s id=%u valid=%d ready=%d\n",
        (unsigned long long)ch->addr, ch->len, ch->size,
        axi_burst_name(ch->burst), ch->id, ch->valid, ch->ready);
}

void axi_r_dump(const axi_r_channel_t *ch) {
    printf("R:  data=0x%016llX id=%u resp=%s last=%u valid=%d ready=%d\n",
        (unsigned long long)ch->data, ch->id, axi_resp_name(ch->resp),
        ch->last, ch->valid, ch->ready);
}
