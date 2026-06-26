#include "buffer_hierarchy.h"
#include <string.h>
#include <stdio.h>

static uint32_t min32(uint32_t a, uint32_t b) { return a < b ? a : b; }

void l1_init(L1Buf *b)
{
    if (!b) return;
    memset(b, 0, sizeof(L1Buf));
    b->mode = BUF_MODE_IDLE;
}

void l1_write(L1Buf *b, const uint8_t *data, uint32_t len)
{
    if (!b || !data || len == 0) return;

    if (b->dbuf_en) {
        uint8_t ab = b->dbuf_active;
        uint32_t cap = BUF_L1_SIZE / 2;
        uint32_t space = cap - b->dbuf_occupancy[ab];
        uint32_t wlen = len < space ? len : space;
        memcpy(b->dbuf_mem[ab] + b->dbuf_wptr[ab], data, wlen);
        b->dbuf_wptr[ab] = (b->dbuf_wptr[ab] + wlen) % cap;
        b->dbuf_occupancy[ab] += wlen;
    } else {
        uint32_t space = BUF_L1_SIZE - b->occupancy;
        uint32_t wlen = len < space ? len : space;
        memcpy(b->mem + b->wptr, data, wlen);
        b->wptr = (b->wptr + wlen) % BUF_L1_SIZE;
        b->occupancy += wlen;
    }
    b->total_bytes_loaded += len;
    b->mode = BUF_MODE_LOADING;
}

uint32_t l1_read(L1Buf *b, uint8_t *out, uint32_t maxlen)
{
    if (!b || !out || maxlen == 0) return 0;

    if (b->dbuf_en) {
        uint8_t ab = b->dbuf_active;
        uint32_t cap = BUF_L1_SIZE / 2;
        uint32_t avail = b->dbuf_occupancy[ab];
        uint32_t rlen = maxlen < avail ? maxlen : avail;
        for (uint32_t i = 0; i < rlen; i++) {
            out[i] = b->dbuf_mem[ab][b->dbuf_rptr[ab]];
            b->dbuf_rptr[ab] = (b->dbuf_rptr[ab] + 1) % cap;
        }
        b->dbuf_occupancy[ab] -= rlen;
        b->total_bytes_read += rlen;
        return rlen;
    } else {
        uint32_t avail = b->occupancy;
        uint32_t rlen = maxlen < avail ? maxlen : avail;
        for (uint32_t i = 0; i < rlen; i++) {
            out[i] = b->mem[b->rptr];
            b->rptr = (b->rptr + 1) % BUF_L1_SIZE;
        }
        b->occupancy -= rlen;
        b->total_bytes_read += rlen;
        return rlen;
    }
}

void l1_swap_dbuf(L1Buf *b)
{
    if (!b || !b->dbuf_en) return;
    b->dbuf_active = 1 - b->dbuf_active;
}

bool l1_is_full(const L1Buf *b)
{
    if (!b) return true;
    if (b->dbuf_en) return b->dbuf_occupancy[b->dbuf_active] >= BUF_L1_SIZE / 2;
    return b->occupancy >= BUF_L1_SIZE;
}

bool l1_is_empty(const L1Buf *b)
{
    if (!b) return true;
    if (b->dbuf_en) return b->dbuf_occupancy[b->dbuf_active] == 0;
    return b->occupancy == 0;
}

void l1_flush(L1Buf *b)
{
    if (!b) return;
    b->wptr = 0;
    b->rptr = 0;
    b->occupancy = 0;
    b->mode = BUF_MODE_IDLE;
    memset(b->dbuf_wptr, 0, sizeof(b->dbuf_wptr));
    memset(b->dbuf_rptr, 0, sizeof(b->dbuf_rptr));
    memset(b->dbuf_occupancy, 0, sizeof(b->dbuf_occupancy));
}

void l2_init(L2Buf *b)
{
    if (!b) return;
    memset(b, 0, sizeof(L2Buf));
    b->mode = BUF_MODE_IDLE;
}

void l2_dma_load(L2Buf *b, const uint8_t *src, uint32_t len)
{
    if (!b || !src || len == 0) return;

    if (b->dbuf_en) {
        uint8_t ab = b->dbuf_active;
        uint32_t cap = BUF_L2_SIZE / 2;
        uint32_t wlen = len < cap ? len : cap;
        memcpy(b->dbuf_mem[ab], src, wlen);
        b->dbuf_wptr[ab] = wlen % cap;
        b->dbuf_occupancy[ab] = wlen;
    } else {
        uint32_t wlen = len < BUF_L2_SIZE ? len : BUF_L2_SIZE;
        memcpy(b->mem, src, wlen);
        b->occupancy = wlen;
        b->wptr = wlen % BUF_L2_SIZE;
    }
    b->total_dma_in += len;
    b->mode = BUF_MODE_LOADING;
}

uint32_t l2_dma_store(L2Buf *b, uint8_t *dst, uint32_t maxlen)
{
    if (!b || !dst || maxlen == 0) return 0;

    if (b->dbuf_en) {
        uint8_t ab = b->dbuf_active;
        uint32_t avail = b->dbuf_occupancy[ab];
        uint32_t rlen = maxlen < avail ? maxlen : avail;
        memcpy(dst, b->dbuf_mem[ab], rlen);
        b->dbuf_occupancy[ab] -= rlen;
        return rlen;
    } else {
        uint32_t avail = b->occupancy;
        uint32_t rlen = maxlen < avail ? maxlen : avail;
        memcpy(dst, b->mem, rlen);
        b->occupancy -= rlen;
        return rlen;
    }
}

void l2_multicast_set(L2Buf *b, uint32_t dest_mask)
{
    if (!b) return;
    b->multicast_mask = dest_mask;
}

void l2_broadcast(L2Buf *b, const uint8_t *data, uint32_t len)
{
    if (!b || !data) return;
    uint32_t dests = b->multicast_mask;
    (void)len;
    if (dests == 0) {
        l2_dma_load(b, data, len);
    } else {
        uint32_t n_dest = 0;
        for (uint32_t i = 0; i < BUF_MAX_MULTICAST_DEST; i++)
            if (dests & (1u << i)) n_dest++;
        b->total_dma_in += (uint64_t)len;
        if (b->dbuf_en) {
            uint8_t ab = b->dbuf_active;
            uint32_t cap = BUF_L2_SIZE / 2;
            uint32_t wlen = len < cap ? len : cap;
            memcpy(b->dbuf_mem[ab], data, wlen);
            b->dbuf_occupancy[ab] = wlen;
        }
        b->total_dma_in += (uint64_t)len * (n_dest - 1);
    }
}

void l2_scatter_read(L2Buf *b, const uint32_t *indices, uint32_t cnt, uint8_t *out)
{
    if (!b || !indices || !out) return;
    for (uint32_t i = 0; i < cnt; i++) {
        uint32_t idx = indices[i];
        if (idx < BUF_L2_SIZE) out[i] = b->mem[idx];
    }
}

void l2_gather_write(L2Buf *b, const uint32_t *indices, uint32_t cnt, const uint8_t *in)
{
    if (!b || !indices || !in) return;
    for (uint32_t i = 0; i < cnt; i++) {
        uint32_t idx = indices[i];
        if (idx < BUF_L2_SIZE) b->mem[idx] = in[i];
    }
}

void l2_swap_dbuf(L2Buf *b)
{
    if (!b || !b->dbuf_en) return;
    b->dbuf_active = 1 - b->dbuf_active;
}

bool l2_can_load(const L2Buf *b)
{
    if (!b) return false;
    if (b->dbuf_en) return b->dbuf_occupancy[b->dbuf_active] < BUF_L2_SIZE / 2;
    return b->occupancy < BUF_L2_SIZE;
}

bool l2_can_unload(const L2Buf *b)
{
    if (!b) return false;
    if (b->dbuf_en) return b->dbuf_occupancy[b->dbuf_active] > 0;
    return b->occupancy > 0;
}

void l3_init(L3Buf *b)
{
    if (!b) return;
    memset(b, 0, sizeof(L3Buf));
    b->mode = BUF_MODE_IDLE;
}

void l3_dma_read_from_dram(L3Buf *b, uint32_t dram_addr, uint32_t len)
{
    if (!b) return;
    (void)dram_addr;

    b->dma_in_progress = true;
    if (b->dbuf_en) {
        uint8_t ab = b->dbuf_active;
        uint32_t cap = BUF_L3_SIZE / 2;
        uint32_t wlen = len < cap ? len : cap;
        b->dbuf_occupancy[ab] = wlen;
    } else {
        uint32_t wlen = len < BUF_L3_SIZE ? len : BUF_L3_SIZE;
        b->occupancy = wlen;
    }
    b->dram_bytes_read += len;
    b->dma_done_irq = true;
    b->dma_in_progress = false;
    b->mode = BUF_MODE_LOADING;
}

void l3_dma_write_to_dram(L3Buf *b, uint32_t dram_addr, uint32_t len)
{
    if (!b) return;
    (void)dram_addr;

    b->dma_in_progress = true;
    if (b->dbuf_en) {
        uint8_t ab = b->dbuf_active;
        uint32_t avail = b->dbuf_occupancy[ab];
        uint32_t wlen = len < avail ? len : avail;
        b->dbuf_occupancy[ab] -= wlen;
    } else {
        uint32_t avail = b->occupancy;
        uint32_t wlen = len < avail ? len : avail;
        b->occupancy -= wlen;
    }
    b->dram_bytes_written += len;
    b->dma_done_irq = true;
    b->dma_in_progress = false;
    b->mode = BUF_MODE_STORING;
}

void l3_swap_dbuf(L3Buf *b)
{
    if (!b || !b->dbuf_en) return;
    b->dbuf_active = 1 - b->dbuf_active;
}

bool l3_is_dma_busy(const L3Buf *b)
{
    if (!b) return false;
    return b->dma_in_progress;
}

void l3_wait_dma(L3Buf *b)
{
    if (!b) return;
    while (b->dma_in_progress) { }
}

uint64_t l3_dram_bandwidth_bytes(const L3Buf *b, uint32_t freq_mhz, uint32_t bus_width)
{
    (void)b;
    return (uint64_t)freq_mhz * (uint64_t)(bus_width / 8);
}

void bh_init(BufferHierarchy *bh)
{
    if (!bh) return;
    memset(bh, 0, sizeof(BufferHierarchy));
    l1_init(&bh->l1);
    l2_init(&bh->l2);
    l3_init(&bh->l3);
}

void bh_enable_overlap(BufferHierarchy *bh, bool en)
{
    if (!bh) return;
    bh->overlap_en = en;
    bh->l1.dbuf_en = en;
    bh->l2.dbuf_en = en;
    bh->l3.dbuf_en = en;
}

void bh_enable_multicast(BufferHierarchy *bh, bool en)
{
    if (!bh) return;
    bh->multicast_en = en;
}

void bh_load_weights(BufferHierarchy *bh, const int8_t *w, uint32_t rows, uint32_t cols)
{
    if (!bh || !w) return;
    uint32_t wbytes = rows * cols * sizeof(int8_t);
    l3_dma_read_from_dram(&bh->l3, 0, wbytes);
    l2_dma_load(&bh->l2, (const uint8_t*)w, min32(wbytes, BUF_L2_SIZE));
    bh->total_bytes_transferred += wbytes;

    if (bh->overlap_en) bh->overlap_saved_cycles += wbytes / 64;
}

void bh_distribute_acts(BufferHierarchy *bh, const int8_t *a, uint32_t len, uint32_t n_dest)
{
    if (!bh || !a) return;

    if (bh->multicast_en) {
        l2_multicast_set(&bh->l2, (1u << n_dest) - 1);
        l2_broadcast(&bh->l2, (const uint8_t*)a, len);
    } else {
        for (uint32_t i = 0; i < n_dest; i++) {
            l2_dma_load(&bh->l2, (const uint8_t*)a, min32(len, BUF_L2_SIZE));
        }
    }
    bh->total_bytes_transferred += (uint64_t)len * n_dest;
}

void bh_collect_outputs(BufferHierarchy *bh, int32_t *out, uint32_t len)
{
    if (!bh || !out) return;
    uint32_t obytes = len * sizeof(int32_t);
    (void)obytes;
    l2_dma_store(&bh->l2, (uint8_t*)out, obytes);
    bh->total_bytes_transferred += obytes;
}

void bh_flush_all(BufferHierarchy *bh)
{
    if (!bh) return;
    l1_flush(&bh->l1);
    l2_dma_store(&bh->l2, NULL, 0);
    l3_dma_write_to_dram(&bh->l3, 0, 0);
}

uint64_t bh_peak_bandwidth(const BufferHierarchy *bh, uint32_t freq_mhz)
{
    if (!bh) return 0;
    (void)freq_mhz;
    return (uint64_t)BUF_L3_SIZE;
}

void bh_dump_stats(const BufferHierarchy *bh)
{
    if (!bh) return;
    printf("=== Buffer Hierarchy Stats ===\n");
    printf("  L1 occupancy: %u / %u\n", bh->l1.occupancy, BUF_L1_SIZE);
    printf("  L2 occupancy: %u / %u\n", bh->l2.occupancy, BUF_L2_SIZE);
    printf("  L3 occupancy: %u / %u\n", bh->l3.occupancy, BUF_L3_SIZE);
    printf("  Total transferred: %llu bytes\n",
           (unsigned long long)bh->total_bytes_transferred);
    printf("  Overlap saved: %llu cycles\n",
           (unsigned long long)bh->overlap_saved_cycles);
    printf("  Multicast: %s\n", bh->multicast_en ? "enabled" : "disabled");
    printf("  DMA overlap: %s\n", bh->overlap_en ? "enabled" : "disabled");
}
