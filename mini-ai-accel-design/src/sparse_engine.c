#include "sparse_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sparse_init_tensor(SparseTensor *t, int elements, double sparsity, SparseFormat fmt) {
    memset(t, 0, sizeof(*t));
    t->format = fmt; t->sparsity = sparsity;
    t->total_elements = elements;
    t->nonzero_count = (int)(elements * (1.0 - sparsity));
    if (t->nonzero_count < 1) t->nonzero_count = 1;
    t->values = (double*)calloc((size_t)t->nonzero_count, sizeof(double));
    t->indices = (int*)calloc((size_t)t->nonzero_count, sizeof(int));
    for (int i = 0; i < t->nonzero_count; i++) { t->values[i] = 1.0; t->indices[i] = i * 2; }
}

void sparse_engine_init(SparseEngine *se, SparseFormat fmt) {
    memset(se, 0, sizeof(*se));
    se->enabled = true; se->supported_format = fmt;
}

double sparse_engine_eie(SparseEngine *se, SparseTensor *a, SparseTensor *b) {
    /* EIE: Efficient Inference Engine for weight-sparse computation */
    (void)b;
    if (!se->enabled) return 1.0;
    se->skipped_macs = a->total_elements - a->nonzero_count;
    se->speedup = (double)a->total_elements / a->nonzero_count;
    se->energy_saving = 1.0 - 1.0 / se->speedup;
    return se->speedup;
}

double sparse_engine_block_sparse(SparseEngine *se, SparseTensor *a, int block_size) {
    if (!se->enabled) return 1.0;
    int blocks = a->total_elements / (block_size * block_size);
    int nonzero_blocks = (int)(blocks * (1.0 - a->sparsity));
    if (nonzero_blocks < 1) nonzero_blocks = 1;
    se->speedup = (double)blocks / nonzero_blocks;
    se->energy_saving = 1.0 - 1.0 / se->speedup;
    se->skipped_macs = (blocks - nonzero_blocks) * block_size * block_size;
    return se->speedup;
}

double sparse_engine_balanced(SparseEngine *se, SparseTensor *a) {
    if (!se->enabled) return 1.0;
    /* Balanced sparsity: equal nonzero per block */
    se->speedup = 1.0 / (1.0 - a->sparsity);
    se->energy_saving = 1.0 - (1.0 - a->sparsity);
    se->skipped_macs = (int)(a->total_elements * a->sparsity);
    return se->speedup;
}

void sparse_print_stats(SparseEngine *se, SparseTensor *t) {
    const char *fmts[] = {"Unstructured","2:4 Structured","Block 4x4"};
    printf("=== Sparse Engine ===\n");
    printf("  Format: %s, Sparsity: %.1f%%\n", fmts[t->format], t->sparsity * 100);
    printf("  Nonzeros: %d/%d\n", t->nonzero_count, t->total_elements);
    printf("  Speedup: %.2fx, Energy saved: %.1f%%\n", se->speedup, se->energy_saving * 100);
}
