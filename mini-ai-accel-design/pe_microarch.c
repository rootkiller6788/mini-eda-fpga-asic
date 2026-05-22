#include "pe_microarch.h"
#include <string.h>
#include <stdio.h>

void pe_init(PEMicroArch *pe, uint32_t id)
{
    if (!pe) return;
    memset(pe, 0, sizeof(PEMicroArch));
    pe->pe_id          = id;
    pe->state          = PE_STATE_IDLE;
    pe->zero_skip_en   = false;
    pe->zero_threshold = 0;
}

void pe_reset(PEMicroArch *pe)
{
    if (!pe) return;
    pe->accumulator   = 0;
    pe->acc_shadow    = 0;
    pe->state         = PE_STATE_IDLE;
    pe->stall_count   = 0;
    pe->mac_count     = 0;
    pe->skip_count    = 0;
    pe->active_cycles = 0;
    pe->bubble_cycles = 0;
    pe->overflow_flag = false;
    pe->clock_gated   = false;

    memset(&pe->wbuf, 0, sizeof(WeightDBuf));
    memset(&pe->afifo, 0, sizeof(ActFIFO));
}

void pe_set_zero_skip(PEMicroArch *pe, bool enable, int8_t threshold)
{
    if (!pe) return;
    pe->zero_skip_en   = enable;
    pe->zero_threshold = threshold;
}

void pe_load_weight(PEMicroArch *pe, int8_t w)
{
    if (!pe) return;
    uint8_t idx = pe->wbuf.active_bank;
    pe->wbuf.banks[idx]       = w;
    pe->wbuf.banks_valid[idx] = true;
    pe->state = PE_STATE_COMPUTE;
}

void pe_load_weight_pair(PEMicroArch *pe, int8_t w0, int8_t w1)
{
    if (!pe) return;
    pe->wbuf.banks[0]       = w0;
    pe->wbuf.banks[1]       = w1;
    pe->wbuf.banks_valid[0] = true;
    pe->wbuf.banks_valid[1] = true;
    pe->wbuf.active_bank    = 0;
    pe->state = PE_STATE_COMPUTE;
}

void pe_prefetch_weight(PEMicroArch *pe, int8_t w)
{
    if (!pe) return;
    if (pe->wbuf.prefetch_count < PE_WEIGHT_PREFETCH) {
        pe->wbuf.prefetch_queue[pe->wbuf.prefetch_head] = w;
        pe->wbuf.prefetch_head = (pe->wbuf.prefetch_head + 1) % PE_WEIGHT_PREFETCH;
        pe->wbuf.prefetch_count++;
    }
}

void pe_swap_weight_bank(PEMicroArch *pe)
{
    if (!pe) return;
    pe->wbuf.active_bank = 1 - pe->wbuf.active_bank;
    pe->wbuf.swap_pending = false;
}

bool pe_is_weight_ready(const PEMicroArch *pe)
{
    if (!pe) return false;
    return pe->wbuf.banks_valid[pe->wbuf.active_bank];
}

int8_t pe_active_weight(const PEMicroArch *pe)
{
    if (!pe || !pe_is_weight_ready(pe)) return 0;
    return pe->wbuf.banks[pe->wbuf.active_bank];
}

void pe_push_activation(PEMicroArch *pe, int8_t a)
{
    if (!pe) return;
    if (pe->afifo.fill >= PE_FIFO_DEPTH) return;

    pe->afifo.entries[pe->afifo.tail] = a;
    pe->afifo.tail = (pe->afifo.tail + 1) % PE_FIFO_DEPTH;
    pe->afifo.fill++;

    pe->afifo.almost_empty = (pe->afifo.fill <= 2);
    pe->afifo.almost_full  = (pe->afifo.fill >= PE_FIFO_DEPTH - 2);
}

bool pe_try_push_activation(PEMicroArch *pe, int8_t a)
{
    if (!pe || pe->afifo.fill >= PE_FIFO_DEPTH) return false;
    pe_push_activation(pe, a);
    return true;
}

int8_t pe_peek_activation(const PEMicroArch *pe)
{
    if (!pe || pe->afifo.fill == 0) return 0;
    return pe->afifo.entries[pe->afifo.head];
}

int8_t pe_consume_activation(PEMicroArch *pe)
{
    if (!pe || pe->afifo.fill == 0) return 0;
    int8_t a = pe->afifo.entries[pe->afifo.head];
    pe->afifo.head = (pe->afifo.head + 1) % PE_FIFO_DEPTH;
    pe->afifo.fill--;
    pe->afifo.almost_empty = (pe->afifo.fill <= 2);
    pe->afifo.almost_full  = (pe->afifo.fill >= PE_FIFO_DEPTH - 2);
    return a;
}

bool pe_has_activation(const PEMicroArch *pe)
{
    if (!pe) return false;
    return pe->afifo.fill > 0;
}

uint8_t pe_activation_count(const PEMicroArch *pe)
{
    if (!pe) return 0;
    return pe->afifo.fill;
}

static MACResult pe_mac_internal(PEMicroArch *pe, int8_t a, int8_t w, bool skip_check)
{
    MACResult r = {0, false, false, 0};

    if (!pe || pe->clock_gated || pe->state == PE_STATE_IDLE) {
        pe->bubble_cycles++;
        r.latency_cycles = 1;
        return r;
    }

    if (skip_check && pe->zero_skip_en) {
        if (a == 0 || (pe->zero_threshold != 0 &&
            (a < pe->zero_threshold && a > -pe->zero_threshold))) {
            pe->skip_count++;
            pe->bubble_cycles++;
            r.result = pe->accumulator;
            r.latency_cycles = 1;
            return r;
        }
    }

    int32_t prod = (int32_t)a * (int32_t)w;
    int64_t sum  = (int64_t)pe->accumulator + (int64_t)prod;

    if (sum > INT32_MAX) { r.overflow = true; pe->overflow_flag = true; }
    if (sum < INT32_MIN) { r.underflow = true; pe->overflow_flag = true; }

    pe->accumulator = (int32_t)sum;
    pe->mac_count++;
    pe->active_cycles++;
    r.result = pe->accumulator;
    r.latency_cycles = 2;
    return r;
}

MACResult pe_mac_step(PEMicroArch *pe)
{
    if (!pe) { MACResult r = {0}; return r; }
    return pe_mac_internal(pe, pe_peek_activation(pe), pe_active_weight(pe), false);
}

MACResult pe_mac_step_skip_zero(PEMicroArch *pe)
{
    if (!pe) { MACResult r = {0}; return r; }
    return pe_mac_internal(pe, pe_peek_activation(pe), pe_active_weight(pe), true);
}

void pe_mac_accumulate(PEMicroArch *pe, int8_t a, int8_t w)
{
    if (!pe) return;
    pe_mac_internal(pe, a, w, pe->zero_skip_en);
}

int32_t pe_read_accumulator(const PEMicroArch *pe)
{
    if (!pe) return 0;
    return pe->accumulator;
}

void pe_write_accumulator(PEMicroArch *pe, int32_t val)
{
    if (!pe) return;
    pe->accumulator = val;
}

void pe_clear_accumulator(PEMicroArch *pe)
{
    if (!pe) return;
    pe->accumulator = 0;
    pe->acc_shadow  = 0;
}

bool pe_is_stalled(const PEMicroArch *pe)
{
    if (!pe) return true;
    return pe->state == PE_STATE_STALLED || pe->state == PE_STATE_IDLE;
}

bool pe_can_accept_activation(const PEMicroArch *pe)
{
    if (!pe) return false;
    return pe->afifo.fill < PE_FIFO_DEPTH;
}

double pe_utilization_pct(const PEMicroArch *pe)
{
    if (!pe) return 0.0;
    uint64_t total = pe->active_cycles + pe->bubble_cycles;
    if (total == 0) return 0.0;
    return (double)pe->active_cycles / (double)total * 100.0;
}

double pe_skip_rate(const PEMicroArch *pe)
{
    if (!pe) return 0.0;
    uint64_t total_ops = pe->mac_count + pe->skip_count;
    if (total_ops == 0) return 0.0;
    return (double)pe->skip_count / (double)total_ops * 100.0;
}

void pe_clock_gate(PEMicroArch *pe, bool gate)
{
    if (!pe) return;
    pe->clock_gated = gate;
}

void pe_dump_state(const PEMicroArch *pe)
{
    if (!pe) return;
    printf("=== PE #%u State ===\n", pe->pe_id);
    printf("  State: %d, Accumulator: %d\n", pe->state, pe->accumulator);
    printf("  MACs: %llu, Skips: %llu, Skip rate: %.1f%%\n",
           (unsigned long long)pe->mac_count,
           (unsigned long long)pe->skip_count,
           pe_skip_rate(pe));
    printf("  Active cycles: %llu, Bubble: %llu, Util: %.1f%%\n",
           (unsigned long long)pe->active_cycles,
           (unsigned long long)pe->bubble_cycles,
           pe_utilization_pct(pe));
    printf("  FIFO fill: %u, Weight ready: %s, Clock-gated: %s\n",
           pe->afifo.fill,
           pe_is_weight_ready(pe) ? "yes" : "no",
           pe->clock_gated ? "yes" : "no");
}
