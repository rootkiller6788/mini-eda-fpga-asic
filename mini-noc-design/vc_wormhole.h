#ifndef VC_WORMHOLE_H
#define VC_WORMHOLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    VC_ESCAPE_VC   = 0,
    VC_ADAPTIVE_VC = 1,
    VC_MAX         = 4
};

#define NOC_FLIT_WIDTH    128
#define NOC_BUFFER_DEPTH  8

typedef enum {
    NOC_FLIT_HEAD = 0,
    NOC_FLIT_BODY = 1,
    NOC_FLIT_TAIL = 2,
    NOC_FLIT_TYPE_COUNT
} noc_flit_type_t;

typedef struct {
    uint64_t data[NOC_FLIT_WIDTH / 64];
    noc_flit_type_t type;
    int  vc_id;
    int  src_id;
    int  dst_id;
    int  hop_count;
    int  sequence_id;
} noc_flit_t;

typedef struct {
    noc_flit_t entries[NOC_BUFFER_DEPTH];
    int  head;
    int  tail;
    int  count;
    int  credits;
    bool is_empty;
    bool is_full;
} noc_vc_buffer_t;

typedef struct {
    noc_vc_buffer_t vc[VC_MAX];
    bool vc_busy[VC_MAX];
    int  vc_assigned;
} noc_vc_port_t;

typedef struct {
    int credits;
    int max_credits;
} noc_credit_counter_t;

void noc_flit_init(noc_flit_t *flit, noc_flit_type_t type, int src, int dst, int seq);
bool noc_flit_is_head(const noc_flit_t *flit);
bool noc_flit_is_tail(const noc_flit_t *flit);
const char *noc_flit_type_name(noc_flit_type_t type);

void noc_vc_buffer_init(noc_vc_buffer_t *buf);
bool noc_vc_buffer_push(noc_vc_buffer_t *buf, const noc_flit_t *flit);
bool noc_vc_buffer_pop(noc_vc_buffer_t *buf, noc_flit_t *out);
bool noc_vc_buffer_peek(const noc_vc_buffer_t *buf, noc_flit_t *out);
bool noc_vc_buffer_has_space(const noc_vc_buffer_t *buf);
int  noc_vc_buffer_available(const noc_vc_buffer_t *buf);

void noc_vc_port_init(noc_vc_port_t *port);
int  noc_vc_allocate(noc_vc_port_t *port);
bool noc_vc_allocate_escape(noc_vc_port_t *port);
void noc_vc_release(noc_vc_port_t *port, int vc_id);
bool noc_vc_is_busy(const noc_vc_port_t *port, int vc_id);

void noc_credit_init(noc_credit_counter_t *cred, int max_credits);
bool noc_credit_consume(noc_credit_counter_t *cred);
void noc_credit_return(noc_credit_counter_t *cred);
int  noc_credit_available(const noc_credit_counter_t *cred);

bool noc_wormhole_forward_flit(noc_vc_port_t *in_port, noc_vc_port_t *out_port,
                                int in_vc, int *out_vc, noc_credit_counter_t *out_credits);

bool noc_escape_vc_required(int hop_count, int congestion_level);
int  noc_select_vc(int hop_count, int congestion_level);

void noc_flit_dump(const noc_flit_t *flit);
void noc_buffer_status(const noc_vc_buffer_t *buf);

#ifdef __cplusplus
}
#endif

#endif
