#include "chiplet_partition.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

void partition_init(PartitionPlan *pp, int num_partitions) {
    memset(pp, 0, sizeof(*pp));
    pp->num_partitions = num_partitions;
    for (int i = 0; i < 4; i++) { pp->cost_per_area[i] = 1.0; pp->yield_per_area[i] = 0.99; }
}

int partition_add_block(PartitionPlan *pp, BlockType type, const char *name, double area, double power) {
    if (pp->block_count >= MAX_BLOCKS) return -1;
    DesignBlock *b = &pp->blocks[pp->block_count];
    b->id = pp->block_count; b->type = type; b->area = area; b->power = power; b->partition = 0;
    strncpy(b->name, name, 31); b->name[31] = '\0';
    return pp->block_count++;
}

/* Simple greedy partitioning: assign blocks to partitions balancing area */
int partition_func(PartitionPlan *pp) {
    if (pp->num_partitions <= 0) return -1;
    double *part_area = (double*)calloc((size_t)pp->num_partitions, sizeof(double));
    if (!part_area) return -1;
    for (int i = 0; i < pp->block_count; i++) {
        int best_p = 0; double min_area = part_area[0];
        for (int p = 1; p < pp->num_partitions; p++) if (part_area[p] < min_area) { min_area = part_area[p]; best_p = p; }
        pp->blocks[i].partition = best_p;
        part_area[best_p] += pp->blocks[i].area;
    }
    free(part_area);
    return 0;
}

double partition_evaluate(PartitionPlan *pp) {
    double cost = 0;
    double *part_area = (double*)calloc((size_t)pp->num_partitions, sizeof(double));
    if (!part_area) return 0;
    for (int i = 0; i < pp->block_count; i++) {
        int p = pp->blocks[i].partition;
        if (p < pp->num_partitions) part_area[p] += pp->blocks[i].area;
        cost += pp->blocks[i].area * pp->cost_per_area[(int)pp->blocks[i].type];
    }
    /* Yield: geometric mean of partition yields */
    double yield = 1.0; double defect_density = 0.01;
    for (int p = 0; p < pp->num_partitions; p++) yield *= exp(-part_area[p] * defect_density);
    pp->total_cost = cost; pp->total_yield = yield;
    free(part_area);
    return cost / (yield + 0.01);
}

void partition_pareto(PartitionPlan *pp, double *costs, double *yields, int max_points) {
    (void)costs; (void)yields; (void)max_points;
    partition_evaluate(pp);
}

void partition_print(PartitionPlan *pp) {
    partition_evaluate(pp);
    printf("=== Partition Plan ===\n");
    printf("  Partitions: %d, Blocks: %d\n", pp->num_partitions, pp->block_count);
    printf("  Total cost: %.2f, Yield: %.3f\n", pp->total_cost, pp->total_yield);
    for (int i = 0; i < pp->block_count; i++) {
        DesignBlock *b = &pp->blocks[i];
        const char *ts[] = {"Logic","Memory","IO","Accel"};
        printf("    %s [%s] area=%.2f pwr=%.2f -> chiplet %d\n", b->name, ts[(int)b->type], b->area, b->power, b->partition);
    }
}
