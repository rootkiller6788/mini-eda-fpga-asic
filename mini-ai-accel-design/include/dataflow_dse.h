#ifndef DATAFLOW_DSE_H
#define DATAFLOW_DSE_H
#include <stdbool.h>

typedef enum { DF_WEIGHT_STATIONARY, DF_OUTPUT_STATIONARY, DF_INPUT_STATIONARY, DF_ROW_STATIONARY, DF_NO_LOCALITY } DataflowType;

typedef struct {
    DataflowType type;
    double energy_per_mac_pj;
    double area_per_mac_um2;
    double throughput_macs_per_cycle;
    int num_pe;
    int buffer_size_kb;
    double dram_bandwidth_gbps;
    double total_energy_mj;
    double total_area_mm2;
    double utilization;
} DataflowConfig;

void dse_init_config(DataflowConfig *cfg, DataflowType type, int num_pe);
double dse_eval_weight_stationary(DataflowConfig *cfg, int M, int N, int K);
double dse_eval_output_stationary(DataflowConfig *cfg, int M, int N, int K);
double dse_eval_input_stationary(DataflowConfig *cfg, int M, int N, int K);
double dse_eval_row_stationary(DataflowConfig *cfg, int M, int N, int K);
int dse_pareto_frontier(DataflowConfig *cfgs, int num_cfgs, int *pareto_ids, int max_pareto);
void dse_print_config(DataflowConfig *cfg);
const char *dse_type_name(DataflowType t);
#endif
