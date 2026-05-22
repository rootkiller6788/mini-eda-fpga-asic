#ifndef CHIPLET_PARTITION_H
#define CHIPLET_PARTITION_H
#include <stdbool.h>

#define MAX_BLOCKS 32
#define MAX_PARTITIONS 8

typedef enum { BLOCK_LOGIC, BLOCK_MEMORY, BLOCK_IO, BLOCK_ACCEL } BlockType;

typedef struct { int id; BlockType type; char name[32]; double area; double power; int partition; } DesignBlock;

typedef struct {
    DesignBlock blocks[MAX_BLOCKS]; int block_count;
    int num_partitions;
    double cost_per_area[4];    /* area cost multiplier per technology */
    double yield_per_area[4];   /* yield model: exp(-area*defect_density) */
    double total_cost;
    double total_yield;
} PartitionPlan;

void partition_init(PartitionPlan *pp, int num_partitions);
int  partition_add_block(PartitionPlan *pp, BlockType type, const char *name, double area, double power);
int  partition_func(PartitionPlan *pp);        /* min-cut partitioning */
double partition_evaluate(PartitionPlan *pp);   /* cost function */
void partition_pareto(PartitionPlan *pp, double *costs, double *yields, int max_points);
void partition_print(PartitionPlan *pp);
#endif
