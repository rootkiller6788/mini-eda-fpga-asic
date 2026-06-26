#ifndef BUFFER_HIERARCHY_H
#define BUFFER_HIERARCHY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ================================================================
 * Buffer Hierarchy — Multi-level on-chip memory for AI accelerators
 * Inspired by Eyeriss spatial architecture buffer hierarchy,
 * Google TPU unified buffer design, and Simba chiplet buffer mesh.
 *
 * L1: Buffer descriptor, address space, DMA descriptor
 * L2: Memory hierarchy concepts (L1/L2/DRAM), data reuse
 * L3: Double buffering, ping-pong, DMA engine
 * L5: Prefetch scheduling, replacement policy
 * L7: Application — memory-aware scheduling for CNN layers
 * ================================================================ */

#define BUF_MAX_LEVELS       4
#define BUF_MAX_SEGMENTS     16
#define BUF_MAX_DMA_CHANNELS 8
#define BUF_NAME_LEN         64
#define BUF_MAX_PREFETCH_Q   32

/* --- Buffer Level Type --- */
typedef enum {
    BUF_LEVEL_L1_LOCAL,       /* per-PE scratchpad */
    BUF_LEVEL_L2_GLOBAL,      /* shared global buffer */
    BUF_LEVEL_L3_LLC,         /* last-level on-chip cache */
    BUF_LEVEL_DRAM            /* off-chip DRAM */
} buf_level_e;

/* --- Buffer Access Pattern --- */
typedef enum {
    BUF_PATTERN_SEQUENTIAL,
    BUF_PATTERN_STRIDED,
    BUF_PATTERN_2D_BLOCK,     /* image/tensor tiling */
    BUF_PATTERN_RANDOM,
    BUF_PATTERN_BROADCAST
} buf_access_pattern_t;

/* --- DMA Direction --- */
typedef enum {
    DMA_DRAM_TO_L2,
    DMA_L2_TO_L1,
    DMA_L1_TO_L2,
    DMA_L2_TO_DRAM,
    DMA_L1_TO_L1,
    DMA_BROADCAST_L2_TO_L1
} dma_direction_t;

/* --- DMA Status --- */
typedef enum {
    DMA_IDLE,
    DMA_PENDING,
    DMA_IN_FLIGHT,
    DMA_COMPLETE,
    DMA_ERROR
} dma_status_t;

/* --- Buffer Segment --- */
typedef struct {
    uint32_t    base_addr;
    uint32_t    size_bytes;
    uint32_t    used_bytes;
    bool        locked;
    bool        dirty;
    uint32_t    last_access_cycle;
    char        tag[BUF_NAME_LEN];
} buf_segment_t;

/* --- Buffer Level --- */
typedef struct {
    buf_level_e     level;
    char            name[BUF_NAME_LEN];
    uint32_t        total_size_bytes;
    uint32_t        free_bytes;
    uint32_t        line_size_bytes;
    uint32_t        access_latency_cycles;
    double          bandwidth_gb_s;
    uint32_t        num_segments;
    buf_segment_t   segments[BUF_MAX_SEGMENTS];
    /* Double buffering */
    bool            double_buffered;
    uint32_t        active_bank;
    /* Statistics */
    uint64_t        read_count;
    uint64_t        write_count;
    uint64_t        read_bytes;
    uint64_t        write_bytes;
    uint64_t        hit_count;
    uint64_t        miss_count;
    uint64_t        conflict_count;
} buf_level_t;

/* --- DMA Transfer Descriptor --- */
typedef struct {
    dma_direction_t     direction;
    buf_level_t        *src_level;
    buf_level_t        *dst_level;
    uint32_t            src_addr;
    uint32_t            dst_addr;
    uint32_t            size_bytes;
    buf_access_pattern_t pattern;
    uint32_t            stride_bytes;
    uint32_t            block_width;
    uint32_t            block_height;
    uint32_t            block_row_stride;
    uint32_t            priority;
    uint32_t            issued_cycle;
    uint32_t            complete_cycle;
    dma_status_t        status;
    /* Callback */
    void               (*on_complete)(void *userdata);
    void               *userdata;
} dma_descriptor_t;

/* --- DMA Engine --- */
typedef struct {
    dma_descriptor_t    channels[BUF_MAX_DMA_CHANNELS];
    bool                channel_busy[BUF_MAX_DMA_CHANNELS];
    uint32_t            num_channels;
    uint32_t            active_transfers;
    double              aggregate_bw_gb_s;
    uint64_t            total_bytes_transferred;
    uint64_t            total_transfer_cycles;
    /* Prefetch queue */
    dma_descriptor_t    prefetch_queue[BUF_MAX_PREFETCH_Q];
    uint32_t            prefetch_head;
    uint32_t            prefetch_tail;
    uint32_t            prefetch_count;
} dma_engine_t;

/* --- Buffer Hierarchy --- */
typedef struct {
    buf_level_t     levels[BUF_MAX_LEVELS];
    uint32_t        level_count;
    dma_engine_t    dma;
    uint32_t        global_cycle;
    /* Energy model */
    double          energy_per_read_pj[BUF_MAX_LEVELS];
    double          energy_per_write_pj[BUF_MAX_LEVELS];
    double          total_energy_pj;
} buffer_hierarchy_t;

/* --- Memory Access Trace Entry --- */
typedef struct {
    uint32_t    cycle;
    buf_level_t level;
    uint32_t    addr;
    uint32_t    size;
    bool        is_read;
} buf_access_trace_t;

/* --- Buffer Performance Metrics --- */
typedef struct {
    double      avg_bw_utilization;
    double      avg_latency_cycles;
    double      hit_rate;
    double      total_energy_uj;
    uint64_t    total_bytes_moved;
    uint64_t    dma_cycles;
    double      double_buffer_efficiency;
    double      prefetch_accuracy;
} buf_perf_t;

/* API — Buffer Level */
void      buf_level_init(buf_level_t *bl, buf_level_e lv, const char *name,
                          uint32_t size_bytes, uint32_t latency, double bw_gbs);
uint32_t  buf_level_alloc(buf_level_t *bl, uint32_t size_bytes, const char *tag);
void      buf_level_free(buf_level_t *bl, uint32_t seg_id);
bool      buf_level_can_fit(const buf_level_t *bl, uint32_t size_bytes);
double    buf_level_utilization(const buf_level_t *bl);
void      buf_level_reset_stats(buf_level_t *bl);
void      buf_level_set_double_buffer(buf_level_t *bl, bool enabled);
void      buf_level_swap_bank(buf_level_t *bl);

/* API — DMA Engine */
void      dma_engine_init(dma_engine_t *dma, uint32_t channels, double bw_gbs);
int       dma_submit(dma_engine_t *dma, const dma_descriptor_t *desc);
void      dma_tick(dma_engine_t *dma, buffer_hierarchy_t *bh);
bool      dma_is_idle(const dma_engine_t *dma);
uint32_t  dma_pending_transfers(const dma_engine_t *dma);
void      dma_wait_all(dma_engine_t *dma, buffer_hierarchy_t *bh);
int       dma_prefetch(dma_engine_t *dma, const dma_descriptor_t *desc);
void      dma_reset(dma_engine_t *dma);

/* API — Buffer Hierarchy */
void      buf_hierarchy_init(buffer_hierarchy_t *bh);
int       buf_hierarchy_add_level(buffer_hierarchy_t *bh, buf_level_e lv,
                                   const char *name, uint32_t size, uint32_t lat, double bw);
buf_level_t *buf_hierarchy_get_level(buffer_hierarchy_t *bh, buf_level_e lv);
void      buf_hierarchy_tick(buffer_hierarchy_t *bh);
void      buf_hierarchy_collect_perf(const buffer_hierarchy_t *bh, buf_perf_t *perf);
void      buf_hierarchy_print_perf(const buf_perf_t *perf);
void      buf_hierarchy_reset(buffer_hierarchy_t *bh);

/* DMA helpers for common patterns */
int       dma_load_weights(buffer_hierarchy_t *bh, const double *weights,
                            uint32_t size_bytes);
int       dma_load_inputs(buffer_hierarchy_t *bh, const double *inputs,
                           uint32_t size_bytes);
int       dma_store_outputs(buffer_hierarchy_t *bh, double *outputs,
                             uint32_t size_bytes);
int       dma_2d_block_transfer(buffer_hierarchy_t *bh,
                                 dma_direction_t dir,
                                 uint32_t src_addr, uint32_t dst_addr,
                                 uint32_t block_w, uint32_t block_h,
                                 uint32_t src_row_stride, uint32_t dst_row_stride);

/* Double buffering management */
void      buf_double_buffer_flip(buffer_hierarchy_t *bh, buf_level_e lv);
bool      buf_double_buffer_ready(const buffer_hierarchy_t *bh, buf_level_e lv);

/* Energy estimation */
double    buf_estimate_access_energy(const buffer_hierarchy_t *bh, buf_level_e lv,
                                      bool is_read, uint32_t bytes);

/* Print / debug */
void      buf_hierarchy_print_layout(const buffer_hierarchy_t *bh);
void      buf_print_dma_status(const dma_engine_t *dma);

#endif /* BUFFER_HIERARCHY_H */