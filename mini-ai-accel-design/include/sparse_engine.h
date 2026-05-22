#ifndef SPARSE_ENGINE_H
#define SPARSE_ENGINE_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { SPARSE_UNSTRUCTURED, SPARSE_STRUCTURED_2_4, SPARSE_BLOCK_4x4 } SparseFormat;

typedef struct {
    SparseFormat format;
    double sparsity; /* 0..1 */
    int total_elements;
    int nonzero_count;
    uint32_t *bitmask;
    double *values;
    int *indices;
} SparseTensor;

typedef struct {
    bool enabled;
    double speedup;
    double energy_saving;
    int skipped_macs;
    SparseFormat supported_format;
} SparseEngine;

void sparse_init_tensor(SparseTensor *t, int elements, double sparsity, SparseFormat fmt);
void sparse_engine_init(SparseEngine *se, SparseFormat fmt);
double sparse_engine_eie(SparseEngine *se, SparseTensor *a, SparseTensor *b); /* EIE-like: weight-sparse */
double sparse_engine_block_sparse(SparseEngine *se, SparseTensor *a, int block_size);
double sparse_engine_balanced(SparseEngine *se, SparseTensor *a);
void sparse_print_stats(SparseEngine *se, SparseTensor *t);
#endif
