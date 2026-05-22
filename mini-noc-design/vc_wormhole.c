#include "vc_wormhole.h"
#include <stdio.h>
#include <string.h>

void noc_flit_init(noc_flit_t *flit, noc_flit_type_t type, int src, int dst, int seq) {
    memset(flit, 0, sizeof(*flit));
    flit->type        = type;
    flit->src_id      = src;
    flit->dst_id      = dst;
    flit->sequence_id = seq;
    flit->vc_id       = -1;
    flit->hop_count   = 0;
}

bool noc_flit_is_head(const noc_flit_t *flit) { return flit->type == NOC_FLIT_HEAD; }
bool noc_flit_is_tail(const noc_flit_t *flit) { return flit->type == NOC_FLIT_TAIL; }

const char *noc_flit_type_name(noc_flit_type_t type) {
    static const char *tbl[] = { "HEAD", "BODY", "TAIL" };
    return (type < NOC_FLIT_TYPE_COUNT) ? tbl[type] : "?";
}

void noc_vc_buffer_init(noc_vc_buffer_t *buf) {
    memset(buf, 0, sizeof(*buf));
    buf->credits = NOC_BUFFER_DEPTH;
}

bool noc_vc_buffer_push(noc_vc_buffer_t *buf, const noc_flit_t *flit) {
    if (buf->count >= NOC_BUFFER_DEPTH) return false;
    buf->entries[buf->tail] = *flit;
    buf->tail = (buf->tail + 1) % NOC_BUFFER_DEPTH;
    buf->count++;
    buf->is_empty = false;
    buf->is_full  = (buf->count == NOC_BUFFER_DEPTH);
    buf->credits--;
    return true;
}

bool noc_vc_buffer_pop(noc_vc_buffer_t *buf, noc_flit_t *out) {
    if (buf->count == 0) return false;
    *out = buf->entries[buf->head];
    buf->head = (buf->head + 1) % NOC_BUFFER_DEPTH;
    buf->count--;
    buf->is_empty = (buf->count == 0);
    buf->is_full  = false;
    buf->credits++;
    return true;
}

bool noc_vc_buffer_peek(const noc_vc_buffer_t *buf, noc_flit_t *out) {
    if (buf->count == 0) return false;
    *out = buf->entries[buf->head];
    return true;
}

bool noc_vc_buffer_has_space(const noc_vc_buffer_t *buf) {
    return buf->count < NOC_BUFFER_DEPTH;
}

int noc_vc_buffer_available(const noc_vc_buffer_t *buf) {
    return NOC_BUFFER_DEPTH - buf->count;
}

void noc_vc_port_init(noc_vc_port_t *port) {
    int i;
    memset(port, 0, sizeof(*port));
    for (i = 0; i < VC_MAX; i++) noc_vc_buffer_init(&port->vc[i]);
}

int noc_vc_allocate(noc_vc_port_t *port) {
    int i;
    for (i = 0; i < VC_MAX; i++) {
        if (!port->vc_busy[i]) { port->vc_busy[i] = true; return i; }
    }
    return -1;
}

bool noc_vc_allocate_escape(noc_vc_port_t *port) {
    if (!port->vc_busy[VC_ESCAPE_VC]) {
        port->vc_busy[VC_ESCAPE_VC] = true;
        return true;
    }
    return false;
}

void noc_vc_release(noc_vc_port_t *port, int vc_id) {
    if (vc_id >= 0 && vc_id < VC_MAX) port->vc_busy[vc_id] = false;
}

bool noc_vc_is_busy(const noc_vc_port_t *port, int vc_id) {
    return (vc_id >= 0 && vc_id < VC_MAX) ? port->vc_busy[vc_id] : true;
}

void noc_credit_init(noc_credit_counter_t *cred, int max_credits) {
    cred->credits     = max_credits;
    cred->max_credits = max_credits;
}

bool noc_credit_consume(noc_credit_counter_t *cred) {
    if (cred->credits <= 0) return false;
    cred->credits--;
    return true;
}

void noc_credit_return(noc_credit_counter_t *cred) {
    if (cred->credits < cred->max_credits) cred->credits++;
}

int noc_credit_available(const noc_credit_counter_t *cred) {
    return cred->credits;
}

bool noc_wormhole_forward_flit(noc_vc_port_t *in_port, noc_vc_port_t *out_port,
                                int in_vc, int *out_vc, noc_credit_counter_t *out_credits) {
    noc_flit_t flit;
    if (!noc_vc_buffer_peek(&in_port->vc[in_vc], &flit)) return false;
    if (out_vc && *out_vc < 0) {
        *out_vc = noc_vc_allocate(out_port);
        if (*out_vc < 0) return false;
    }
    if (out_vc && !noc_vc_buffer_has_space(&out_port->vc[*out_vc])) return false;
    if (!noc_credit_consume(out_credits)) return false;
    noc_vc_buffer_pop(&in_port->vc[in_vc], &flit);
    if (out_vc) { flit.vc_id = *out_vc; noc_vc_buffer_push(&out_port->vc[*out_vc], &flit); }
    return true;
}

bool noc_escape_vc_required(int hop_count, int congestion_level) {
    return (congestion_level > 3) || (hop_count > 10);
}

int noc_select_vc(int hop_count, int congestion_level) {
    if (noc_escape_vc_required(hop_count, congestion_level))
        return VC_ESCAPE_VC;
    return VC_ADAPTIVE_VC + (congestion_level % (VC_MAX - 1));
}

void noc_flit_dump(const noc_flit_t *flit) {
    printf("Flit: %s src=%d dst=%d seq=%d vc=%d hops=%d\n",
        noc_flit_type_name(flit->type), flit->src_id, flit->dst_id,
        flit->sequence_id, flit->vc_id, flit->hop_count);
}

void noc_buffer_status(const noc_vc_buffer_t *buf) {
    printf("VC_Buffer: count=%d/%d credits=%d empty=%d full=%d\n",
        buf->count, NOC_BUFFER_DEPTH, buf->credits, buf->is_empty, buf->is_full);
}
