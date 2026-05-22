#ifndef MEMORY_HIERARCHY_H
#define MEMORY_HIERARCHY_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { MEM_RF, MEM_L1, MEM_L2, MEM_GBUF, MEM_DRAM } MemLevel;

typedef struct {
    MemLevel level; char name[16]; double size_kb; double access_energy_pj;
    double bandwidth_gbps; int latency_cycles;
} MemBank;

typedef struct {
    MemBank levels[8]; int level_count;
    double total_access_energy_mj;
    int total_data_movement_bytes;
} MemHierarchy;

void mem_hier_init(MemHierarchy *mh);
int  mem_hier_add_level(MemHierarchy *mh, MemLevel level, double size_kb, double energy_pj, double bw, int lat);
double mem_hier_access_energy(MemHierarchy *mh, MemLevel level, int bytes);
double mem_hier_dataflow(MemHierarchy *mh, int weights_bytes, int inputs_bytes, int outputs_bytes);
void mem_hier_optimize(MemHierarchy *mh, int total_data_bytes);
void mem_hier_print(MemHierarchy *mh);
#endif
