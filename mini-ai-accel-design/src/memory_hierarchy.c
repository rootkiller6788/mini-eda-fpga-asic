#include "memory_hierarchy.h"
#include <stdio.h>
#include <string.h>

void mem_hier_init(MemHierarchy *mh) {
    memset(mh, 0, sizeof(*mh));
}

int mem_hier_add_level(MemHierarchy *mh, MemLevel level, double size_kb, double energy_pj, double bw, int lat) {
    if (mh->level_count >= 8) return -1;
    MemBank *mb = &mh->levels[mh->level_count];
    mb->level = level; mb->size_kb = size_kb; mb->access_energy_pj = energy_pj;
    mb->bandwidth_gbps = bw; mb->latency_cycles = lat;
    const char *names[] = {"RF","L1","L2","GlobalBuf","DRAM"};
    snprintf(mb->name, sizeof(mb->name), "%s", names[(int)level]);
    return mh->level_count++;
}

double mem_hier_access_energy(MemHierarchy *mh, MemLevel level, int bytes) {
    for (int i = 0; i < mh->level_count; i++) {
        if (mh->levels[i].level == level)
            return (double)bytes * mh->levels[i].access_energy_pj / 1024.0;
    }
    return 0;
}

double mem_hier_dataflow(MemHierarchy *mh, int weights_bytes, int inputs_bytes, int outputs_bytes) {
    double e_w = mem_hier_access_energy(mh, MEM_DRAM, weights_bytes);
    double e_i = mem_hier_access_energy(mh, MEM_DRAM, inputs_bytes);
    double e_o = mem_hier_access_energy(mh, MEM_DRAM, outputs_bytes);
    double e_gbuf_w = mem_hier_access_energy(mh, MEM_GBUF, weights_bytes);
    double e_rf = mem_hier_access_energy(mh, MEM_RF, outputs_bytes);
    mh->total_access_energy_mj = (e_w + e_i + e_o + e_gbuf_w + e_rf) * 1e-9;
    mh->total_data_movement_bytes = weights_bytes + inputs_bytes + outputs_bytes;
    return mh->total_access_energy_mj;
}

void mem_hier_optimize(MemHierarchy *mh, int total_data_bytes) {
    /* Simple optimization: if data fits in L2, move it there */
    double l2_size = 0;
    for (int i = 0; i < mh->level_count; i++) if (mh->levels[i].level == MEM_L2) l2_size = mh->levels[i].size_kb;
    if (total_data_bytes < (int)(l2_size * 1024)) {
        for (int i = 0; i < mh->level_count; i++) if (mh->levels[i].level == MEM_DRAM) mh->levels[i].access_energy_pj *= 0.1;
    }
}

void mem_hier_print(MemHierarchy *mh) {
    printf("=== Memory Hierarchy ===\n");
    for (int i = 0; i < mh->level_count; i++) {
        MemBank *mb = &mh->levels[i];
        printf("  %-10s: %.0f KB, %.1f pJ/byte, %.1f GB/s, %d cycles\n", mb->name, mb->size_kb, mb->access_energy_pj, mb->bandwidth_gbps, mb->latency_cycles);
    }
    printf("  Total energy: %.6f mJ, Data movement: %d bytes\n", mh->total_access_energy_mj, mh->total_data_movement_bytes);
}
