#ifndef ARRAY_PARTITION_H
#define ARRAY_PARTITION_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum {
    ARRAY_PART_NONE,
    ARRAY_PART_BLOCK,
    ARRAY_PART_CYCLIC,
    ARRAY_PART_COMPLETE
} HlsArrayPartType;

typedef struct {
    uint32_t         size;
    uint32_t         partition_factor;
    HlsArrayPartType part_type;
} HlsArrayDim;

typedef enum {
    MEM_BRAM,
    MEM_LUTRAM,
    MEM_URAM,
    MEM_DISTRIBUTED,
    MEM_AUTO
} HlsMemType;

typedef struct {
    uint32_t     array_id;
    char         name[128];
    uint32_t     elem_width;
    uint32_t     num_dims;
    HlsArrayDim *dims;
    uint32_t     total_elements;
    HlsMemType   mem_type;
    bool         is_partitioned;
    bool         is_reshaped;
    uint32_t     num_banks;
    uint32_t     num_ports;
    bool         dual_port;
    uint32_t     bank_conflicts;
    char         resource_tag[64];
} HlsArray;

typedef struct {
    uint32_t         dim;
    HlsArrayPartType type;
    uint32_t         factor;
    bool             complete;
} HlsPartConfig;

typedef struct {
    uint32_t         dim;
    uint32_t         factor;
    HlsArrayPartType type;
} HlsReshapeConfig;

typedef struct {
    uint32_t num_banks;
    uint32_t bank_depth;
    uint32_t ports_per_bank;
    uint32_t total_ports;
    bool     has_conflicts;
    uint32_t conflict_count;
    char     recommendation[256];
} HlsBankingReport;

typedef struct {
    uint32_t  element_count;
    uint32_t  element_width;
    bool      fits_in_bram;
    bool      fits_in_lutram;
    uint32_t  bram_count;
    uint32_t  lut_usage;
    uint32_t  ff_usage;
    HlsMemType recommended;
    char      rationale[256];
} HlsMemoryTradeoff;

HlsArray* hls_array_create(const char *name, uint32_t elem_width,
            uint32_t num_dims);
void      hls_array_destroy(HlsArray *arr);
bool      hls_array_set_dim(HlsArray *arr, uint32_t dim, uint32_t size);

bool hls_array_partition_block(HlsArray *arr, uint32_t dim, uint32_t factor);
bool hls_array_partition_cyclic(HlsArray *arr, uint32_t dim, uint32_t factor);
bool hls_array_partition_complete(HlsArray *arr, uint32_t dim);
bool hls_array_partition_configured(HlsArray *arr, const HlsPartConfig *cfg);
bool hls_array_partition_legal(HlsArray *arr, const HlsPartConfig *cfg);

bool hls_array_reshape(HlsArray *arr, const HlsReshapeConfig *cfg);
bool hls_array_reshape_block(HlsArray *arr, uint32_t dim, uint32_t factor);
bool hls_array_reshape_cyclic(HlsArray *arr, uint32_t dim, uint32_t factor);

bool     hls_array_bank_analyze(HlsArray *arr, HlsBankingReport *report);
bool     hls_array_bank_assign(HlsArray *arr, uint32_t num_banks);
uint32_t hls_array_get_bank(HlsArray *arr, uint32_t elem_index);
bool     hls_array_bank_conflict_detect(HlsArray *arr,
             const uint32_t *access_indices, uint32_t num_accesses);

HlsMemoryTradeoff hls_memory_tradeoff_analyze(uint32_t elems, uint32_t width);
bool              hls_array_set_memory_type(HlsArray *arr, HlsMemType type);
HlsMemType        hls_array_recommend_memory(HlsArray *arr);

bool hls_array_set_ports(HlsArray *arr, uint32_t num_ports, bool dual_port);
bool hls_array_can_access_parallel(HlsArray *arr, uint32_t num_accesses);

void hls_array_print_layout(HlsArray *arr, FILE *out);
void hls_array_print_banking(HlsArray *arr, FILE *out);

#endif
