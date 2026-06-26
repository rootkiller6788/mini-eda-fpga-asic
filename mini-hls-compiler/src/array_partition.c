#include "array_partition.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define HLS_MAX_DIMS      8
#define HLS_BRAM_THRESH   128
#define HLS_MAX_BANKS     16

HlsArray* hls_array_create(const char *name, uint32_t elem_width,
        uint32_t num_dims)
{
    if (num_dims > HLS_MAX_DIMS) return NULL;
    HlsArray *arr = calloc(1, sizeof(HlsArray));
    if (!arr) return NULL;
    arr->array_id = 0;
    strncpy(arr->name, name ? name : "", sizeof(arr->name)-1);
    arr->elem_width = elem_width;
    arr->num_dims = num_dims;
    arr->dims = calloc(num_dims, sizeof(HlsArrayDim));
    if (!arr->dims) { free(arr); return NULL; }
    arr->mem_type = MEM_AUTO;
    arr->num_ports = 1;
    arr->dual_port = false;
    arr->num_banks = 1;
    return arr;
}

void hls_array_destroy(HlsArray *arr)
{
    if (!arr) return;
    free(arr->dims);
    free(arr);
}

bool hls_array_set_dim(HlsArray *arr, uint32_t dim, uint32_t size)
{
    if (!arr || dim >= arr->num_dims) return false;
    arr->dims[dim].size = size;
    arr->dims[dim].partition_factor = 1;
    arr->dims[dim].part_type = ARRAY_PART_NONE;
    arr->total_elements = 1;
    for (uint32_t i = 0; i < arr->num_dims; i++) {
        if (arr->dims[i].size > 0)
            arr->total_elements *= arr->dims[i].size;
    }
    return true;
}

bool hls_array_partition_block(HlsArray *arr, uint32_t dim,
        uint32_t factor)
{
    if (!arr || dim >= arr->num_dims || factor < 2) return false;
    if (arr->dims[dim].size % factor != 0) return false;
    arr->dims[dim].part_type = ARRAY_PART_BLOCK;
    arr->dims[dim].partition_factor = factor;
    arr->is_partitioned = true;
    arr->num_banks *= factor;
    return true;
}

bool hls_array_partition_cyclic(HlsArray *arr, uint32_t dim,
        uint32_t factor)
{
    if (!arr || dim >= arr->num_dims || factor < 2) return false;
    arr->dims[dim].part_type = ARRAY_PART_CYCLIC;
    arr->dims[dim].partition_factor = factor;
    arr->is_partitioned = true;
    arr->num_banks *= factor;
    return true;
}

bool hls_array_partition_complete(HlsArray *arr, uint32_t dim)
{
    if (!arr || dim >= arr->num_dims) return false;
    arr->dims[dim].part_type = ARRAY_PART_COMPLETE;
    arr->dims[dim].partition_factor = arr->dims[dim].size;
    arr->is_partitioned = true;
    arr->num_banks *= arr->dims[dim].size;
    return true;
}

bool hls_array_partition_configured(HlsArray *arr,
        const HlsPartConfig *cfg)
{
    if (!arr || !cfg) return false;
    if (!hls_array_partition_legal(arr, cfg)) return false;
    if (cfg->complete)
        return hls_array_partition_complete(arr, cfg->dim);
    switch (cfg->type) {
        case ARRAY_PART_BLOCK:
            return hls_array_partition_block(arr, cfg->dim, cfg->factor);
        case ARRAY_PART_CYCLIC:
            return hls_array_partition_cyclic(arr, cfg->dim, cfg->factor);
        case ARRAY_PART_COMPLETE:
            return hls_array_partition_complete(arr, cfg->dim);
        default:
            return false;
    }
}

bool hls_array_partition_legal(HlsArray *arr, const HlsPartConfig *cfg)
{
    if (!arr || !cfg || cfg->dim >= arr->num_dims) return false;
    if (cfg->type == ARRAY_PART_BLOCK) {
        if (arr->dims[cfg->dim].size % cfg->factor != 0)
            return false;
    }
    return true;
}

bool hls_array_reshape(HlsArray *arr, const HlsReshapeConfig *cfg)
{
    if (!arr || !cfg || cfg->dim >= arr->num_dims) return false;
    if (arr->dims[cfg->dim].size % cfg->factor != 0) return false;
    if (cfg->type == ARRAY_PART_BLOCK)
        return hls_array_reshape_block(arr, cfg->dim, cfg->factor);
    else if (cfg->type == ARRAY_PART_CYCLIC)
        return hls_array_reshape_cyclic(arr, cfg->dim, cfg->factor);
    return false;
}

bool hls_array_reshape_block(HlsArray *arr, uint32_t dim, uint32_t factor)
{
    if (!arr || dim >= arr->num_dims || factor < 2) return false;
    if (arr->dims[dim].size % factor != 0) return false;
    arr->dims[dim].size /= factor;
    arr->dims[dim].partition_factor = factor;
    arr->elem_width *= factor;
    arr->is_reshaped = true;
    return true;
}

bool hls_array_reshape_cyclic(HlsArray *arr, uint32_t dim, uint32_t factor)
{
    if (!arr || dim >= arr->num_dims || factor < 2) return false;
    arr->dims[dim].partition_factor = factor;
    arr->elem_width *= factor;
    arr->is_reshaped = true;
    return true;
}

bool hls_array_bank_analyze(HlsArray *arr, HlsBankingReport *report)
{
    if (!arr || !report) return false;
    memset(report, 0, sizeof(*report));
    report->num_banks = arr->num_banks;
    report->bank_depth = (arr->num_banks > 0)
        ? arr->total_elements / arr->num_banks : 0;
    report->ports_per_bank = arr->num_ports;
    report->total_ports = arr->num_ports * arr->num_banks;
    report->has_conflicts = (arr->bank_conflicts > 0);
    report->conflict_count = arr->bank_conflicts;
    if (report->has_conflicts)
        snprintf(report->recommendation,
            sizeof(report->recommendation),
            "Detected %u banking conflicts. Increase partition "
            "factor or use cyclic partitioning.",
            arr->bank_conflicts);
    else
        snprintf(report->recommendation,
            sizeof(report->recommendation),
            "No banking conflicts detected.");
    return true;
}

bool hls_array_bank_assign(HlsArray *arr, uint32_t num_banks)
{
    if (!arr || num_banks == 0 || num_banks > HLS_MAX_BANKS)
        return false;
    arr->num_banks = num_banks;
    return true;
}

uint32_t hls_array_get_bank(HlsArray *arr, uint32_t elem_index)
{
    if (!arr || arr->num_banks == 0) return 0;
    return elem_index % arr->num_banks;
}

bool hls_array_bank_conflict_detect(HlsArray *arr,
        const uint32_t *access_indices, uint32_t num_accesses)
{
    if (!arr || !access_indices) return false;
    if (num_accesses > arr->num_banks) {
        arr->bank_conflicts++;
        return true;
    }
    uint32_t banks_seen[HLS_MAX_BANKS] = {0};
    uint32_t unique_banks = 0;
    for (uint32_t i = 0; i < num_accesses; i++) {
        uint32_t b = hls_array_get_bank(arr, access_indices[i]);
        if (!banks_seen[b]) {
            banks_seen[b] = 1;
            unique_banks++;
        }
    }
    if (unique_banks < num_accesses) {
        arr->bank_conflicts++;
        return true;
    }
    return false;
}

HlsMemoryTradeoff hls_memory_tradeoff_analyze(uint32_t elems,
        uint32_t width)
{
    HlsMemoryTradeoff mt;
    memset(&mt, 0, sizeof(mt));
    mt.element_count = elems;
    mt.element_width = width;
    mt.fits_in_bram = (elems >= HLS_BRAM_THRESH);
    mt.fits_in_lutram = (elems <= 256);
    mt.bram_count = mt.fits_in_bram
        ? (elems * width + 16383U) / 16384U : 0;
    mt.lut_usage = mt.fits_in_lutram
        ? (elems * width + 63U) / 64U : 0;
    mt.ff_usage = mt.fits_in_lutram
        ? elems * width : 0;
    mt.recommended = (elems >= HLS_BRAM_THRESH) ? MEM_BRAM : MEM_LUTRAM;
    snprintf(mt.rationale, sizeof(mt.rationale),
        "%u elements x %u bits -> %s recommended",
        elems, width,
        mt.recommended == MEM_BRAM ? "BRAM" : "LUTRAM");
    return mt;
}

bool hls_array_set_memory_type(HlsArray *arr, HlsMemType type)
{
    if (!arr) return false;
    arr->mem_type = type;
    HlsMemoryTradeoff mt = hls_memory_tradeoff_analyze(
        arr->total_elements, arr->elem_width);
    snprintf(arr->resource_tag, sizeof(arr->resource_tag),
        "%s:%ux%u",
        type == MEM_BRAM ? "BRAM" : type == MEM_LUTRAM ? "LUTRAM" :
        type == MEM_URAM ? "URAM" : "AUTO",
        arr->total_elements, arr->elem_width);
    (void)mt;
    return true;
}

HlsMemType hls_array_recommend_memory(HlsArray *arr)
{
    if (!arr) return MEM_AUTO;
    HlsMemoryTradeoff mt = hls_memory_tradeoff_analyze(
        arr->total_elements, arr->elem_width);
    return mt.recommended;
}

bool hls_array_set_ports(HlsArray *arr, uint32_t num_ports,
        bool dual_port)
{
    if (!arr || num_ports == 0 || num_ports > 4) return false;
    arr->num_ports = num_ports;
    arr->dual_port = dual_port;
    return true;
}

bool hls_array_can_access_parallel(HlsArray *arr, uint32_t num_accesses)
{
    if (!arr) return false;
    uint32_t max_ports = arr->num_banks * arr->num_ports;
    return num_accesses <= max_ports;
}

void hls_array_print_layout(HlsArray *arr, FILE *out)
{
    if (!arr || !out) return;
    fprintf(out, "Array \"%s\": %u dims, %u elements, %u bits/elem\n",
        arr->name, arr->num_dims, arr->total_elements, arr->elem_width);
    for (uint32_t i = 0; i < arr->num_dims; i++) {
        const char *pt =
            arr->dims[i].part_type == ARRAY_PART_BLOCK ? "block" :
            arr->dims[i].part_type == ARRAY_PART_CYCLIC ? "cyclic" :
            arr->dims[i].part_type == ARRAY_PART_COMPLETE ? "complete" :
            "none";
        fprintf(out, "  dim[%u]: size=%u part=%s factor=%u\n",
            i, arr->dims[i].size, pt,
            arr->dims[i].partition_factor);
    }
    fprintf(out, "  banks=%u ports=%u dual=%d conflicts=%u "
        "mem=%d\n",
        arr->num_banks, arr->num_ports, arr->dual_port,
        arr->bank_conflicts, arr->mem_type);
}

void hls_array_print_banking(HlsArray *arr, FILE *out)
{
    if (!arr || !out) return;
    HlsBankingReport report;
    hls_array_bank_analyze(arr, &report);
    fprintf(out, "Banks: %u, depth: %u, ports/bank: %u\n",
        report.num_banks, report.bank_depth, report.ports_per_bank);
    fprintf(out, "Conflicts: %s (%u)\n",
        report.has_conflicts ? "YES" : "NO",
        report.conflict_count);
    fprintf(out, "Recommendation: %s\n", report.recommendation);
    HlsMemoryTradeoff mt = hls_memory_tradeoff_analyze(
        arr->total_elements, arr->elem_width);
    fprintf(out, "Memory: BRAM=%u LUT=%u FF=%u -> %s\n",
        mt.bram_count, mt.lut_usage, mt.ff_usage,
        mt.recommended == MEM_BRAM ? "BRAM" : "LUTRAM");
}
