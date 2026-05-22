#ifndef DNN_ISA_H
#define DNN_ISA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define IQ_DEPTH              256
#define MAX_LOOP_NEST          6
#define MAX_TILE_DIMS          3
#define ISA_INST_WIDTH_BYTES  16

typedef enum {
    IOP_NOP = 0,
    IOP_LOAD_WEIGHT,
    IOP_LOAD_ACT,
    IOP_MATMUL,
    IOP_ACTIVATION,
    IOP_STORE,
    IOP_SYNC,
    IOP_LOOP_BEGIN,
    IOP_LOOP_END,
    IOP_BARRIER,
    IOP_DMA_LOAD,
    IOP_DMA_STORE,
    IOP_SET_TILE,
    IOP_WAIT,
    IOP_HALT
} IsaOp;

typedef enum {
    AF_RELU,
    AF_SIGMOID,
    AF_TANH,
    AF_GELU,
    AF_SWISH,
    AF_LEAKY_RELU,
    AF_NONE
} ActFn;

typedef enum {
    LOOP_M,
    LOOP_N,
    LOOP_K
} LoopDim;

typedef struct __attribute__((packed)) {
    uint8_t  opcode;
    uint8_t  flags;
    uint16_t reserved;
    union {
        struct { uint32_t addr; uint16_t rows; uint16_t cols; } __attribute__((packed)) lw;
        struct { uint32_t addr; uint16_t len; uint16_t _pad; } __attribute__((packed)) la;
        struct { uint16_t m; uint16_t n; uint16_t k; uint8_t _pad[2]; } __attribute__((packed)) mm;
        struct { uint8_t fn; uint8_t _pad0; uint16_t len; uint32_t _pad1; } __attribute__((packed)) act;
        struct { uint32_t addr; uint16_t len; uint16_t _pad; } __attribute__((packed)) st;
        struct { uint16_t id; uint8_t _pad[6]; } __attribute__((packed)) sync;
        struct { uint16_t count; uint16_t stride; uint32_t _pad; } __attribute__((packed)) loop;
        struct { uint16_t id; uint8_t _pad[6]; } __attribute__((packed)) barrier;
        struct { uint32_t src; uint32_t dst; uint16_t len; uint8_t _pad[2]; } __attribute__((packed)) dma;
        struct { uint16_t tm; uint16_t tn; uint16_t tk; uint8_t _pad[2]; } __attribute__((packed)) tile;
        uint8_t raw[14];
    };
} IsaInst;

typedef struct {
    IsaInst  ring[IQ_DEPTH];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    bool     ovf;
} InstQueue;

typedef struct {
    IsaInst  current;
    uint32_t pc;
    bool     stall;
    bool     done;
    bool     branch_taken;
    uint32_t branch_target;
} IsaDecoder;

typedef struct {
    uint32_t loop_cnt[MAX_LOOP_NEST];
    uint32_t loop_end[MAX_LOOP_NEST];
    uint32_t loop_step[MAX_LOOP_NEST];
    uint32_t loop_start_pc[MAX_LOOP_NEST];
    uint8_t  depth;
} HWLoop;

typedef struct {
    uint32_t tile_m;
    uint32_t tile_n;
    uint32_t tile_k;
    uint32_t grid_m;
    uint32_t grid_n;
    uint32_t grid_k;
    uint32_t cur_m;
    uint32_t cur_n;
    uint32_t cur_k;
    uint32_t m_iters;
    uint32_t n_iters;
    uint32_t k_iters;
    uint32_t cur_m_iter;
    uint32_t cur_n_iter;
    uint32_t cur_k_iter;
    bool     m_done;
    bool     n_done;
    bool     k_done;
} TileEngine;

typedef struct {
    uint32_t m_total;
    uint32_t n_total;
    uint32_t k_total;
    uint32_t m_tile;
    uint32_t n_tile;
    uint32_t k_tile;
} MatmulShape;

typedef struct {
    InstQueue   iq;
    IsaDecoder  decoder;
    HWLoop      hwloops;
    TileEngine  tiler;
    MatmulShape shape;
    uint64_t    cycle;
    uint64_t    inst_retired;
    bool        running;
    bool        halted;
} AccelSeq;

void iq_reset(InstQueue *q);
bool iq_enq(InstQueue *q, IsaInst inst);
bool iq_deq(InstQueue *q, IsaInst *out);
bool iq_empty(const InstQueue *q);
bool iq_full(const InstQueue *q);
uint16_t iq_avail(const InstQueue *q);

IsaInst isa_nop(void);
IsaInst isa_load_weight(uint32_t addr, uint16_t rows, uint16_t cols);
IsaInst isa_load_act(uint32_t addr, uint16_t len);
IsaInst isa_matmul(uint16_t m, uint16_t n, uint16_t k);
IsaInst isa_activation(ActFn fn, uint16_t len);
IsaInst isa_store(uint32_t addr, uint16_t len);
IsaInst isa_sync(void);
IsaInst isa_loop_begin(uint16_t count, uint16_t stride);
IsaInst isa_loop_end(void);
IsaInst isa_barrier(uint16_t id);
IsaInst isa_dma_load(uint32_t src, uint32_t dst, uint16_t len);
IsaInst isa_dma_store(uint32_t src, uint32_t dst, uint16_t len);
IsaInst isa_set_tile(uint16_t tm, uint16_t tn, uint16_t tk);
IsaInst isa_wait(uint16_t id);
IsaInst isa_halt(void);
const char *isa_opcode_name(IsaOp op);
const char *isa_actfn_name(ActFn fn);

void dec_reset(IsaDecoder *d);
void dec_feed(IsaDecoder *d, IsaInst inst);
IsaInst dec_current(const IsaDecoder *d);
bool dec_stalled(const IsaDecoder *d);
bool dec_done(const IsaDecoder *d);
void dec_advance(IsaDecoder *d);

void hwloop_reset(HWLoop *hl);
void hwloop_begin(HWLoop *hl, uint32_t count, uint32_t stride);
bool hwloop_step(HWLoop *hl);
bool hwloop_active(const HWLoop *hl);

void tile_init(TileEngine *te, uint32_t m, uint32_t n, uint32_t k,
               uint32_t tm, uint32_t tn, uint32_t tk);
bool tile_next(TileEngine *te, uint32_t *mo, uint32_t *no, uint32_t *ko);
bool tile_has_more(const TileEngine *te);
void tile_reset(TileEngine *te);
void tile_progress(const TileEngine *te, uint32_t *m_done, uint32_t *n_done, uint32_t *k_done);

void seq_init(AccelSeq *s);
void seq_load(AccelSeq *s, const IsaInst *prog, uint32_t len);
void seq_step(AccelSeq *s);
bool seq_running(const AccelSeq *s);
bool seq_halted(const AccelSeq *s);
void seq_halt(AccelSeq *s);
void seq_reset(AccelSeq *s);
uint64_t seq_cycles(const AccelSeq *s);
uint64_t seq_instructions(const AccelSeq *s);

void isa_program_matmul_tiled(AccelSeq *s, const MatmulShape *shape);
void isa_program_convolution(AccelSeq *s, uint32_t H, uint32_t W,
                              uint32_t C_in, uint32_t C_out,
                              uint32_t K, uint32_t stride, uint32_t pad);
void isa_program_linear_layer(AccelSeq *s, uint32_t in_f, uint32_t out_f, uint32_t batch);
void isa_program_attention(AccelSeq *s, uint32_t seq_len, uint32_t d_model, uint32_t heads);

#endif
