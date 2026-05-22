#include "clb_pack.h"
#include <stdio.h>
#include <string.h>

void packer_init(Packer *p, LutInstance *luts, int lut_count) {
    p->luts = luts;
    p->lut_count = lut_count;
    p->cluster_count = 0;
}

bool pack_greedy(Packer *p) {
    bool packed[MAX_CLUSTERS * 8] = {false};
    int packed_count = 0;

    while (packed_count < p->lut_count && p->cluster_count < MAX_CLUSTERS) {
        ClbCluster *c = &p->clusters[p->cluster_count];
        c->clb_id = p->cluster_count;
        c->lut_count = 0;
        c->ff_count = 0;
        c->input_pin_count = 0;

        for (int i = 0; i < p->lut_count && c->lut_count < 8; i++) {
            if (packed[i]) continue;
            if (c->lut_count == 0 || i < p->lut_count) {
                c->luts[c->lut_count++] = p->luts[i].id;
                if (p->luts[i].input_count > c->input_pin_count)
                    c->input_pin_count = p->luts[i].input_count;
                packed[i] = true;
                packed_count++;
            }
        }
        c->shared_inputs = (c->input_pin_count > 4);
        if (c->lut_count > 0) p->cluster_count++;
    }
    return packed_count == p->lut_count;
}

bool pack_seed(Packer *p, int seed_lut) {
    if (p->cluster_count >= MAX_CLUSTERS) return false;
    if (seed_lut < 0 || seed_lut >= p->lut_count) return false;

    ClbCluster *c = &p->clusters[p->cluster_count];
    c->clb_id = p->cluster_count;
    c->lut_count = 1;
    c->luts[0] = p->luts[seed_lut].id;
    c->input_pin_count = p->luts[seed_lut].input_count;
    p->cluster_count++;
    return true;
}

bool pack_legalize(Packer *p) {
    for (int i = 0; i < p->cluster_count; i++) {
        if (p->clusters[i].lut_count > 8) {
            return false;
        }
    }
    return true;
}

int pack_clb_count(Packer *p) { return p->cluster_count; }

int pack_total_utilization(Packer *p) {
    if (p->cluster_count == 0) return 0;
    int total_luts = 0;
    for (int i = 0; i < p->cluster_count; i++)
        total_luts += p->clusters[i].lut_count;
    return total_luts * 100 / (p->cluster_count * 8);
}

void pack_print(Packer *p) {
    printf("Packer: %d CLBs, %d LUTs\n", p->cluster_count, p->lut_count);
    for (int i = 0; i < p->cluster_count; i++) {
        printf("  CLB #%d: %d LUTs, %d FFs, %d input pins\n",
               p->clusters[i].clb_id, p->clusters[i].lut_count,
               p->clusters[i].ff_count, p->clusters[i].input_pin_count);
    }
    printf("  Utilization: %d%%\n", pack_total_utilization(p));
}
