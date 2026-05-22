#include "dataflow_dse.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

const char *dse_type_name(DataflowType t) {
    switch (t) { case DF_WEIGHT_STATIONARY: return "Weight Stationary"; case DF_OUTPUT_STATIONARY: return "Output Stationary"; case DF_INPUT_STATIONARY: return "Input Stationary"; case DF_ROW_STATIONARY: return "Row Stationary"; case DF_NO_LOCALITY: return "No Locality"; default: return "?"; }
}

void dse_init_config(DataflowConfig *cfg, DataflowType type, int num_pe) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->type = type; cfg->num_pe = num_pe;
    switch (type) {
        case DF_WEIGHT_STATIONARY: cfg->energy_per_mac_pj = 0.5; cfg->area_per_mac_um2 = 800; break;
        case DF_OUTPUT_STATIONARY: cfg->energy_per_mac_pj = 0.8; cfg->area_per_mac_um2 = 600; break;
        case DF_INPUT_STATIONARY: cfg->energy_per_mac_pj = 0.7; cfg->area_per_mac_um2 = 650; break;
        case DF_ROW_STATIONARY: cfg->energy_per_mac_pj = 0.4; cfg->area_per_mac_um2 = 900; break;
        case DF_NO_LOCALITY: cfg->energy_per_mac_pj = 2.0; cfg->area_per_mac_um2 = 400; break;
    }
    cfg->buffer_size_kb = num_pe / 4;
    cfg->throughput_macs_per_cycle = (double)num_pe;
}

static double compute_energy(DataflowConfig *cfg, int M, int N, int K) {
    double macs = (double)M * N * K;
    double dram_accesses = 0;
    switch (cfg->type) {
        case DF_WEIGHT_STATIONARY: dram_accesses = (double)(M * K + M * N); break;
        case DF_OUTPUT_STATIONARY: dram_accesses = (double)(M * K + K * N); break;
        case DF_INPUT_STATIONARY: dram_accesses = (double)(M * N + K * N); break;
        case DF_ROW_STATIONARY: dram_accesses = (double)(M * K + K * N + M * N) * 0.5; break;
        default: dram_accesses = (double)(M * K + K * N + M * N); break;
    }
    return macs * cfg->energy_per_mac_pj + dram_accesses * 20.0; /* 20pJ per DRAM access */
}

double dse_eval_weight_stationary(DataflowConfig *cfg, int M, int N, int K) { cfg->type = DF_WEIGHT_STATIONARY; return compute_energy(cfg, M, N, K); }
double dse_eval_output_stationary(DataflowConfig *cfg, int M, int N, int K) { cfg->type = DF_OUTPUT_STATIONARY; return compute_energy(cfg, M, N, K); }
double dse_eval_input_stationary(DataflowConfig *cfg, int M, int N, int K) { cfg->type = DF_INPUT_STATIONARY; return compute_energy(cfg, M, N, K); }
double dse_eval_row_stationary(DataflowConfig *cfg, int M, int N, int K) { cfg->type = DF_ROW_STATIONARY; return compute_energy(cfg, M, N, K); }

int dse_pareto_frontier(DataflowConfig *cfgs, int num_cfgs, int *pareto_ids, int max_pareto) {
    int np = 0;
    for (int i = 0; i < num_cfgs && np < max_pareto; i++) {
        bool dominated = false;
        for (int j = 0; j < num_cfgs; j++) {
            if (i == j) continue;
            if (cfgs[j].total_energy_mj <= cfgs[i].total_energy_mj && cfgs[j].total_area_mm2 <= cfgs[i].total_area_mm2)
                if (cfgs[j].total_energy_mj < cfgs[i].total_energy_mj || cfgs[j].total_area_mm2 < cfgs[i].total_area_mm2)
                    { dominated = true; break; }
        }
        if (!dominated) pareto_ids[np++] = i;
    }
    return np;
}

void dse_print_config(DataflowConfig *cfg) {
    printf("  %s: %.2f pJ/MAC, %.0f um2/MAC, %d PEs\n", dse_type_name(cfg->type), cfg->energy_per_mac_pj, cfg->area_per_mac_um2, cfg->num_pe);
}
