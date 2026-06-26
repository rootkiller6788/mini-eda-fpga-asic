#ifndef BUFFER_HIERARCHY_H
#define BUFFER_HIERARCHY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define BUF_L1_SIZE        512
#define BUF_L2_SIZE        (64 * 1024)
#define BUF_L3_SIZE        (2 * 1024 * 1024)
#define BUF_MAX_MULTICAST_DEST 16
#define BUF_MAX_SCATTER        256

typedef enum {
    BUF_L1_PE_LOCAL,
    BUF_L2_ARRAY_GLOBAL,
    BUF_L3_CHIP_GLOBAL
} BufLevel;

typedef enum {
    BUF_MODE_IDLE,
    BUF_MODE_LOADING,
    BUF_MODE_COMPUTING,
    BUF_MODE_STORING,
    BUF_MODE_FLUSHING
} BufMode;

typedef struct {
    uint8_t  mem[BUF_L1_SIZE];
    uint32_t wptr;
    uint32_t rptr;
    uint32_t occupancy;
    bool     dbuf_en;
    uint8_t  dbuf_active;
    uint8_t  dbuf_mem[2][BUF_L1_SIZE / 2];
    uint32_t dbuf_wptr[2];
    uint32_t dbuf_rptr[2];
    uint32_t dbuf_occupancy[2];
    BufMode  mode;
    uint32_t total_bytes_loaded;
    uint32_t total_bytes_read;
} L1Buf;

typedef struct {
    uint8_t  mem[BUF_L2_SIZE];
    uint32_t wptr;
    uint32_t rptr;
    uint32_t occupancy;
    bool     dbuf_en;
    uint8_t  dbuf_active;
    uint8_t  dbuf_mem[2][BUF_L2_SIZE / 2];
    uint32_t dbuf_wptr[2];
    uint32_t dbuf_rptr[2];
    uint32_t dbuf_occupancy[2];
    BufMode  mode;
    uint32_t multicast_mask;
    uint32_t total_dma_in;
    uint32_t total_dma_out;
} L2Buf;

typedef struct {
    uint8_t  mem[BUF_L3_SIZE];
    uint32_t wptr;
    uint32_t rptr;
    uint32_t occupancy;
    bool     dbuf_en;
    uint8_t  dbuf_active;
    uint8_t  dbuf_mem[2][BUF_L3_SIZE / 2];
    uint32_t dbuf_wptr[2];
    uint32_t dbuf_rptr[2];
    uint32_t dbuf_occupancy[2];
    BufMode  mode;
    uint64_t dram_bytes_read;
    uint64_t dram_bytes_written;
    bool     dma_in_progress;
    bool     dma_done_irq;
} L3Buf;

typedef struct {
    uint32_t src_idx;
    uint32_t dst_idx;
    uint32_t length;
    bool     valid;
} ScatterGatherEntry;

typedef struct {
    L1Buf              l1;
    L2Buf              l2;
    L3Buf              l3;
    bool               overlap_en;
    bool               multicast_en;
    ScatterGatherEntry sg_table[BUF_MAX_SCATTER];
    uint32_t           sg_count;
    uint64_t           total_bytes_transferred;
    uint64_t           overlap_saved_cycles;
} BufferHierarchy;

void l1_init(L1Buf *b);
void l1_write(L1Buf *b, const uint8_t *data, uint32_t len);
uint32_t l1_read(L1Buf *b, uint8_t *out, uint32_t maxlen);
void l1_swap_dbuf(L1Buf *b);
bool l1_is_full(const L1Buf *b);
bool l1_is_empty(const L1Buf *b);
void l1_flush(L1Buf *b);

void l2_init(L2Buf *b);
void l2_dma_load(L2Buf *b, const uint8_t *src, uint32_t len);
uint32_t l2_dma_store(L2Buf *b, uint8_t *dst, uint32_t maxlen);
void l2_multicast_set(L2Buf *b, uint32_t dest_mask);
void l2_broadcast(L2Buf *b, const uint8_t *data, uint32_t len);
void l2_scatter_read(L2Buf *b, const uint32_t *indices, uint32_t cnt, uint8_t *out);
void l2_gather_write(L2Buf *b, const uint32_t *indices, uint32_t cnt, const uint8_t *in);
void l2_swap_dbuf(L2Buf *b);
bool l2_can_load(const L2Buf *b);
bool l2_can_unload(const L2Buf *b);

void l3_init(L3Buf *b);
void l3_dma_read_from_dram(L3Buf *b, uint32_t dram_addr, uint32_t len);
void l3_dma_write_to_dram(L3Buf *b, uint32_t dram_addr, uint32_t len);
void l3_swap_dbuf(L3Buf *b);
bool l3_is_dma_busy(const L3Buf *b);
void l3_wait_dma(L3Buf *b);
uint64_t l3_dram_bandwidth_bytes(const L3Buf *b, uint32_t freq_mhz, uint32_t bus_width);

void bh_init(BufferHierarchy *bh);
void bh_enable_overlap(BufferHierarchy *bh, bool en);
void bh_enable_multicast(BufferHierarchy *bh, bool en);
void bh_load_weights(BufferHierarchy *bh, const int8_t *w, uint32_t rows, uint32_t cols);
void bh_distribute_acts(BufferHierarchy *bh, const int8_t *a, uint32_t len, uint32_t n_dest);
void bh_collect_outputs(BufferHierarchy *bh, int32_t *out, uint32_t len);
void bh_flush_all(BufferHierarchy *bh);
uint64_t bh_peak_bandwidth(const BufferHierarchy *bh, uint32_t freq_mhz);
void bh_dump_stats(const BufferHierarchy *bh);

#endif
