#ifndef PE_MICROARCH_H
#define PE_MICROARCH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PE_FIFO_DEPTH        32
#define PE_WEIGHT_DBUF_SIZE   2
#define PE_WEIGHT_PREFETCH    4

typedef struct {
    int32_t result;
    bool    overflow;
    bool    underflow;
    uint8_t latency_cycles;
} MACResult;

typedef struct {
    int8_t  entries[PE_FIFO_DEPTH];
    uint8_t head;
    uint8_t tail;
    uint8_t fill;
    bool    almost_full;
    bool    almost_empty;
} ActFIFO;

typedef struct {
    int8_t  banks[PE_WEIGHT_DBUF_SIZE];
    int8_t  prefetch_queue[PE_WEIGHT_PREFETCH];
    uint8_t active_bank;
    uint8_t prefetch_head;
    uint8_t prefetch_count;
    bool    banks_valid[PE_WEIGHT_DBUF_SIZE];
    bool    swap_pending;
} WeightDBuf;

typedef enum {
    PE_STATE_IDLE,
    PE_STATE_LOAD_WEIGHT,
    PE_STATE_COMPUTE,
    PE_STATE_STALLED,
    PE_STATE_DRAIN
} PEState;

typedef struct {
    int32_t accumulator;
    int32_t acc_shadow;
    WeightDBuf wbuf;
    ActFIFO    afifo;
    PEState    state;
    uint32_t   stall_count;
    uint64_t   mac_count;
    uint64_t   skip_count;
    uint64_t   active_cycles;
    uint64_t   bubble_cycles;
    bool       zero_skip_en;
    int8_t     zero_threshold;
    bool       overflow_flag;
    bool       clock_gated;
    uint32_t   pe_id;
} PEMicroArch;

void pe_init(PEMicroArch *pe, uint32_t id);
void pe_reset(PEMicroArch *pe);
void pe_set_zero_skip(PEMicroArch *pe, bool enable, int8_t threshold);

void pe_load_weight(PEMicroArch *pe, int8_t w);
void pe_load_weight_pair(PEMicroArch *pe, int8_t w0, int8_t w1);
void pe_prefetch_weight(PEMicroArch *pe, int8_t w);
void pe_swap_weight_bank(PEMicroArch *pe);
bool pe_is_weight_ready(const PEMicroArch *pe);
int8_t pe_active_weight(const PEMicroArch *pe);

void pe_push_activation(PEMicroArch *pe, int8_t a);
bool pe_try_push_activation(PEMicroArch *pe, int8_t a);
int8_t pe_peek_activation(const PEMicroArch *pe);
int8_t pe_consume_activation(PEMicroArch *pe);
bool pe_has_activation(const PEMicroArch *pe);
uint8_t pe_activation_count(const PEMicroArch *pe);

MACResult pe_mac_step(PEMicroArch *pe);
MACResult pe_mac_step_skip_zero(PEMicroArch *pe);
void pe_mac_accumulate(PEMicroArch *pe, int8_t a, int8_t w);
int32_t pe_read_accumulator(const PEMicroArch *pe);
void pe_write_accumulator(PEMicroArch *pe, int32_t val);
void pe_clear_accumulator(PEMicroArch *pe);

bool pe_is_stalled(const PEMicroArch *pe);
bool pe_can_accept_activation(const PEMicroArch *pe);
double pe_utilization_pct(const PEMicroArch *pe);
double pe_skip_rate(const PEMicroArch *pe);
void pe_clock_gate(PEMicroArch *pe, bool gate);
void pe_dump_state(const PEMicroArch *pe);

#endif
