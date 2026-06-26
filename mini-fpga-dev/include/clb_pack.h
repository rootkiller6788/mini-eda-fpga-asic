#ifndef CLB_PACK_H
#define CLB_PACK_H

#include "fpga_arch.h"
#include "lut_synth.h"
#include <stdbool.h>

#define MAX_CLUSTERS 32

typedef struct {
    int      clb_id;
    int      luts[8];
    int      lut_count;
    int      ffs[8];
    int      ff_count;
    bool     shared_inputs;
    int      input_pins[32];
    int      input_pin_count;
} ClbCluster;

typedef struct {
    ClbCluster clusters[MAX_CLUSTERS];
    int        cluster_count;
    LutInstance *luts;
    int         lut_count;
} Packer;

void packer_init(Packer *p, LutInstance *luts, int lut_count);
bool pack_greedy(Packer *p);
bool pack_seed(Packer *p, int seed_lut);
bool pack_legalize(Packer *p);
int  pack_clb_count(Packer *p);
int  pack_total_utilization(Packer *p);
void pack_print(Packer *p);

#endif
