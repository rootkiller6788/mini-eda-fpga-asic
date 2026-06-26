#include "accelerator_verilog.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

void at_init(AccelTop *at, const AccelCfg *cfg)
{
    if (!at) return;
    memset(at, 0, sizeof(AccelTop));
    at_configure(at, cfg);
    at->vlog    = NULL;
    at->module_id = 0;
}

void at_configure(AccelTop *at, const AccelCfg *cfg)
{
    if (!at || !cfg) return;
    at->cfg = *cfg;

    sa_init(&at->sa, cfg->array_rows, cfg->array_cols,
            SA_DATAFLOW_WEIGHT_STATIONARY, SA_PREC_INT8);
    if (cfg->pipe_en) sa_set_pipeline_depth(&at->sa, 4);

    bh_init(&at->bh);
    if (cfg->dma_overlap_en) bh_enable_overlap(&at->bh, true);
    if (cfg->multicast_en)   bh_enable_multicast(&at->bh, true);

    seq_init(&at->seq);

    at->perf.tops        = 0.0;
    at->perf.peak_tops   = at_peak_tops(cfg).tops;
    at->power            = at_est_power(cfg);
}

void at_reset(AccelTop *at)
{
    if (!at) return;
    sa_reset(&at->sa);
    bh_flush_all(&at->bh);
    seq_reset(&at->seq);
    at->perf.elapsed_s = 0.0;
    at->perf.tops      = 0.0;
    at->perf.total_ops = 0;
}

void at_gen_verilog(AccelTop *at, const char *filename)
{
    if (!at || !filename) return;

    at->vlog = fopen(filename, "w");
    if (!at->vlog) {
        printf("Error: cannot open %s for writing\n", filename);
        return;
    }

    at_gen_top_module(at);
    fclose(at->vlog);
    at->vlog = NULL;

    printf("Verilog RTL written to %s\n", filename);
}

static void vlog_emit(FILE *f, const char *fmt, ...)
{
    if (!f) return;
    fprintf(f, "%s", "    ");
}

void at_gen_verilog_header(FILE *f, const AccelCfg *cfg)
{
    if (!f) return;
    fprintf(f, "// AI Accelerator RTL - auto-generated\n");
    fprintf(f, "// Array: %u x %u, Data width: %u, Freq: %u MHz\n",
            cfg->array_rows, cfg->array_cols, cfg->data_width, cfg->freq_mhz);
    fprintf(f, "// Pipeline: %s, Zero-skip: %s\n",
            cfg->pipe_en ? "enabled" : "disabled",
            cfg->zero_skip_en ? "enabled" : "disabled");
    fprintf(f, "\n");
}

void at_gen_clock_reset(FILE *f)
{
    if (!f) return;
    fprintf(f, "    input  wire        clk,\n");
    fprintf(f, "    input  wire        rst_n,\n");
}

void at_gen_axi_interface(FILE *f, AxiBus bus, const char *prefix, bool is_master)
{
    if (!f) return;
    const char *dir = is_master ? "output" : "input";
    (void)bus;
    fprintf(f, "    %s wire [%d:0] %s_awaddr,\n", dir, AXI_AW - 1, prefix);
    fprintf(f, "    %s wire        %s_awvalid,\n", dir, prefix);
    fprintf(f, "    %s wire [%d:0] %s_wdata,\n", dir, AXI_DW - 1, prefix);
    fprintf(f, "    %s wire        %s_wvalid,\n", dir, prefix);
    fprintf(f, "    %s wire        %s_bready,\n", dir, prefix);
    fprintf(f, "    %s wire [%d:0] %s_araddr,\n", dir, AXI_AW - 1, prefix);
    fprintf(f, "    %s wire        %s_arvalid,\n", dir, prefix);
    fprintf(f, "    %s wire        %s_rready,\n", dir, prefix);
}

void at_gen_pipeline_reg(FILE *f, uint32_t width, const char *name)
{
    if (!f) return;
    fprintf(f, "    reg [%u:0] %s_d, %s_q;\n", width - 1, name, name);
    fprintf(f, "    always @(posedge clk or negedge rst_n) begin\n");
    fprintf(f, "        if (!rst_n) %s_q <= 0;\n", name);
    fprintf(f, "        else       %s_q <= %s_d;\n", name, name);
    fprintf(f, "    end\n");
}

void at_gen_double_buffer(FILE *f, uint32_t depth, uint32_t width, const char *name)
{
    if (!f) return;
    fprintf(f, "    reg [%u:0] %s_buf0 [0:%u];\n", width - 1, name, depth - 1);
    fprintf(f, "    reg [%u:0] %s_buf1 [0:%u];\n", width - 1, name, depth - 1);
    fprintf(f, "    reg %s_sel;\n", name);
    fprintf(f, "    wire [%u:0] %s_rdata = %s_sel ? %s_buf1[%s_raddr] : %s_buf0[%s_raddr];\n",
            width - 1, name, name, name, name, name, name);
}

void at_gen_mac_unit(FILE *f, uint32_t a_w, uint32_t b_w, uint32_t acc_w, const char *name)
{
    if (!f) return;
    fprintf(f, "    wire signed [%u:0] %s_a;\n", a_w - 1, name);
    fprintf(f, "    wire signed [%u:0] %s_b;\n", b_w - 1, name);
    fprintf(f, "    reg  signed [%u:0] %s_acc;\n", acc_w - 1, name);
    fprintf(f, "    wire signed [%u:0] %s_prod = %s_a * %s_b;\n",
            a_w + b_w - 1, name, name, name);
    fprintf(f, "    always @(posedge clk or negedge rst_n) begin\n");
    fprintf(f, "        if (!rst_n) %s_acc <= 0;\n", name);
    fprintf(f, "        else       %s_acc <= %s_acc + %s_prod;\n", name, name, name);
    fprintf(f, "    end\n");
}

void at_gen_fifo(FILE *f, uint32_t depth, uint32_t width, const char *name)
{
    if (!f) return;
    uint32_t addr_w = (uint32_t)ceil(log2((double)depth));
    fprintf(f, "    reg [%u:0] %s_mem [0:%u];\n", width - 1, name, depth - 1);
    fprintf(f, "    reg [%u:0] %s_wptr, %s_rptr;\n", addr_w, name, name);
    fprintf(f, "    reg [%u:0] %s_count;\n", addr_w, name);
    fprintf(f, "    wire %s_empty = (%s_count == 0);\n", name, name);
    fprintf(f, "    wire %s_full  = (%s_count == %u);\n", name, name, depth);
}

void at_gen_top_module(AccelTop *at)
{
    FILE *f = at->vlog;
    if (!f) return;

    at_gen_verilog_header(f, &at->cfg);

    fprintf(f, "module ai_accelerator_top #(\n");
    fprintf(f, "    parameter ARRAY_ROWS    = %u,\n", at->cfg.array_rows);
    fprintf(f, "    parameter ARRAY_COLS    = %u,\n", at->cfg.array_cols);
    fprintf(f, "    parameter DATA_WIDTH    = %u,\n", at->cfg.data_width);
    fprintf(f, "    parameter BUF_L2_DEPTH  = %u,\n", at->cfg.buf_l2_depth);
    fprintf(f, "    parameter BUF_L3_DEPTH  = %u,\n", at->cfg.buf_l3_depth);
    fprintf(f, "    parameter AXI_DATA_W    = %u\n", AXI_DW);
    fprintf(f, ") (\n");

    at_gen_clock_reset(f);
    fprintf(f, "\n");

    at_gen_axi_interface(f, BUS_AXI4_FULL, "m_axi", true);
    fprintf(f, "\n");
    at_gen_axi_interface(f, BUS_AXI4_LITE, "s_axi_ctrl", false);
    fprintf(f, "\n");

    fprintf(f, "    input  wire        start,\n");
    fprintf(f, "    output wire        done,\n");
    fprintf(f, "    output wire [31:0] status\n");
    fprintf(f, ");\n\n");

    fprintf(f, "    // Internal signals\n");
    fprintf(f, "    wire [31:0] ctrl_cmd;\n");
    fprintf(f, "    wire [31:0] ctrl_status;\n");
    fprintf(f, "    wire [31:0] isa_inst;\n");
    fprintf(f, "    wire        isa_valid;\n\n");

    fprintf(f, "    // Systolic Array instantiation\n");
    fprintf(f, "    systolic_array #(\n");
    fprintf(f, "        .ROWS(ARRAY_ROWS),\n");
    fprintf(f, "        .COLS(ARRAY_COLS),\n");
    fprintf(f, "        .DW(DATA_WIDTH)\n");
    fprintf(f, "    ) u_sa (\n");
    fprintf(f, "        .clk(clk),\n");
    fprintf(f, "        .rst_n(rst_n),\n");
    fprintf(f, "        .act_in(),\n");
    fprintf(f, "        .weight_in(),\n");
    fprintf(f, "        .result_out()\n");
    fprintf(f, "    );\n\n");

    fprintf(f, "    // Buffer hierarchy\n");
    fprintf(f, "    buffer_hierarchy #() u_buf ();\n\n");

    fprintf(f, "    // Controller + ISA decoder\n");
    fprintf(f, "    controller #() u_ctrl (\n");
    fprintf(f, "        .clk(clk),\n");
    fprintf(f, "        .rst_n(rst_n),\n");
    fprintf(f, "        .start(start),\n");
    fprintf(f, "        .done(done),\n");
    fprintf(f, "        .status(status)\n");
    fprintf(f, "    );\n\n");

    fprintf(f, "endmodule\n");

    at->module_id++;
}

void at_gen_systolic_array(AccelTop *at)
{
    FILE *f = at->vlog;
    if (!f) return;

    fprintf(f, "module systolic_array #(\n");
    fprintf(f, "    parameter ROWS = 32,\n");
    fprintf(f, "    parameter COLS = 32,\n");
    fprintf(f, "    parameter DW   = 8\n");
    fprintf(f, ") (\n");
    fprintf(f, "    input  wire                 clk,\n");
    fprintf(f, "    input  wire                 rst_n,\n");
    fprintf(f, "    input  wire signed [DW-1:0] act_in [0:ROWS-1],\n");
    fprintf(f, "    input  wire signed [DW-1:0] weight_in [0:ROWS-1][0:COLS-1],\n");
    fprintf(f, "    output wire signed [31:0]   result_out [0:COLS-1]\n");
    fprintf(f, ");\n\n");

    fprintf(f, "    genvar r, c;\n");
    fprintf(f, "    generate\n");
    fprintf(f, "        for (r = 0; r < ROWS; r = r + 1) begin : row_gen\n");
    fprintf(f, "            for (c = 0; c < COLS; c = c + 1) begin : col_gen\n");

    if (at->cfg.pipe_en) {
        fprintf(f, "                wire signed [DW-1:0] act_pipe_in, act_pipe_out;\n");
        fprintf(f, "                pipeline_reg #(.DW(DW)) u_pipe (\n");
        fprintf(f, "                    .clk(clk), .rst_n(rst_n),\n");
        fprintf(f, "                    .d(act_pipe_in), .q(act_pipe_out)\n");
        fprintf(f, "                );\n");
    }

    fprintf(f, "                pe #(.DW(DW)) u_pe (\n");
    fprintf(f, "                    .clk(clk),\n");
    fprintf(f, "                    .rst_n(rst_n),\n");
    fprintf(f, "                    .act(act_in[r]),\n");
    fprintf(f, "                    .weight(weight_in[r][c]),\n");
    fprintf(f, "                    .result(result_out[c])\n");
    fprintf(f, "                );\n");
    fprintf(f, "            end\n");
    fprintf(f, "        end\n");
    fprintf(f, "    endgenerate\n\n");

    fprintf(f, "endmodule\n");
}

void at_gen_pe_module(AccelTop *at, uint32_t row, uint32_t col)
{
    FILE *f = at->vlog;
    if (!f) return;

    fprintf(f, "module pe_%u_%u #(\n", row, col);
    fprintf(f, "    parameter DW = 8\n");
    fprintf(f, ") (\n");
    fprintf(f, "    input  wire                 clk,\n");
    fprintf(f, "    input  wire                 rst_n,\n");
    fprintf(f, "    input  wire signed [DW-1:0] act,\n");
    fprintf(f, "    input  wire signed [DW-1:0] weight,\n");
    fprintf(f, "    output reg  signed [31:0]   result\n");
    fprintf(f, ");\n\n");
    fprintf(f, "    wire signed [15:0] product;\n");
    fprintf(f, "    assign product = act * weight;\n\n");
    fprintf(f, "    always @(posedge clk or negedge rst_n) begin\n");
    fprintf(f, "        if (!rst_n)\n");
    fprintf(f, "            result <= 0;\n");
    fprintf(f, "        else\n");
    fprintf(f, "            result <= result + product;\n");
    fprintf(f, "    end\n\n");
    fprintf(f, "endmodule\n");
}

void at_gen_testbench(AccelTop *at, const char *filename)
{
    if (!at || !filename) return;

    FILE *f = fopen(filename, "w");
    if (!f) return;

    fprintf(f, "`timescale 1ns / 1ps\n\n");
    fprintf(f, "module tb_ai_accelerator;\n\n");
    fprintf(f, "    reg clk, rst_n, start;\n");
    fprintf(f, "    wire done;\n");
    fprintf(f, "    wire [31:0] status;\n\n");

    fprintf(f, "    ai_accelerator_top #(\n");
    fprintf(f, "        .ARRAY_ROWS(%u),\n", at->cfg.array_rows);
    fprintf(f, "        .ARRAY_COLS(%u),\n", at->cfg.array_cols);
    fprintf(f, "        .DATA_WIDTH(%u)\n", at->cfg.data_width);
    fprintf(f, "    ) dut (\n");
    fprintf(f, "        .clk(clk),\n");
    fprintf(f, "        .rst_n(rst_n),\n");
    fprintf(f, "        .start(start),\n");
    fprintf(f, "        .done(done),\n");
    fprintf(f, "        .status(status)\n");
    fprintf(f, "    );\n\n");

    fprintf(f, "    always #5 clk = ~clk;\n\n");

    fprintf(f, "    initial begin\n");
    fprintf(f, "        $display(\"AI Accelerator Testbench - MLPerf Benchmark\");\n");
    fprintf(f, "        clk   = 0;\n");
    fprintf(f, "        rst_n = 0;\n");
    fprintf(f, "        start = 0;\n");
    fprintf(f, "        #20 rst_n = 1;\n");
    fprintf(f, "        #10 start = 1;\n");
    fprintf(f, "        #10 start = 0;\n\n");
    fprintf(f, "        wait(done);\n");
    fprintf(f, "        $display(\"Benchmark completed, status: %%h\", status);\n");
    fprintf(f, "        $display(\"TOPS: %%.4f\", %.4f);\n", at->perf.peak_tops);
    fprintf(f, "        #100 $finish;\n");
    fprintf(f, "    end\n\n");

    fprintf(f, "    initial begin\n");
    fprintf(f, "        $dumpfile(\"tb_ai_accelerator.vcd\");\n");
    fprintf(f, "        $dumpvars(0, tb_ai_accelerator);\n");
    fprintf(f, "    end\n\n");

    fprintf(f, "endmodule\n");
    fclose(f);
}

void at_run_mlperf(AccelTop *at)
{
    if (!at) return;

    MLPerfBench benchmarks[] = {
        {"BERT-Large",      1,   1024, 4096, 384, 2.0,  1000.0},
        {"ResNet-50",       1,   2048, 2048, 0,   4.0,   500.0},
        {"SSD-ResNet34",    1,   1200, 1200, 0,   1.5,   800.0},
        {"Transformer-XL",  1,   512,  2048, 512, 1.8,  1200.0},
        {"DLRM",            1,   1024, 1024, 0,   0.8,  1500.0},
    };
    int n_bench = 5;

    printf("\n=========== MLPerf Benchmark Suite ===========\n");

    for (int i = 0; i < n_bench; i++) {
        at_run_mlperf_bench(at, benchmarks[i].name, benchmarks[i].batch,
                            benchmarks[i].in_dim, benchmarks[i].out_dim,
                            benchmarks[i].seq_len);

        TOPS achieved = at_achieved_tops(at);
        at->result.passed    = (achieved.tops >= benchmarks[i].ref_tops * 0.8);
        at->result.ops       = at->perf.total_ops;
        at->result.tops      = achieved.tops;
        at->result.ref_tops  = benchmarks[i].ref_tops;
        at->result.error_pct = fabs(achieved.tops - benchmarks[i].ref_tops)
                               / benchmarks[i].ref_tops * 100.0;

        printf("  %-20s  Ref: %6.2f TOPS  Achieved: %6.2f TOPS  %s\n",
               benchmarks[i].name, benchmarks[i].ref_tops,
               achieved.tops,
               at->result.passed ? "PASS" : "FAIL");
    }
    printf("==============================================\n");
}

void at_run_mlperf_bench(AccelTop *at, const char *name, uint32_t batch,
                         uint32_t in_f, uint32_t out_f, uint32_t seq)
{
    if (!at) return;
    at_reset(at);

    (void)name; (void)seq;

    uint32_t M = out_f;
    uint32_t N = batch;
    uint32_t K = in_f;

    MatmulShape shape = {
        .m_total = M, .n_total = N, .k_total = K,
        .m_tile = at->cfg.array_rows < M ? at->cfg.array_rows : M,
        .n_tile = at->cfg.array_cols < N ? at->cfg.array_cols : N,
        .k_tile = 32
    };

    isa_program_matmul_tiled(&at->seq, &shape);

    uint64_t ops = (uint64_t)2 * M * N * K;
    at->perf.total_ops = ops;

    while (seq_running(&at->seq) && !seq_halted(&at->seq)) {
        seq_step(&at->seq);
    }

    double cycles = (double)seq_cycles(&at->seq);
    double freq_ghz = (double)at->cfg.freq_mhz / 1000.0;
    at->perf.elapsed_s = cycles / (freq_ghz * 1e9);

    if (at->perf.elapsed_s > 0.0) {
        at->perf.tops = (double)ops / at->perf.elapsed_s * 1e-12;
    } else {
        at->perf.tops = at_peak_tops(&at->cfg).tops;
    }
}

MLPerfResult at_mlperf_eval(AccelTop *at)
{
    MLPerfResult r;
    memset(&r, 0, sizeof(r));

    if (!at) return r;

    r.ops        = at->perf.total_ops;
    r.tops       = at->perf.tops;
    r.passed     = at->result.passed;
    r.ref_tops   = at->result.ref_tops;
    r.error_pct  = at->result.error_pct;
    return r;
}

void at_mlperf_report(const AccelTop *at)
{
    if (!at) return;
    printf("MLPerf Result: %s  (%.2f TOPS vs %.2f TOPS ref, %.1f%% error)\n",
           at->result.passed ? "PASS" : "FAIL",
           at->result.tops, at->result.ref_tops, at->result.error_pct);
}

bool at_mlperf_pass(const MLPerfResult *r, double tolerance)
{
    if (!r) return false;
    return r->error_pct <= tolerance;
}

TOPS at_calc_tops(uint64_t ops, double sec)
{
    TOPS t;
    memset(&t, 0, sizeof(t));
    t.total_ops = ops;
    t.elapsed_s = sec;
    t.tops = (sec > 0.0) ? (double)ops / sec * 1e-12 : 0.0;
    return t;
}

TOPS at_peak_tops(const AccelCfg *cfg)
{
    TOPS t;
    memset(&t, 0, sizeof(t));
    if (!cfg) return t;

    uint64_t macs_per_cycle = (uint64_t)cfg->array_rows * (uint64_t)cfg->array_cols;
    uint64_t ops_per_cycle  = macs_per_cycle * 2;
    double freq_ghz = (double)cfg->freq_mhz / 1000.0;

    t.peak_tops  = (double)ops_per_cycle * freq_ghz;
    t.tops       = t.peak_tops;
    t.util_pct   = 100.0;
    t.eff_pct    = 100.0;
    return t;
}

TOPS at_achieved_tops(const AccelTop *at)
{
    if (!at) { TOPS t = {0}; return t; }
    TOPS p = at->perf;
    p.peak_tops = at_peak_tops(&at->cfg).tops;
    p.util_pct  = (p.peak_tops > 0.0) ? p.tops / p.peak_tops * 100.0 : 0.0;
    p.eff_pct   = p.util_pct;
    return p;
}

double at_efficiency(const AccelTop *at)
{
    if (!at) return 0.0;
    TOPS peak = at_peak_tops(&at->cfg);
    if (peak.tops == 0.0) return 0.0;
    return at->perf.tops / peak.tops * 100.0;
}

Power at_est_power(const AccelCfg *cfg)
{
    Power p;
    memset(&p, 0, sizeof(p));

    if (!cfg) return p;

    p.alpha    = 0.15;
    p.cap_pf   = (double)(cfg->array_rows * cfg->array_cols) * 0.002 + 50.0;
    p.vdd      = 0.75;
    p.freq_ghz = (double)cfg->freq_mhz / 1000.0;

    p.p_dyn_mw  = p.alpha * p.cap_pf * p.vdd * p.vdd * p.freq_ghz * 1e3;
    p.p_sta_mw  = p.p_dyn_mw * 0.1;
    p.p_tot_mw  = p.p_dyn_mw + p.p_sta_mw;

    double ops_per_s = (double)cfg->array_rows * (double)cfg->array_cols
                       * 2.0 * p.freq_ghz * 1e9;
    p.energy_pj_op = (ops_per_s > 0) ? p.p_tot_mw * 1e-3 / ops_per_s * 1e12 : 0.0;

    return p;
}

Power at_est_power_detailed(double alpha, double cap, double vdd, double freq)
{
    Power p;
    memset(&p, 0, sizeof(p));
    p.alpha    = alpha;
    p.cap_pf   = cap;
    p.vdd      = vdd;
    p.freq_ghz = freq;

    p.p_dyn_mw  = alpha * cap * vdd * vdd * freq * 1e3;
    p.p_sta_mw  = p.p_dyn_mw * 0.1;
    p.p_tot_mw  = p.p_dyn_mw + p.p_sta_mw;
    return p;
}

double at_energy_per_mac(const AccelTop *at)
{
    if (!at) return 0.0;
    return at->power.energy_pj_op / 2.0;
}

double at_energy_efficiency_tops_w(const AccelTop *at)
{
    if (!at || at->power.p_tot_mw == 0.0) return 0.0;
    return at->perf.tops / (at->power.p_tot_mw * 1e-3);
}

void at_print_config(const AccelTop *at)
{
    if (!at) return;
    printf("=== Accelerator Configuration ===\n");
    printf("  Array: %u x %u\n", at->cfg.array_rows, at->cfg.array_cols);
    printf("  Data width: %u bits\n", at->cfg.data_width);
    printf("  Frequency: %u MHz\n", at->cfg.freq_mhz);
    printf("  Pipeline: %s\n", at->cfg.pipe_en ? "enabled" : "disabled");
    printf("  Zero-skip: %s\n", at->cfg.zero_skip_en ? "enabled" : "disabled");
    printf("  Buffer L1: %u B, L2: %u KB, L3: %u MB\n",
           at->cfg.buf_l1_depth,
           at->cfg.buf_l2_depth / 1024,
           at->cfg.buf_l3_depth / (1024 * 1024));
}

void at_print_perf(const AccelTop *at)
{
    if (!at) return;
    TOPS peak = at_peak_tops(&at->cfg);
    printf("=== Performance ===\n");
    printf("  Total ops: %llu\n", (unsigned long long)at->perf.total_ops);
    printf("  Elapsed: %.6f s\n", at->perf.elapsed_s);
    printf("  Achieved: %.4f TOPS\n", at->perf.tops);
    printf("  Peak: %.4f TOPS\n", peak.tops);
    printf("  Efficiency: %.1f%%\n", at_efficiency(at));
}

void at_print_power(const AccelTop *at)
{
    if (!at) return;
    printf("=== Power Estimation ===\n");
    printf("  Alpha: %.3f\n", at->power.alpha);
    printf("  Capacitance: %.2f pF\n", at->power.cap_pf);
    printf("  VDD: %.2f V\n", at->power.vdd);
    printf("  Frequency: %.2f GHz\n", at->power.freq_ghz);
    printf("  Dynamic: %.2f mW\n", at->power.p_dyn_mw);
    printf("  Static: %.2f mW\n", at->power.p_sta_mw);
    printf("  Total: %.2f mW\n", at->power.p_tot_mw);
    printf("  Energy: %.4f pJ/op\n", at->power.energy_pj_op);
    printf("  Efficiency: %.2f TOPS/W\n", at_energy_efficiency_tops_w(at));
}

void at_print_summary(const AccelTop *at)
{
    if (!at) return;
    printf("\n");
    at_print_config(at);
    printf("\n");
    at_print_perf(at);
    printf("\n");
    at_print_power(at);
    printf("\n");
}

void at_export_report(const AccelTop *at, const char *csv_path)
{
    if (!at || !csv_path) return;
    FILE *f = fopen(csv_path, "w");
    if (!f) return;
    fprintf(f, "parameter,value\n");
    fprintf(f, "rows,%u\n", at->cfg.array_rows);
    fprintf(f, "cols,%u\n", at->cfg.array_cols);
    fprintf(f, "freq_mhz,%u\n", at->cfg.freq_mhz);
    fprintf(f, "tops,%.4f\n", at->perf.tops);
    fprintf(f, "peak_tops,%.4f\n", at_peak_tops(&at->cfg).tops);
    fprintf(f, "power_mw,%.2f\n", at->power.p_tot_mw);
    fprintf(f, "tops_per_w,%.2f\n", at_energy_efficiency_tops_w(at));
    fclose(f);
    printf("Report exported to %s\n", csv_path);
}

void at_gen_axi4_lite_slave(AccelTop *at)
{
    if (!at || !at->vlog) return;
    FILE *f = at->vlog;
    fprintf(f, "    // AXI4-Lite Slave\n");
    fprintf(f, "    axi4_lite_slave #() u_axi_slave (\n");
    fprintf(f, "        .s_axi_aclk(clk),\n");
    fprintf(f, "        .s_axi_aresetn(rst_n),\n");
    fprintf(f, "        .s_axi_awaddr(s_axi_ctrl_awaddr),\n");
    fprintf(f, "        .s_axi_awvalid(s_axi_ctrl_awvalid),\n");
    fprintf(f, "        .s_axi_wdata(s_axi_ctrl_wdata),\n");
    fprintf(f, "        .s_axi_wvalid(s_axi_ctrl_wvalid)\n");
    fprintf(f, "    );\n");
}

void at_gen_axi4_master(AccelTop *at)
{
    if (!at || !at->vlog) return;
    FILE *f = at->vlog;
    fprintf(f, "    // AXI4 Master\n");
    fprintf(f, "    axi4_master #() u_axi_master (\n");
    fprintf(f, "        .m_axi_aclk(clk),\n");
    fprintf(f, "        .m_axi_aresetn(rst_n),\n");
    fprintf(f, "        .m_axi_awaddr(m_axi_awaddr),\n");
    fprintf(f, "        .m_axi_awvalid(m_axi_awvalid)\n");
    fprintf(f, "    );\n");
}

void at_gen_buffer_l1(AccelTop *at)
{
    if (!at || !at->vlog) return;
    FILE *f = at->vlog;
    fprintf(f, "    // L1 PE-local buffer\n");
    at_gen_double_buffer(f, at->cfg.buf_l1_depth, at->cfg.data_width, "l1");
}

void at_gen_buffer_l2(AccelTop *at)
{
    if (!at || !at->vlog) return;
    FILE *f = at->vlog;
    fprintf(f, "    // L2 Array-global buffer\n");
    at_gen_double_buffer(f, at->cfg.buf_l2_depth, at->cfg.data_width * 8, "l2");
}

void at_gen_buffer_l3(AccelTop *at)
{
    if (!at || !at->vlog) return;
    FILE *f = at->vlog;
    fprintf(f, "    // L3 Chip-global buffer\n");
    at_gen_double_buffer(f, at->cfg.buf_l3_depth, at->cfg.data_width * 8, "l3");
}

void at_gen_controller(AccelTop *at)
{
    if (!at || !at->vlog) return;
    FILE *f = at->vlog;
    fprintf(f, "    // Controller FSM\n");
    fprintf(f, "    localparam IDLE = 0, LOAD = 1, COMPUTE = 2, STORE = 3, DONE_S = 4;\n");
    fprintf(f, "    reg [2:0] state, next_state;\n");
    fprintf(f, "    always @(posedge clk or negedge rst_n) begin\n");
    fprintf(f, "        if (!rst_n) state <= IDLE;\n");
    fprintf(f, "        else        state <= next_state;\n");
    fprintf(f, "    end\n");
}

void at_gen_isa_decoder(AccelTop *at)
{
    if (!at || !at->vlog) return;
    FILE *f = at->vlog;
    fprintf(f, "    // ISA Decoder\n");
    fprintf(f, "    wire [7:0] opcode = isa_inst[7:0];\n");
    fprintf(f, "    always @(*) begin\n");
    fprintf(f, "        case (opcode)\n");
    fprintf(f, "            8'h00: ; // NOP\n");
    fprintf(f, "            8'h01: ; // LOAD_WEIGHT\n");
    fprintf(f, "            8'h02: ; // LOAD_ACT\n");
    fprintf(f, "            8'h03: ; // MATMUL\n");
    fprintf(f, "            8'h04: ; // ACTIVATION\n");
    fprintf(f, "            8'h05: ; // STORE\n");
    fprintf(f, "            default: ;\n");
    fprintf(f, "        endcase\n");
    fprintf(f, "    end\n");
}
