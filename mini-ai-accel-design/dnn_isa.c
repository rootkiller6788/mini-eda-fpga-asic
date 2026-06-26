#include "dnn_isa.h"
#include <string.h>
#include <stdio.h>

static uint16_t min16(uint16_t a, uint16_t b) { return a < b ? a : b; }

void iq_reset(InstQueue *q)
{
    if (!q) return;
    memset(q, 0, sizeof(InstQueue));
}

bool iq_enq(InstQueue *q, IsaInst inst)
{
    if (!q) return false;
    if (q->count >= IQ_DEPTH) { q->ovf = true; return false; }
    q->ring[q->tail] = inst;
    q->tail = (q->tail + 1) % IQ_DEPTH;
    q->count++;
    return true;
}

bool iq_deq(InstQueue *q, IsaInst *out)
{
    if (!q || !out || q->count == 0) return false;
    *out = q->ring[q->head];
    q->head = (q->head + 1) % IQ_DEPTH;
    q->count--;
    return true;
}

bool iq_empty(const InstQueue *q) { return q ? q->count == 0 : true; }
bool iq_full(const InstQueue *q) { return q ? q->count >= IQ_DEPTH : false; }
uint16_t iq_avail(const InstQueue *q) { return q ? (uint16_t)(IQ_DEPTH - q->count) : 0; }

IsaInst isa_nop(void) { IsaInst i; memset(&i, 0, sizeof(i)); i.opcode = IOP_NOP; return i; }

IsaInst isa_load_weight(uint32_t addr, uint16_t rows, uint16_t cols)
{
    IsaInst i; memset(&i, 0, sizeof(i));
    i.opcode = IOP_LOAD_WEIGHT;
    i.lw.addr = addr; i.lw.rows = rows; i.lw.cols = cols;
    return i;
}

IsaInst isa_load_act(uint32_t addr, uint16_t len)
{
    IsaInst i; memset(&i, 0, sizeof(i));
    i.opcode = IOP_LOAD_ACT;
    i.la.addr = addr; i.la.len = len;
    return i;
}

IsaInst isa_matmul(uint16_t m, uint16_t n, uint16_t k)
{
    IsaInst i; memset(&i, 0, sizeof(i));
    i.opcode = IOP_MATMUL;
    i.mm.m = m; i.mm.n = n; i.mm.k = k;
    return i;
}

IsaInst isa_activation(ActFn fn, uint16_t len)
{
    IsaInst i; memset(&i, 0, sizeof(i));
    i.opcode = IOP_ACTIVATION;
    i.act.fn = (uint8_t)fn; i.act.len = len;
    return i;
}

IsaInst isa_store(uint32_t addr, uint16_t len)
{
    IsaInst i; memset(&i, 0, sizeof(i));
    i.opcode = IOP_STORE;
    i.st.addr = addr; i.st.len = len;
    return i;
}

IsaInst isa_sync(void)
{
    IsaInst i; memset(&i, 0, sizeof(i));
    i.opcode = IOP_SYNC;
    return i;
}

IsaInst isa_loop_begin(uint16_t count, uint16_t stride)
{
    IsaInst i; memset(&i, 0, sizeof(i));
    i.opcode = IOP_LOOP_BEGIN;
    i.loop.count = count; i.loop.stride = stride;
    return i;
}

IsaInst isa_loop_end(void)
{
    IsaInst i; memset(&i, 0, sizeof(i));
    i.opcode = IOP_LOOP_END;
    return i;
}

IsaInst isa_barrier(uint16_t id)
{
    IsaInst i; memset(&i, 0, sizeof(i));
    i.opcode = IOP_BARRIER;
    i.barrier.id = id;
    return i;
}

IsaInst isa_dma_load(uint32_t src, uint32_t dst, uint16_t len)
{
    IsaInst i; memset(&i, 0, sizeof(i));
    i.opcode = IOP_DMA_LOAD;
    i.dma.src = src; i.dma.dst = dst; i.dma.len = len;
    return i;
}

IsaInst isa_dma_store(uint32_t src, uint32_t dst, uint16_t len)
{
    IsaInst i; memset(&i, 0, sizeof(i));
    i.opcode = IOP_DMA_STORE;
    i.dma.src = src; i.dma.dst = dst; i.dma.len = len;
    return i;
}

IsaInst isa_set_tile(uint16_t tm, uint16_t tn, uint16_t tk)
{
    IsaInst i; memset(&i, 0, sizeof(i));
    i.opcode = IOP_SET_TILE;
    i.tile.tm = tm; i.tile.tn = tn; i.tile.tk = tk;
    return i;
}

IsaInst isa_wait(uint16_t id)
{
    IsaInst i; memset(&i, 0, sizeof(i));
    i.opcode = IOP_WAIT;
    i.sync.id = id;
    return i;
}

IsaInst isa_halt(void)
{
    IsaInst i; memset(&i, 0, sizeof(i));
    i.opcode = IOP_HALT;
    return i;
}

const char *isa_opcode_name(IsaOp op)
{
    switch (op) {
    case IOP_NOP:          return "NOP";
    case IOP_LOAD_WEIGHT:  return "LOAD_WEIGHT";
    case IOP_LOAD_ACT:     return "LOAD_ACT";
    case IOP_MATMUL:       return "MATMUL";
    case IOP_ACTIVATION:   return "ACTIVATION";
    case IOP_STORE:        return "STORE";
    case IOP_SYNC:         return "SYNC";
    case IOP_LOOP_BEGIN:   return "LOOP_BEGIN";
    case IOP_LOOP_END:     return "LOOP_END";
    case IOP_BARRIER:      return "BARRIER";
    case IOP_DMA_LOAD:     return "DMA_LOAD";
    case IOP_DMA_STORE:    return "DMA_STORE";
    case IOP_SET_TILE:     return "SET_TILE";
    case IOP_WAIT:         return "WAIT";
    case IOP_HALT:         return "HALT";
    default:               return "UNKNOWN";
    }
}

const char *isa_actfn_name(ActFn fn)
{
    switch (fn) {
    case AF_RELU:       return "ReLU";
    case AF_SIGMOID:    return "Sigmoid";
    case AF_TANH:       return "Tanh";
    case AF_GELU:       return "GELU";
    case AF_SWISH:      return "Swish";
    case AF_LEAKY_RELU: return "LeakyReLU";
    case AF_NONE:       return "None";
    default:            return "Unknown";
    }
}

void dec_reset(IsaDecoder *d)
{
    if (!d) return;
    memset(d, 0, sizeof(IsaDecoder));
    d->current.opcode = IOP_NOP;
}

void dec_feed(IsaDecoder *d, IsaInst inst)
{
    if (!d) return;
    d->current = inst;
    d->pc++;
    d->done = (inst.opcode == IOP_HALT);
}

IsaInst dec_current(const IsaDecoder *d)
{
    if (!d) return isa_nop();
    return d->current;
}

bool dec_stalled(const IsaDecoder *d) { return d ? d->stall : true; }
bool dec_done(const IsaDecoder *d) { return d ? d->done : true; }

void dec_advance(IsaDecoder *d)
{
    if (!d) return;
    d->pc++;
}

void hwloop_reset(HWLoop *hl)
{
    if (!hl) return;
    memset(hl, 0, sizeof(HWLoop));
}

void hwloop_begin(HWLoop *hl, uint32_t count, uint32_t stride)
{
    if (!hl || hl->depth >= MAX_LOOP_NEST) return;
    hl->loop_cnt[hl->depth]  = 0;
    hl->loop_end[hl->depth]  = count;
    hl->loop_step[hl->depth] = stride;
    hl->depth++;
}

bool hwloop_step(HWLoop *hl)
{
    if (!hl || hl->depth == 0) return false;

    uint8_t top = hl->depth - 1;
    hl->loop_cnt[top]++;

    if (hl->loop_cnt[top] >= hl->loop_end[top]) {
        hl->loop_cnt[top] = 0;
        hl->depth--;
        return false;
    }
    return true;
}

bool hwloop_active(const HWLoop *hl) { return hl ? hl->depth > 0 : false; }

void tile_init(TileEngine *te, uint32_t m, uint32_t n, uint32_t k,
               uint32_t tm, uint32_t tn, uint32_t tk)
{
    if (!te) return;
    memset(te, 0, sizeof(TileEngine));
    te->tile_m = tm; te->tile_n = tn; te->tile_k = tk;
    te->grid_m = m; te->grid_n = n; te->grid_k = k;
    te->m_iters = (m + tm - 1) / tm;
    te->n_iters = (n + tn - 1) / tn;
    te->k_iters = (k + tk - 1) / tk;
}

bool tile_next(TileEngine *te, uint32_t *mo, uint32_t *no, uint32_t *ko)
{
    if (!te) return false;

    if (te->k_done) return false;

    if (mo) *mo = te->cur_m;
    if (no) *no = te->cur_n;
    if (ko) *ko = te->cur_k;

    te->cur_m_iter++;
    te->cur_m += te->tile_m;

    if (te->cur_m_iter >= te->m_iters) {
        te->cur_m_iter = 0;
        te->cur_m = 0;
        te->cur_n_iter++;
        te->cur_n += te->tile_n;

        if (te->cur_n_iter >= te->n_iters) {
            te->cur_n_iter = 0;
            te->cur_n = 0;
            te->cur_k_iter++;
            te->cur_k += te->tile_k;

            if (te->cur_k_iter >= te->k_iters) {
                te->k_done = true;
            }
        }
    }

    te->m_done = (te->cur_k_iter >= te->k_iters);
    te->n_done = te->m_done;
    return !te->k_done;
}

bool tile_has_more(const TileEngine *te) { return te ? !te->k_done : false; }

void tile_reset(TileEngine *te)
{
    if (!te) return;
    te->cur_m = 0; te->cur_n = 0; te->cur_k = 0;
    te->cur_m_iter = 0; te->cur_n_iter = 0; te->cur_k_iter = 0;
    te->m_done = false; te->n_done = false; te->k_done = false;
}

void tile_progress(const TileEngine *te, uint32_t *m_done, uint32_t *n_done, uint32_t *k_done)
{
    if (!te) return;
    if (m_done) *m_done = te->cur_m;
    if (n_done) *n_done = te->cur_n;
    if (k_done) *k_done = te->cur_k;
}

void seq_init(AccelSeq *s)
{
    if (!s) return;
    memset(s, 0, sizeof(AccelSeq));
    iq_reset(&s->iq);
    dec_reset(&s->decoder);
    hwloop_reset(&s->hwloops);
}

void seq_load(AccelSeq *s, const IsaInst *prog, uint32_t len)
{
    if (!s || !prog) return;
    iq_reset(&s->iq);
    for (uint32_t i = 0; i < len; i++) iq_enq(&s->iq, prog[i]);
    s->running = true;
    s->halted  = false;
}

void seq_step(AccelSeq *s)
{
    if (!s || !s->running || s->halted) return;
    s->cycle++;

    if (iq_empty(&s->iq)) { s->halted = true; return; }

    IsaInst inst;
    if (!iq_deq(&s->iq, &inst)) { s->halted = true; return; }

    dec_feed(&s->decoder, inst);
    s->inst_retired++;

    switch (inst.opcode) {
    case IOP_LOOP_BEGIN:
        hwloop_begin(&s->hwloops, inst.loop.count, inst.loop.stride);
        s->hwloops.loop_start_pc[s->hwloops.depth - 1] = s->decoder.pc;
        break;
    case IOP_LOOP_END:
        if (hwloop_step(&s->hwloops)) {
            s->decoder.pc = s->hwloops.loop_start_pc[s->hwloops.depth];
        }
        break;
    case IOP_HALT:
        s->halted = true;
        s->running = false;
        break;
    case IOP_SET_TILE:
        tile_init(&s->tiler, s->shape.m_total, s->shape.n_total,
                  s->shape.k_total, inst.tile.tm, inst.tile.tn, inst.tile.tk);
        break;
    default:
        break;
    }
}

bool seq_running(const AccelSeq *s) { return s ? s->running : false; }
bool seq_halted(const AccelSeq *s) { return s ? s->halted : false; }

void seq_halt(AccelSeq *s)
{
    if (!s) return;
    s->halted = true;
    s->running = false;
}

void seq_reset(AccelSeq *s) { if (s) seq_init(s); }
uint64_t seq_cycles(const AccelSeq *s) { return s ? s->cycle : 0; }
uint64_t seq_instructions(const AccelSeq *s) { return s ? s->inst_retired : 0; }

void isa_program_matmul_tiled(AccelSeq *s, const MatmulShape *shape)
{
    if (!s || !shape) return;
    seq_reset(s);

    s->shape = *shape;

    IsaInst prog[32];
    uint32_t pc = 0;

    prog[pc++] = isa_set_tile((uint16_t)shape->m_tile,
                              (uint16_t)shape->n_tile,
                              (uint16_t)shape->k_tile);

    uint32_t mt = (shape->m_total + shape->m_tile - 1) / shape->m_tile;
    uint32_t nt = (shape->n_total + shape->n_tile - 1) / shape->n_tile;
    uint32_t kt = (shape->k_total + shape->k_tile - 1) / shape->k_tile;

    prog[pc++] = isa_loop_begin((uint16_t)kt, (uint16_t)shape->k_tile);
    prog[pc++] = isa_loop_begin((uint16_t)nt, (uint16_t)shape->n_tile);
    prog[pc++] = isa_loop_begin((uint16_t)mt, (uint16_t)shape->m_tile);

    prog[pc++] = isa_load_weight(0x1000, (uint16_t)shape->m_tile, (uint16_t)shape->k_tile);
    prog[pc++] = isa_load_act(0x2000, (uint16_t)(shape->k_tile * shape->n_tile));
    prog[pc++] = isa_matmul((uint16_t)shape->m_tile,
                            (uint16_t)shape->n_tile,
                            (uint16_t)shape->k_tile);
    prog[pc++] = isa_activation(AF_RELU, (uint16_t)(shape->m_tile * shape->n_tile));
    prog[pc++] = isa_store(0x3000, (uint16_t)(shape->m_tile * shape->n_tile));

    prog[pc++] = isa_loop_end();
    prog[pc++] = isa_loop_end();
    prog[pc++] = isa_loop_end();

    prog[pc++] = isa_sync();
    prog[pc++] = isa_halt();

    seq_load(s, prog, pc);
}

void isa_program_convolution(AccelSeq *s, uint32_t H, uint32_t W,
                              uint32_t C_in, uint32_t C_out,
                              uint32_t K, uint32_t stride, uint32_t pad)
{
    if (!s) return;
    seq_reset(s);

    uint32_t H_out = (H + 2 * pad - K) / stride + 1;
    uint32_t W_out = (W + 2 * pad - K) / stride + 1;
    uint32_t M = C_out;
    uint32_t N = H_out * W_out;
    uint32_t KK = K * K * C_in;

    MatmulShape shape = { M, N, KK, min16((uint16_t)M, 32), min16((uint16_t)N, 32), min16((uint16_t)KK, 32) };
    isa_program_matmul_tiled(s, &shape);
}

void isa_program_linear_layer(AccelSeq *s, uint32_t in_f, uint32_t out_f, uint32_t batch)
{
    if (!s) return;
    seq_reset(s);

    MatmulShape shape = { out_f, batch, in_f, 64, 64, 64 };
    isa_program_matmul_tiled(s, &shape);
}

void isa_program_attention(AccelSeq *s, uint32_t seq_len, uint32_t d_model, uint32_t heads)
{
    if (!s) return;
    seq_reset(s);

    uint32_t d_head = d_model / heads;
    MatmulShape q_shape = { seq_len, d_head, d_model, 32, 32, 32 };
    MatmulShape k_shape = { seq_len, d_head, d_model, 32, 32, 32 };
    MatmulShape v_shape = { seq_len, d_head, d_model, 32, 32, 32 };
    MatmulShape attn_shape = { seq_len, seq_len, d_head, 32, 32, 32 };
    MatmulShape out_shape = { seq_len, d_model, d_model, 32, 32, 32 };

    IsaInst prog[64];
    uint32_t pc = 0;

    prog[pc++] = isa_set_tile(32, 32, 32);

    for (uint32_t h = 0; h < heads; h++) {
        (void)q_shape; (void)k_shape; (void)v_shape;
        prog[pc++] = isa_load_weight(0x1000 + h * 0x1000, (uint16_t)d_head, (uint16_t)d_model);
        prog[pc++] = isa_load_act(0x2000, (uint16_t)(d_model * seq_len));
        prog[pc++] = isa_matmul((uint16_t)seq_len, (uint16_t)d_head, (uint16_t)d_model);
    }

    (void)attn_shape; (void)out_shape;
    prog[pc++] = isa_sync();
    prog[pc++] = isa_halt();

    seq_load(s, prog, pc);
}
