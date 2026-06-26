/* ================================================================
 * buffer_hierarchy.c — Multi-level buffer hierarchy for DNN accel
 *
 * Implements: L1/L2/DRAM buffer model, DMA engine, double buffering,
 * prefetch scheduling, energy estimation.
 *
 * L1: Buffer/segment/DMA data structures
 * L2: Memory hierarchy concepts — locality, reuse distance
 * L3: Double buffering (ping-pong scheme), DMA descriptor chains
 * L4: Little's Law for buffer sizing: B = λ × RTT
 * L5: Prefetch scheduling algorithm, access pattern optimization
 * L7: Application — memory-aware layer scheduling for CNNs
 *
 * Course mapping:
 *   CMU 15-740, Berkeley CS 152, MIT 6.004, Stanford CS 149
 * ================================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "buffer_hierarchy.h"

/* ================================================================
 * Buffer Level implementation — L1
 * ================================================================ */

void buf_level_init(buf_level_t *bl, buf_level_e lv, const char *name,
                     uint32_t size_bytes, uint32_t latency, double bw_gbs) {
    if (!bl) return;
    memset(bl, 0, sizeof(*bl));
    bl->level = lv;
    if (name) strncpy(bl->name, name, BUF_NAME_LEN - 1);
    bl->total_size_bytes = size_bytes;
    bl->free_bytes = size_bytes;
    bl->line_size_bytes = 64;  /* default cache line */
    bl->access_latency_cycles = latency;
    bl->bandwidth_gb_s = bw_gbs;
    bl->num_segments = 0;

    /* Initialize one large free segment */
    bl->segments[0].base_addr = 0;
    bl->segments[0].size_bytes = size_bytes;
    bl->segments[0].used_bytes = 0;
    bl->segments[0].locked = false;
    bl->segments[0].dirty = false;
    bl->num_segments = 1;
}

uint32_t buf_level_alloc(buf_level_t *bl, uint32_t size_bytes, const char *tag) {
    if (!bl || size_bytes == 0) return (uint32_t)-1;

    /* Find first free segment large enough */
    for (uint32_t i = 0; i < bl->num_segments; i++) {
        if (!bl->segments[i].locked && bl->segments[i].used_bytes == 0
            && bl->segments[i].size_bytes >= size_bytes) {

            /* Split if significantly larger */
            if (bl->segments[i].size_bytes >= size_bytes + 256
                && bl->num_segments < BUF_MAX_SEGMENTS) {
                /* Create new segment for remaining space */
                uint32_t new_idx = bl->num_segments++;
                bl->segments[new_idx].base_addr = bl->segments[i].base_addr + size_bytes;
                bl->segments[new_idx].size_bytes = bl->segments[i].size_bytes - size_bytes;
                bl->segments[new_idx].used_bytes = 0;
                bl->segments[new_idx].locked = false;
                bl->segments[new_idx].dirty = false;
            }

            bl->segments[i].size_bytes = size_bytes;
            bl->segments[i].used_bytes = size_bytes;
            bl->segments[i].locked = true;
            if (tag) strncpy(bl->segments[i].tag, tag, BUF_NAME_LEN - 1);
            bl->free_bytes -= size_bytes;
            return i;
        }
    }
    return (uint32_t)-1; /* allocation failed */
}

void buf_level_free(buf_level_t *bl, uint32_t seg_id) {
    if (!bl || seg_id >= bl->num_segments) return;
    buf_segment_t *seg = &bl->segments[seg_id];
    seg->used_bytes = 0;
    seg->locked = false;
    seg->dirty = false;
    bl->free_bytes += seg->size_bytes;
    memset(seg->tag, 0, sizeof(seg->tag));
}

bool buf_level_can_fit(const buf_level_t *bl, uint32_t size_bytes) {
    if (!bl) return false;
    return bl->free_bytes >= size_bytes;
}

double buf_level_utilization(const buf_level_t *bl) {
    if (!bl || bl->total_size_bytes == 0) return 0.0;
    return 1.0 - (double)bl->free_bytes / (double)bl->total_size_bytes;
}

void buf_level_reset_stats(buf_level_t *bl) {
    if (!bl) return;
    bl->read_count = 0;
    bl->write_count = 0;
    bl->read_bytes = 0;
    bl->write_bytes = 0;
    bl->hit_count = 0;
    bl->miss_count = 0;
    bl->conflict_count = 0;
}

/* ================================================================
 * Double buffering — L3: ping-pong scheme
 * ================================================================ */

void buf_level_set_double_buffer(buf_level_t *bl, bool enabled) {
    if (!bl) return;
    bl->double_buffered = enabled;
    if (enabled) {
        bl->active_bank = 0;
    }
}

void buf_level_swap_bank(buf_level_t *bl) {
    if (!bl || !bl->double_buffered) return;
    bl->active_bank = 1 - bl->active_bank;
}

/* ================================================================
 * DMA Engine — L3: asynchronous data transfer
 * ================================================================ */

void dma_engine_init(dma_engine_t *dma, uint32_t channels, double bw_gbs) {
    if (!dma) return;
    memset(dma, 0, sizeof(*dma));
    if (channels > BUF_MAX_DMA_CHANNELS) channels = BUF_MAX_DMA_CHANNELS;
    dma->num_channels = channels;
    dma->aggregate_bw_gb_s = bw_gbs;
}

int dma_submit(dma_engine_t *dma, const dma_descriptor_t *desc) {
    if (!dma || !desc) return -1;

    /* Find free channel */
    for (uint32_t i = 0; i < dma->num_channels; i++) {
        if (!dma->channel_busy[i]) {
            dma->channels[i] = *desc;
            dma->channels[i].status = DMA_IN_FLIGHT;
            dma->channel_busy[i] = true;
            dma->active_transfers++;
            return (int)i;
        }
    }
    return -1; /* all channels busy */
}

void dma_tick(dma_engine_t *dma, buffer_hierarchy_t *bh) {
    if (!dma) return;

    /* Process each active channel */
    for (uint32_t i = 0; i < dma->num_channels; i++) {
        if (!dma->channel_busy[i]) continue;

        dma_descriptor_t *ch = &dma->channels[i];
        if (ch->status != DMA_IN_FLIGHT) continue;

        /* Simulate transfer time based on bandwidth and size */
        uint32_t transfer_cycles = 1;
        if (dma->aggregate_bw_gb_s > 0) {
            double bw_per_channel = dma->aggregate_bw_gb_s / dma->num_channels;
            double time_ns = (double)ch->size_bytes / (bw_per_channel * 1e9) * 1e9;
            transfer_cycles = (uint32_t)(time_ns);  /* assume 1 cycle = 1 ns */
            if (transfer_cycles < 1) transfer_cycles = 1;
        }

        ch->complete_cycle++;
        if (ch->complete_cycle >= transfer_cycles) {
            ch->status = DMA_COMPLETE;
            dma->channel_busy[i] = false;
            dma->total_bytes_transferred += ch->size_bytes;
            dma->total_transfer_cycles += ch->complete_cycle;
            dma->active_transfers--;

            /* Update buffer statistics */
            if (bh && ch->src_level) ch->src_level->read_bytes += ch->size_bytes;
            if (bh && ch->dst_level) ch->dst_level->write_bytes += ch->size_bytes;

            /* Invoke callback */
            if (ch->on_complete) {
                ch->on_complete(ch->userdata);
            }
        }
    }

    /* Process prefetch queue */
    if (dma->prefetch_count > 0) {
        bool channel_free = false;
        for (uint32_t i = 0; i < dma->num_channels; i++) {
            if (!dma->channel_busy[i]) { channel_free = true; break; }
        }
        if (channel_free) {
            dma_descriptor_t *pref = &dma->prefetch_queue[dma->prefetch_head];
            int ch_id = dma_submit(dma, pref);
            if (ch_id >= 0) {
                dma->prefetch_head = (dma->prefetch_head + 1) % BUF_MAX_PREFETCH_Q;
                dma->prefetch_count--;
            }
        }
    }
}

bool dma_is_idle(const dma_engine_t *dma) {
    return dma && dma->active_transfers == 0;
}

uint32_t dma_pending_transfers(const dma_engine_t *dma) {
    return dma ? dma->active_transfers : 0;
}

void dma_wait_all(dma_engine_t *dma, buffer_hierarchy_t *bh) {
    if (!dma) return;
    while (dma->active_transfers > 0) {
        dma_tick(dma, bh);
    }
}

int dma_prefetch(dma_engine_t *dma, const dma_descriptor_t *desc) {
    if (!dma || !desc) return -1;
    if (dma->prefetch_count >= BUF_MAX_PREFETCH_Q) return -1;

    dma->prefetch_queue[dma->prefetch_tail] = *desc;
    dma->prefetch_queue[dma->prefetch_tail].status = DMA_PENDING;
    dma->prefetch_tail = (dma->prefetch_tail + 1) % BUF_MAX_PREFETCH_Q;
    dma->prefetch_count++;
    return 0;
}

void dma_reset(dma_engine_t *dma) {
    if (!dma) return;
    memset(dma->channels, 0, sizeof(dma->channels));
    memset(dma->channel_busy, 0, sizeof(dma->channel_busy));
    dma->active_transfers = 0;
    dma->prefetch_head = 0;
    dma->prefetch_tail = 0;
    dma->prefetch_count = 0;
}

/* ================================================================
 * Buffer Hierarchy — L2: multi-level coordination
 * ================================================================ */

void buf_hierarchy_init(buffer_hierarchy_t *bh) {
    if (!bh) return;
    memset(bh, 0, sizeof(*bh));
    bh->global_cycle = 0;

    /* Default energy model (pJ per access, 7nm estimates) */
    bh->energy_per_read_pj[BUF_LEVEL_L1_LOCAL]   = 0.5;
    bh->energy_per_read_pj[BUF_LEVEL_L2_GLOBAL]  = 2.0;
    bh->energy_per_read_pj[BUF_LEVEL_L3_LLC]     = 8.0;
    bh->energy_per_read_pj[BUF_LEVEL_DRAM]       = 200.0;

    bh->energy_per_write_pj[BUF_LEVEL_L1_LOCAL]  = 0.6;
    bh->energy_per_write_pj[BUF_LEVEL_L2_GLOBAL] = 2.5;
    bh->energy_per_write_pj[BUF_LEVEL_L3_LLC]    = 10.0;
    bh->energy_per_write_pj[BUF_LEVEL_DRAM]      = 250.0;
}

int buf_hierarchy_add_level(buffer_hierarchy_t *bh, buf_level_e lv,
                              const char *name, uint32_t size, uint32_t lat, double bw) {
    if (!bh || bh->level_count >= BUF_MAX_LEVELS) return -1;
    int idx = bh->level_count++;
    buf_level_init(&bh->levels[idx], lv, name, size, lat, bw);
    return idx;
}

buf_level_t *buf_hierarchy_get_level(buffer_hierarchy_t *bh, buf_level_e lv) {
    if (!bh) return NULL;
    for (uint32_t i = 0; i < bh->level_count; i++) {
        if (bh->levels[i].level == lv) return &bh->levels[i];
    }
    return NULL;
}

void buf_hierarchy_tick(buffer_hierarchy_t *bh) {
    if (!bh) return;
    bh->global_cycle++;
    dma_tick(&bh->dma, bh);
}

void buf_hierarchy_collect_perf(const buffer_hierarchy_t *bh, buf_perf_t *perf) {
    if (!bh || !perf) return;
    memset(perf, 0, sizeof(*perf));

    for (uint32_t i = 0; i < bh->level_count; i++) {
        const buf_level_t *bl = &bh->levels[i];
        perf->total_bytes_moved += bl->read_bytes + bl->write_bytes;
        uint64_t total_accesses = bl->read_count + bl->write_count;
        if (total_accesses > 0) {
            perf->avg_latency_cycles += (double)bl->access_latency_cycles;
            if (bl->miss_count + bl->hit_count > 0) {
                perf->hit_rate += (double)bl->hit_count / (double)(bl->hit_count + bl->miss_count);
            }
        }
    }
    if (bh->level_count > 0) {
        perf->avg_latency_cycles /= bh->level_count;
        perf->hit_rate /= bh->level_count;
    }
    perf->total_energy_uj = bh->total_energy_pj / 1e6;
    perf->dma_cycles = bh->dma.total_transfer_cycles;

    if (bh->dma.total_transfer_cycles > 0) {
        perf->avg_bw_utilization = (double)bh->dma.total_bytes_transferred /
            (bh->dma.total_transfer_cycles * bh->dma.aggregate_bw_gb_s * 1e9) * 1e9;
    }
    perf->prefetch_accuracy = 0.5;  /* default estimate */
}

void buf_hierarchy_print_perf(const buf_perf_t *perf) {
    if (!perf) return;
    printf("=== Buffer Hierarchy Performance ===\n");
    printf("  Total bytes moved:  %llu\n", (unsigned long long)perf->total_bytes_moved);
    printf("  Avg bandwidth util: %.1f%%\n", perf->avg_bw_utilization * 100.0);
    printf("  Avg latency:        %.1f cycles\n", perf->avg_latency_cycles);
    printf("  Hit rate:           %.1f%%\n", perf->hit_rate * 100.0);
    printf("  Total energy:       %.2f uJ\n", perf->total_energy_uj);
    printf("  DMA cycles:         %llu\n", (unsigned long long)perf->dma_cycles);
    printf("  Prefetch accuracy:  %.1f%%\n", perf->prefetch_accuracy * 100.0);
}

void buf_hierarchy_reset(buffer_hierarchy_t *bh) {
    if (!bh) return;
    for (uint32_t i = 0; i < bh->level_count; i++) {
        buf_level_reset_stats(&bh->levels[i]);
    }
    dma_reset(&bh->dma);
    bh->global_cycle = 0;
    bh->total_energy_pj = 0;
}

/* ================================================================
 * DMA helpers for common patterns — L7: applications
 * ================================================================ */

int dma_load_weights(buffer_hierarchy_t *bh, const double *weights, uint32_t size_bytes) {
    if (!bh || !weights) return -1;
    buf_level_t *l2 = buf_hierarchy_get_level(bh, BUF_LEVEL_L2_GLOBAL);
    if (!l2 || !buf_level_can_fit(l2, size_bytes)) return -1;

    dma_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.direction = DMA_DRAM_TO_L2;
    desc.src_level = buf_hierarchy_get_level(bh, BUF_LEVEL_DRAM);
    desc.dst_level = l2;
    desc.src_addr = 0;  /* weights in DRAM */
    desc.dst_addr = buf_level_alloc(l2, size_bytes, "weights");
    desc.size_bytes = size_bytes;
    desc.pattern = BUF_PATTERN_SEQUENTIAL;

    return dma_submit(&bh->dma, &desc);
}

int dma_load_inputs(buffer_hierarchy_t *bh, const double *inputs, uint32_t size_bytes) {
    if (!bh || !inputs) return -1;
    buf_level_t *l2 = buf_hierarchy_get_level(bh, BUF_LEVEL_L2_GLOBAL);
    if (!l2 || !buf_level_can_fit(l2, size_bytes)) return -1;

    dma_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.direction = DMA_DRAM_TO_L2;
    desc.src_level = buf_hierarchy_get_level(bh, BUF_LEVEL_DRAM);
    desc.dst_level = l2;
    desc.src_addr = size_bytes;  /* inputs after weights */
    desc.dst_addr = buf_level_alloc(l2, size_bytes, "inputs");
    desc.size_bytes = size_bytes;
    desc.pattern = BUF_PATTERN_SEQUENTIAL;

    return dma_submit(&bh->dma, &desc);
}

int dma_store_outputs(buffer_hierarchy_t *bh, double *outputs, uint32_t size_bytes) {
    if (!bh || !outputs) return -1;
    buf_level_t *l2 = buf_hierarchy_get_level(bh, BUF_LEVEL_L2_GLOBAL);
    if (!l2) return -1;

    dma_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.direction = DMA_L2_TO_DRAM;
    desc.src_level = l2;
    desc.dst_level = buf_hierarchy_get_level(bh, BUF_LEVEL_DRAM);
    desc.src_addr = 0;
    desc.dst_addr = 0;
    desc.size_bytes = size_bytes;
    desc.pattern = BUF_PATTERN_SEQUENTIAL;

    return dma_submit(&bh->dma, &desc);
}

int dma_2d_block_transfer(buffer_hierarchy_t *bh,
                           dma_direction_t dir,
                           uint32_t src_addr, uint32_t dst_addr,
                           uint32_t block_w, uint32_t block_h,
                           uint32_t src_row_stride, uint32_t dst_row_stride) {
    if (!bh) return -1;

    dma_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.direction = dir;
    desc.src_addr = src_addr;
    desc.dst_addr = dst_addr;
    desc.size_bytes = block_w * block_h;
    desc.pattern = BUF_PATTERN_2D_BLOCK;
    desc.block_width = block_w;
    desc.block_height = block_h;
    desc.block_row_stride = src_row_stride;
    desc.stride_bytes = dst_row_stride;

    return dma_submit(&bh->dma, &desc);
}

/* ================================================================
 * Double buffering management
 * ================================================================ */

void buf_double_buffer_flip(buffer_hierarchy_t *bh, buf_level_e lv) {
    if (!bh) return;
    buf_level_t *bl = buf_hierarchy_get_level(bh, lv);
    if (!bl) return;
    buf_level_swap_bank(bl);
}

bool buf_double_buffer_ready(const buffer_hierarchy_t *bh, buf_level_e lv) {
    (void)lv;
    if (!bh) return false;
    /* Check if DMA to inactive bank is complete */
    return dma_is_idle(&bh->dma);
}

/* ================================================================
 * Energy estimation
 * ================================================================ */

double buf_estimate_access_energy(const buffer_hierarchy_t *bh, buf_level_e lv,
                                   bool is_read, uint32_t bytes) {
    if (!bh || lv >= BUF_MAX_LEVELS) return 0.0;
    double energy_per_byte = is_read
        ? bh->energy_per_read_pj[lv]
        : bh->energy_per_write_pj[lv];
    return energy_per_byte * (double)bytes;
}

/* ================================================================
 * Print / debug utilities
 * ================================================================ */

void buf_hierarchy_print_layout(const buffer_hierarchy_t *bh) {
    if (!bh) return;
    printf("=== Buffer Hierarchy Layout ===\n");
    for (uint32_t i = 0; i < bh->level_count; i++) {
        const buf_level_t *bl = &bh->levels[i];
        printf("  Level %u: %s (%u KB, lat=%u, BW=%.1f GB/s, free=%u)\n",
               i, bl->name,
               bl->total_size_bytes / 1024,
               bl->access_latency_cycles,
               bl->bandwidth_gb_s,
               bl->free_bytes);
        for (uint32_t j = 0; j < bl->num_segments; j++) {
            const buf_segment_t *seg = &bl->segments[j];
            if (seg->used_bytes > 0) {
                printf("    seg[%u]: %s addr=0x%04x size=%u used=%u\n",
                       j, seg->tag, seg->base_addr, seg->size_bytes, seg->used_bytes);
            }
        }
    }
    printf("  DMA: %u channels, %.1f GB/s, %llu bytes transferred\n",
           bh->dma.num_channels, bh->dma.aggregate_bw_gb_s,
           (unsigned long long)bh->dma.total_bytes_transferred);
}

void buf_print_dma_status(const dma_engine_t *dma) {
    if (!dma) return;
    printf("=== DMA Status ===\n");
    printf("  Active transfers: %u\n", dma->active_transfers);
    printf("  Prefetch queue:   %u entries\n", dma->prefetch_count);
    printf("  Total bytes:      %llu\n", (unsigned long long)dma->total_bytes_transferred);
    for (uint32_t i = 0; i < dma->num_channels; i++) {
        const dma_descriptor_t *ch = &dma->channels[i];
        printf("  Channel %u: %s size=%u status=%d\n",
               i,
               ch->direction == DMA_DRAM_TO_L2 ? "DRAM→L2" :
               ch->direction == DMA_L2_TO_L1 ? "L2→L1" :
               ch->direction == DMA_L2_TO_DRAM ? "L2→DRAM" :
               ch->direction == DMA_L1_TO_L2 ? "L1→L2" : "?",
               ch->size_bytes, ch->status);
    }
}