#ifndef ACCELERATOR_VERILOG_H
#define ACCELERATOR_VERILOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "systolic_rtl.h"
#include "pe_microarch.h"
#include "buffer_hierarchy.h"
#include "dnn_isa.h"

#define AXI_DW     512
#define AXI_AW      48
#define AXI_IW       8
#define AXI_UW       4

typedef enum {
    BUS_AXI4_LITE,
    BUS_AXI4_FULL,
    BUS_AXI4_STREAM
} AxiBus;

typedef struct {
    uint32_t array_rows;
    uint32_t array_cols;
    uint32_t data_width;
    uint32_t buf_l1_depth;
    uint32_t buf_l2_depth;
    uint32_t buf_l3_depth;
    uint32_t freq_mhz;
    bool     pipe_en;
    bool     zero_skip_en;
    bool     dma_overlap_en;
    bool     multicast_en;
    AxiBus   ctrl_bus;
    AxiBus   data_bus;
} AccelCfg;

typedef struct {
    uint64_t total_ops;
    double   elapsed_s;
    double   tops;
    double   util_pct;
    double   peak_tops;
    double   eff_pct;
} TOPS;

typedef struct {
    double alpha;
    double cap_pf;
    double vdd;
    double freq_ghz;
    double p_dyn_mw;
    double p_sta_mw;
    double p_tot_mw;
    double energy_pj_op;
} Power;

typedef struct {
    const char *name;
    uint32_t batch;
    uint32_t in_dim;
    uint32_t out_dim;
    uint32_t seq_len;
    double   ref_tops;
    double   ref_latency_us;
} MLPerfBench;

typedef struct {
    bool      passed;
    uint64_t  ops;
    double    latency_us;
    double    tops;
    double    ref_tops;
    double    error_pct;
} MLPerfResult;

typedef struct {
    AccelCfg           cfg;
    SystolicArray      sa;
    BufferHierarchy    bh;
    AccelSeq           seq;
    FILE              *vlog;
    TOPS               perf;
    Power              power;
    MLPerfBench        bench;
    MLPerfResult       result;
    uint64_t           module_id;
} AccelTop;

void at_init(AccelTop *at, const AccelCfg *cfg);
void at_configure(AccelTop *at, const AccelCfg *cfg);
void at_reset(AccelTop *at);

void at_gen_verilog(AccelTop *at, const char *filename);
void at_gen_top_module(AccelTop *at);
void at_gen_axi4_lite_slave(AccelTop *at);
void at_gen_axi4_master(AccelTop *at);
void at_gen_systolic_array(AccelTop *at);
void at_gen_pe_module(AccelTop *at, uint32_t row, uint32_t col);
void at_gen_buffer_l1(AccelTop *at);
void at_gen_buffer_l2(AccelTop *at);
void at_gen_buffer_l3(AccelTop *at);
void at_gen_controller(AccelTop *at);
void at_gen_isa_decoder(AccelTop *at);
void at_gen_testbench(AccelTop *at, const char *filename);

void at_gen_verilog_header(FILE *f, const AccelCfg *cfg);
void at_gen_wire_declarations(FILE *f, const AccelCfg *cfg);
void at_gen_clock_reset(FILE *f);
void at_gen_axi_interface(FILE *f, AxiBus bus, const char *prefix, bool is_master);
void at_gen_pipeline_reg(FILE *f, uint32_t width, const char *name);
void at_gen_double_buffer(FILE *f, uint32_t depth, uint32_t width, const char *name);
void at_gen_mac_unit(FILE *f, uint32_t a_w, uint32_t b_w, uint32_t acc_w, const char *name);
void at_gen_fifo(FILE *f, uint32_t depth, uint32_t width, const char *name);

void at_run_mlperf(AccelTop *at);
void at_run_mlperf_bench(AccelTop *at, const char *name, uint32_t batch,
                         uint32_t in_f, uint32_t out_f, uint32_t seq);
MLPerfResult at_mlperf_eval(AccelTop *at);
void at_mlperf_report(const AccelTop *at);
bool at_mlperf_pass(const MLPerfResult *r, double tolerance);

TOPS at_calc_tops(uint64_t ops, double sec);
TOPS at_peak_tops(const AccelCfg *cfg);
TOPS at_achieved_tops(const AccelTop *at);
double at_efficiency(const AccelTop *at);

Power at_est_power(const AccelCfg *cfg);
Power at_est_power_detailed(double alpha, double cap, double vdd, double freq);
double at_energy_per_mac(const AccelTop *at);
double at_energy_efficiency_tops_w(const AccelTop *at);

void at_print_config(const AccelTop *at);
void at_print_perf(const AccelTop *at);
void at_print_power(const AccelTop *at);
void at_print_summary(const AccelTop *at);
void at_export_report(const AccelTop *at, const char *csv_path);

#endif
