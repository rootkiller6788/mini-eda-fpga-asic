#include <stdio.h>
#include <stdlib.h>
#include "loop_optimize.h"

int main(void)
{
    printf("=== mini-hls-compiler: Loop Optimization Example ===\n\n");

    HlsLoopNest *nest = hls_loop_nest_create();

    HlsLoop *outer = hls_loop_add(nest, NULL, LOOP_FOR, "outer_loop");
    hls_loop_set_trip_count(outer, 64);

    HlsLoop *inner = hls_loop_add(nest, outer, LOOP_FOR, "inner_loop");
    hls_loop_set_trip_count(inner, 128);

    HlsLoop *inner2 = hls_loop_add(nest, outer, LOOP_FOR, "inner_loop2");
    hls_loop_set_trip_count(inner2, 128);

    printf("Loop nest: %u loops, max depth=%u\n",
        nest->num_loops, nest->max_depth);

    HlsTripCount tc_inner = hls_analyze_trip_count(inner);
    printf("Inner trip count: %s\n", tc_inner.bound_desc);

    printf("\n--- Unroll ---\n");
    if (hls_loop_unroll_partial(inner, 4))
        printf("inner_loop partially unrolled by factor 4, "
            "new trip: %lld\n", (long long)inner->trip_count);

    if (hls_loop_unroll_is_legal(outer, 8))
        printf("outer_loop legal to unroll by 8\n");

    HlsUnrollConfig ucfg = { 2, false, true, "" };
    if (hls_loop_unroll_configured(outer, &ucfg))
        printf("outer_loop configured unroll factor=2\n");

    printf("\n--- Flatten ---\n");
    HlsFlattenConfig fcfg = { false, 1, false };
    if (hls_loop_flatten(nest, &fcfg))
        printf("Loops flattened\n");
    if (hls_loop_flatten_pair(outer, inner2))
        printf("inner_loop2 flattened into outer_loop\n");

    printf("\n--- Merge ---\n");
    HlsLoop *loopA = hls_loop_add(nest, NULL, LOOP_FOR, "loopA");
    hls_loop_set_trip_count(loopA, 100);
    HlsLoop *loopB = hls_loop_add(nest, NULL, LOOP_FOR, "loopB");
    hls_loop_set_trip_count(loopB, 100);

    if (hls_loop_merge(loopA, loopB))
        printf("loopB merged into loopA\n");

    HlsMergeConfig mcfg = { false, 0 };
    uint32_t merged = hls_loop_merge_consecutive(nest, &mcfg);
    printf("Consecutive merges: %u\n", merged);

    printf("\n--- Rewind ---\n");
    HlsRewindConfig rwc = { true, 4, true };
    if (hls_loop_rewind_configure(inner, &rwc))
        printf("Rewind enabled on inner_loop\n");
    hls_loop_rewind_reset(inner);

    printf("\n--- Pipeline ---\n");
    hls_loop_pipeline_set_ii(inner, 1);
    hls_loop_pipeline_set_ii(outer, 3);

    if (hls_loop_pipeline_verify(nest))
        printf("Loop pipeline verification passed\n");
    else
        printf("Loop pipeline verification failed\n");

    printf("\n--- Report ---\n");
    hls_loop_pipeline_print(nest, stdout);

    int64_t cd = hls_trip_count_ceil_div(127, 8);
    printf("\nceil_div(127, 8) = %lld\n", (long long)cd);

    hls_loop_nest_destroy(nest);
    printf("Done.\n");
    return 0;
}
