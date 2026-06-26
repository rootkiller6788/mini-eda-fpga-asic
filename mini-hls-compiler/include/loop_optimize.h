#ifndef LOOP_OPTIMIZE_H
#define LOOP_OPTIMIZE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum {
    LOOP_FOR,
    LOOP_WHILE,
    LOOP_DO_WHILE
} LoopType;

typedef enum {
    UNROLL_NONE,
    UNROLL_PARTIAL,
    UNROLL_COMPLETE
} UnrollKind;

typedef struct hls_loop {
    uint32_t          loop_id;
    LoopType          type;
    int64_t           trip_count;
    uint32_t          unroll_factor;
    UnrollKind        unroll_kind;
    bool              is_pipelined;
    uint32_t          pipeline_ii;
    uint32_t          depth;
    struct hls_loop  *parent;
    struct hls_loop **children;
    uint32_t          num_children;
    bool              flattened;
    char              label[128];
} HlsLoop;

typedef struct {
    HlsLoop  *root;
    HlsLoop **loops;
    uint32_t  num_loops;
    uint32_t  max_depth;
} HlsLoopNest;

typedef struct {
    uint32_t factor;
    bool     skip_check;
    bool     keep_order;
    char     target_label[128];
} HlsUnrollConfig;

typedef struct {
    bool     flatten_all;
    uint32_t max_depth;
    bool     allow_duplicate;
} HlsFlattenConfig;

typedef struct {
    bool     merge_all;
    uint32_t max_gap;
} HlsMergeConfig;

typedef struct {
    bool     enabled;
    uint32_t rewind_depth;
    bool     continuous;
} HlsRewindConfig;

typedef struct {
    int64_t min_trips;
    int64_t max_trips;
    int64_t avg_trips;
    bool    is_constant;
    bool    is_affine;
    char    bound_desc[256];
} HlsTripCount;

HlsLoopNest* hls_loop_nest_create(void);
void         hls_loop_nest_destroy(HlsLoopNest *nest);
HlsLoop*     hls_loop_add(HlsLoopNest *nest, HlsLoop *parent,
                LoopType type, const char *label);
void         hls_loop_set_trip_count(HlsLoop *loop, int64_t count);

bool hls_loop_unroll_partial(HlsLoop *loop, uint32_t factor);
bool hls_loop_unroll_complete(HlsLoop *loop);
bool hls_loop_unroll_configured(HlsLoop *loop, const HlsUnrollConfig *cfg);
bool hls_loop_unroll_is_legal(HlsLoop *loop, uint32_t factor);

bool hls_loop_flatten(HlsLoopNest *nest, const HlsFlattenConfig *cfg);
bool hls_loop_flatten_pair(HlsLoop *outer, HlsLoop *inner);

bool     hls_loop_merge(HlsLoop *loop1, HlsLoop *loop2);
uint32_t hls_loop_merge_consecutive(HlsLoopNest *nest,
             const HlsMergeConfig *cfg);

bool hls_loop_rewind_configure(HlsLoop *loop, const HlsRewindConfig *cfg);
bool hls_loop_rewind_enable(HlsLoop *loop, bool en);
void hls_loop_rewind_reset(HlsLoop *loop);

HlsTripCount hls_analyze_trip_count(HlsLoop *loop);
int64_t      hls_trip_count_ceil_div(int64_t n, int64_t d);
bool         hls_trip_count_can_compute(HlsLoop *loop);

bool hls_loop_pipeline_set_ii(HlsLoop *loop, uint32_t ii);
bool hls_loop_pipeline_verify(HlsLoopNest *nest);
void hls_loop_pipeline_print(HlsLoopNest *nest, FILE *out);

#endif
