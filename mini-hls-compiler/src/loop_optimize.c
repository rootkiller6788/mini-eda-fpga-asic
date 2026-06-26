#include "loop_optimize.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define HLS_MAX_LOOPS    256
#define HLS_MAX_CHILDREN 16

HlsLoopNest* hls_loop_nest_create(void)
{
    HlsLoopNest *nest = calloc(1, sizeof(HlsLoopNest));
    if (!nest) return NULL;
    nest->loops = calloc(HLS_MAX_LOOPS, sizeof(HlsLoop*));
    if (!nest->loops) { free(nest); return NULL; }
    return nest;
}

void hls_loop_nest_destroy(HlsLoopNest *nest)
{
    if (!nest) return;
    for (uint32_t i = 0; i < nest->num_loops; i++) {
        HlsLoop *l = nest->loops[i];
        if (l) { free(l->children); free(l); }
    }
    free(nest->loops);
    free(nest);
}

HlsLoop* hls_loop_add(HlsLoopNest *nest, HlsLoop *parent,
        LoopType type, const char *label)
{
    if (!nest || nest->num_loops >= HLS_MAX_LOOPS) return NULL;
    HlsLoop *loop = calloc(1, sizeof(HlsLoop));
    if (!loop) return NULL;
    loop->loop_id = nest->num_loops;
    loop->type = type;
    strncpy(loop->label, label ? label : "", sizeof(loop->label) - 1);
    loop->parent = parent;
    loop->depth = parent ? parent->depth + 1 : 0;
    loop->children = calloc(HLS_MAX_CHILDREN, sizeof(HlsLoop*));
    if (!loop->children) { free(loop); return NULL; }
    if (parent && parent->num_children < HLS_MAX_CHILDREN)
        parent->children[parent->num_children++] = loop;
    if (!nest->root) nest->root = loop;
    nest->loops[nest->num_loops++] = loop;
    if (loop->depth + 1 > nest->max_depth)
        nest->max_depth = loop->depth + 1;
    return loop;
}

void hls_loop_set_trip_count(HlsLoop *loop, int64_t count)
{
    if (!loop) return;
    loop->trip_count = count;
}

bool hls_loop_unroll_partial(HlsLoop *loop, uint32_t factor)
{
    if (!loop || factor < 2) return false;
    if (!hls_loop_unroll_is_legal(loop, factor)) return false;
    loop->unroll_factor = factor;
    loop->unroll_kind = UNROLL_PARTIAL;
    if (loop->trip_count > 0)
        loop->trip_count = loop->trip_count / (int64_t)factor;
    return true;
}

bool hls_loop_unroll_complete(HlsLoop *loop)
{
    if (!loop) return false;
    if (loop->trip_count < 0) {
        HlsTripCount tc = hls_analyze_trip_count(loop);
        if (!tc.is_constant) return false;
    }
    loop->unroll_factor = 1;
    loop->unroll_kind = UNROLL_COMPLETE;
    loop->trip_count = 0;
    return true;
}

bool hls_loop_unroll_configured(HlsLoop *loop, const HlsUnrollConfig *cfg)
{
    if (!loop || !cfg) return false;
    if (cfg->factor == 0)
        return hls_loop_unroll_complete(loop);
    if (cfg->factor > 0)
        return hls_loop_unroll_partial(loop, cfg->factor);
    return false;
}

bool hls_loop_unroll_is_legal(HlsLoop *loop, uint32_t factor)
{
    if (!loop || factor == 0) return false;
    HlsTripCount tc = hls_analyze_trip_count(loop);
    if (tc.is_constant && tc.min_trips > 0) {
        if ((uint64_t)tc.min_trips % factor != 0)
            return false;
    }
    return true;
}

bool hls_loop_flatten(HlsLoopNest *nest, const HlsFlattenConfig *cfg)
{
    if (!nest || !cfg) return false;
    bool any = false;
    for (uint32_t i = 0; i < nest->num_loops; i++) {
        HlsLoop *l = nest->loops[i];
        if (!l || !l->parent) continue;
        if (!cfg->flatten_all && l->depth >= cfg->max_depth)
            continue;
        if (hls_loop_flatten_pair(l->parent, l))
            any = true;
    }
    return any;
}

bool hls_loop_flatten_pair(HlsLoop *outer, HlsLoop *inner)
{
    if (!outer || !inner || inner->parent != outer) return false;
    if (inner->unroll_kind != UNROLL_NONE)
        return false;
    inner->flattened = true;
    if (outer->trip_count > 0 && inner->trip_count > 0)
        outer->trip_count = outer->trip_count * inner->trip_count;
    return true;
}

bool hls_loop_merge(HlsLoop *loop1, HlsLoop *loop2)
{
    if (!loop1 || !loop2) return false;
    if (loop1->parent != loop2->parent) return false;
    if (loop1->type != loop2->type) return false;
    if (loop1->trip_count != loop2->trip_count) return false;
    /* merge: loop2 absorbed into loop1 */
    if (loop2->parent && loop2->parent->num_children > 0) {
        for (uint32_t i = 0; i < loop2->parent->num_children; i++) {
            if (loop2->parent->children[i] == loop2) {
                loop2->parent->children[i] = loop1;
                break;
            }
        }
    }
    loop2->flattened = true;
    return true;
}

uint32_t hls_loop_merge_consecutive(HlsLoopNest *nest,
        const HlsMergeConfig *cfg)
{
    if (!nest || !cfg) return 0;
    uint32_t merged = 0;
    uint32_t gap = 0;
    for (uint32_t i = 0; i < nest->num_loops; i++) {
        HlsLoop *l = nest->loops[i];
        if (!l || l->flattened) continue;
        for (uint32_t j = i + 1; j < nest->num_loops; j++) {
            HlsLoop *k = nest->loops[j];
            if (!k || k->flattened) continue;
            if (k->parent == l->parent && gap <= cfg->max_gap) {
                if (hls_loop_merge(l, k)) merged++;
                gap = 0;
            } else {
                gap++;
                if (gap > cfg->max_gap) break;
            }
        }
    }
    return merged;
}

bool hls_loop_rewind_configure(HlsLoop *loop, const HlsRewindConfig *cfg)
{
    if (!loop || !cfg) return false;
    loop->is_pipelined = cfg->enabled;
    if (cfg->enabled && loop->pipeline_ii == 0)
        loop->pipeline_ii = 1;
    return true;
}

bool hls_loop_rewind_enable(HlsLoop *loop, bool en)
{
    if (!loop) return false;
    HlsRewindConfig rwc = { en, 0, false };
    return hls_loop_rewind_configure(loop, &rwc);
}

void hls_loop_rewind_reset(HlsLoop *loop)
{
    if (!loop) return;
    HlsRewindConfig rwc = { false, 0, false };
    hls_loop_rewind_configure(loop, &rwc);
}

HlsTripCount hls_analyze_trip_count(HlsLoop *loop)
{
    HlsTripCount tc = {0, 0, 0, false, false, ""};
    if (!loop) return tc;
    if (loop->trip_count > 0) {
        tc.min_trips = loop->trip_count;
        tc.max_trips = loop->trip_count;
        tc.avg_trips = loop->trip_count;
        tc.is_constant = true;
        tc.is_affine = true;
        snprintf(tc.bound_desc, sizeof(tc.bound_desc),
            "Constant: %lld", (long long)loop->trip_count);
    } else {
        tc.min_trips = 0;
        tc.max_trips = 1000000;
        tc.avg_trips = 1000;
        tc.is_constant = false;
        tc.is_affine = false;
        snprintf(tc.bound_desc, sizeof(tc.bound_desc),
            "Unknown bound");
    }
    return tc;
}

int64_t hls_trip_count_ceil_div(int64_t n, int64_t d)
{
    if (d == 0) return INT64_MAX;
    return (n + d - 1) / d;
}

bool hls_trip_count_can_compute(HlsLoop *loop)
{
    HlsTripCount tc = hls_analyze_trip_count(loop);
    return tc.is_constant;
}

bool hls_loop_pipeline_set_ii(HlsLoop *loop, uint32_t ii)
{
    if (!loop || ii == 0) return false;
    loop->pipeline_ii = ii;
    loop->is_pipelined = true;
    return true;
}

bool hls_loop_pipeline_verify(HlsLoopNest *nest)
{
    if (!nest) return false;
    for (uint32_t i = 0; i < nest->num_loops; i++) {
        HlsLoop *l = nest->loops[i];
        if (!l || !l->is_pipelined) continue;
        if (l->pipeline_ii == 0) return false;
    }
    return true;
}

void hls_loop_pipeline_print(HlsLoopNest *nest, FILE *out)
{
    if (!nest || !out) return;
    for (uint32_t i = 0; i < nest->num_loops; i++) {
        HlsLoop *l = nest->loops[i];
        if (!l) continue;
        fprintf(out, "Loop[%u] \"%s\": type=%d depth=%u trip=%lld "
            "pipelined=%d II=%u unroll=%d\n",
            l->loop_id, l->label, l->type, l->depth,
            (long long)l->trip_count,
            l->is_pipelined, l->pipeline_ii, l->unroll_kind);
    }
}
