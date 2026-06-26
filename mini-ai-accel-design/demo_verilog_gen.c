#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "accelerator_verilog.h"
#include "systolic_rtl.h"
#include "pe_microarch.h"
#include "buffer_hierarchy.h"
#include "dnn_isa.h"

static void write_verilog_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "w");
    if (!f) { printf("  FAIL: cannot write %s\n", path); return; }
    fprintf(f, "%s", content);
    fclose(f);
    printf("  Generated: %s\n", path);
}

static void gen_config_v(const char *base, const AccelCfg *cfg)
{
    char path[512];
    snprintf(path, sizeof(path), "%s_pe.v", base);
    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "// Processing Element - Auto-generated\n");
    fprintf(f, "// %u x %u array, DW=%u\n\n", cfg->array_rows, cfg->array_cols, cfg->data_width);

    fprintf(f, "module pe #(\n");
    fprintf(f, "    parameter DW = %u\n", cfg->data_width);
    fprintf(f, ") (\n");
    fprintf(f, "    input  wire                 clk,\n");
    fprintf(f, "    input  wire                 rst_n,\n");

    if (cfg->pipe_en) {
        fprintf(f, "    input  wire                 pipe_en,\n");
        fprintf(f, "    input  wire                 pipe_in_valid,\n");
        fprintf(f, "    output wire                 pipe_out_valid,\n");
    }

    fprintf(f, "    input  wire signed [DW-1:0] act_in,\n");
    fprintf(f, "    output wire signed [DW-1:0] act_out,\n");
    fprintf(f, "    input  wire signed [DW-1:0] weight,\n");
    fprintf(f, "    input  wire                 weight_load,\n");
    fprintf(f, "    input  wire                 weight_swap,\n");

    if (cfg->zero_skip_en) {
        fprintf(f, "    input  wire                 zero_skip_en,\n");
        fprintf(f, "    output wire                 mult_skipped,\n");
    }

    fprintf(f, "    output reg  signed [31:0]   result,\n");
    fprintf(f, "    input  wire                 result_clear\n");
    fprintf(f, ");\n\n");

    fprintf(f, "    reg signed [DW-1:0] w_reg;\n");
    fprintf(f, "    reg signed [DW-1:0] w_reg_alt;\n");
    fprintf(f, "    reg                 w_valid;\n");
    fprintf(f, "    wire signed [15:0]  product = act_in * w_reg;\n");
    fprintf(f, "    wire                compute_en = act_in != 0 || !zero_skip_en;\n\n");

    fprintf(f, "    always @(posedge clk or negedge rst_n) begin\n");
    fprintf(f, "        if (!rst_n) begin\n");
    fprintf(f, "            w_reg  <= 0;\n");
    fprintf(f, "            result <= 0;\n");
    fprintf(f, "        end else begin\n");
    fprintf(f, "            if (weight_load) w_reg <= weight;\n");
    fprintf(f, "            if (weight_swap) w_reg <= w_reg_alt;\n");
    fprintf(f, "            if (result_clear) result <= 0;\n");
    fprintf(f, "            else if (compute_en) result <= result + product;\n");
    fprintf(f, "        end\n");
    fprintf(f, "    end\n\n");

    fprintf(f, "    assign act_out = act_in;\n");

    if (cfg->zero_skip_en) {
        fprintf(f, "    assign mult_skipped = (act_in == 0) && zero_skip_en;\n");
    }

    fprintf(f, "endmodule\n");
    fclose(f);
    printf("  Generated: %s\n", path);
}

static void gen_systolic_array_v(const char *base, const AccelCfg *cfg)
{
    char path[512];
    snprintf(path, sizeof(path), "%s_sa.v", base);
    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "// Systolic Array - %u x %u, Pipeline: %s\n",
            cfg->array_rows, cfg->array_cols, cfg->pipe_en ? "yes" : "no");
    fprintf(f, "// Dataflow: Weight-Stationary\n\n");

    fprintf(f, "module systolic_array #(\n");
    fprintf(f, "    parameter ROWS = %u,\n", cfg->array_rows);
    fprintf(f, "    parameter COLS = %u\n", cfg->array_cols);
    fprintf(f, ") (\n");
    fprintf(f, "    input  wire                 clk,\n");
    fprintf(f, "    input  wire                 rst_n,\n");
    fprintf(f, "    input  wire signed [7:0]    act_in [0:ROWS-1],\n");
    fprintf(f, "    input  wire signed [7:0]    weight_in [0:ROWS-1][0:COLS-1],\n");
    fprintf(f, "    input  wire                 weight_load,\n");
    fprintf(f, "    input  wire                 weight_swap [0:ROWS-1][0:COLS-1],\n");
    fprintf(f, "    output reg  signed [31:0]   partial_out [0:COLS-1],\n");
    fprintf(f, "    output wire signed [31:0]   result_out [0:COLS-1]\n");
    fprintf(f, ");\n\n");

    fprintf(f, "    genvar r, c;\n");
    fprintf(f, "    wire signed [7:0] act_pipe [0:ROWS-1][0:COLS];\n\n");

    fprintf(f, "    generate\n");
    fprintf(f, "        for (r = 0; r < ROWS; r = r + 1) begin : rows\n");
    fprintf(f, "            for (c = 0; c < COLS; c = c + 1) begin : cols\n\n");

    if (cfg->pipe_en) {
        fprintf(f, "                wire signed [7:0] pe_act_out;\n");
        fprintf(f, "                pipeline_reg #(.DW(8)) u_pipe (\n");
        fprintf(f, "                    .clk(clk), .rst_n(rst_n),\n");
        fprintf(f, "                    .d(c > 0 ? act_pipe[r][c] : act_in[r]),\n");
        fprintf(f, "                    .q(act_pipe[r][c+1])\n");
        fprintf(f, "                );\n\n");
    }

    fprintf(f, "                wire signed [31:0] pe_result;\n");
    fprintf(f, "                pe #(.DW(8)) u_pe (\n");
    fprintf(f, "                    .clk(clk), .rst_n(rst_n),\n");
    fprintf(f, "                    .act_in(%s),\n",
            cfg->pipe_en ? "act_pipe[r][c]" : "act_in[r]");
    fprintf(f, "                    .weight(weight_in[r][c]),\n");
    fprintf(f, "                    .weight_load(weight_load),\n");
    fprintf(f, "                    .weight_swap(weight_swap[r][c]),\n");
    fprintf(f, "                    .result(pe_result),\n");
    fprintf(f, "                    .result_clear(1'b0)\n");
    fprintf(f, "                );\n\n");
    fprintf(f, "            end\n");
    fprintf(f, "        end\n");

    fprintf(f, "        for (c = 0; c < COLS; c = c + 1) begin : reduce\n");
    fprintf(f, "            wire signed [31:0] col_partials [0:ROWS-1];\n");
    fprintf(f, "            assign col_partials[0] = rows[0].cols[c].u_pe.result;\n");
    for (uint32_t r = 1; r < cfg->array_rows && r < 32; r++) {
        fprintf(f, "            assign col_partials[%u] = col_partials[%u] + rows[%u].cols[c].u_pe.result;\n",
                r, r - 1, r);
    }
    fprintf(f, "            assign result_out[c] = col_partials[ROWS-1];\n");
    fprintf(f, "        end\n");

    fprintf(f, "    endgenerate\n\n");
    fprintf(f, "endmodule\n");
    fclose(f);
    printf("  Generated: %s\n", path);
}

static void gen_axi_top_v(const char *base, const AccelCfg *cfg)
{
    char path[512];
    snprintf(path, sizeof(path), "%s_top.v", base);
    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "// AI Accelerator Top Module\n");
    fprintf(f, "// %u x %u array, %u MHz, AXI4 interfaces\n", cfg->array_rows, cfg->array_cols, cfg->freq_mhz);
    fprintf(f, "// Pipeline: %s, Zero-skip: %s, DMA overlap: %s\n\n",
            cfg->pipe_en ? "yes" : "no",
            cfg->zero_skip_en ? "yes" : "no",
            cfg->dma_overlap_en ? "yes" : "no");

    fprintf(f, "module ai_accelerator_top #(\n");
    fprintf(f, "    parameter DATA_WIDTH   = %u,\n", cfg->data_width);
    fprintf(f, "    parameter ARRAY_ROWS   = %u,\n", cfg->array_rows);
    fprintf(f, "    parameter ARRAY_COLS   = %u,\n", cfg->array_cols);
    fprintf(f, "    parameter BUF_L2_DEPTH = %u,\n", cfg->buf_l2_depth);
    fprintf(f, "    parameter BUF_L3_DEPTH = %u\n", cfg->buf_l3_depth);
    fprintf(f, ") (\n");
    fprintf(f, "    input  wire         clk,\n");
    fprintf(f, "    input  wire         rst_n,\n\n");

    fprintf(f, "    // AXI4 Memory-mapped master interface\n");
    fprintf(f, "    output wire [47:0]  m_axi_awaddr,\n");
    fprintf(f, "    output wire         m_axi_awvalid,\n");
    fprintf(f, "    input  wire         m_axi_awready,\n");
    fprintf(f, "    output wire [511:0] m_axi_wdata,\n");
    fprintf(f, "    output wire         m_axi_wvalid,\n");
    fprintf(f, "    input  wire         m_axi_wready,\n");
    fprintf(f, "    input  wire [1:0]   m_axi_bresp,\n");
    fprintf(f, "    input  wire         m_axi_bvalid,\n");
    fprintf(f, "    output wire         m_axi_bready,\n\n");

    fprintf(f, "    // AXI4-Lite control interface\n");
    fprintf(f, "    input  wire [47:0]  s_axi_ctrl_awaddr,\n");
    fprintf(f, "    input  wire         s_axi_ctrl_awvalid,\n");
    fprintf(f, "    output wire         s_axi_ctrl_awready,\n");
    fprintf(f, "    input  wire [31:0]  s_axi_ctrl_wdata,\n");
    fprintf(f, "    input  wire         s_axi_ctrl_wvalid,\n");
    fprintf(f, "    output wire         s_axi_ctrl_wready,\n\n");

    fprintf(f, "    output wire         irq\n");
    fprintf(f, ");\n\n");

    fprintf(f, "    // Register map\n");
    fprintf(f, "    localparam REG_CTRL     = 8'h00;\n");
    fprintf(f, "    localparam REG_STATUS   = 8'h04;\n");
    fprintf(f, "    localparam REG_WADDR    = 8'h08;\n");
    fprintf(f, "    localparam REG_IADDR    = 8'h0C;\n");
    fprintf(f, "    localparam REG_OADDR    = 8'h10;\n");
    fprintf(f, "    localparam REG_M_ROWS   = 8'h14;\n");
    fprintf(f, "    localparam REG_N_COLS   = 8'h18;\n");
    fprintf(f, "    localparam REG_K_DIM    = 8'h1C;\n");
    fprintf(f, "    localparam REG_PERF_CTR = 8'h20;\n\n");

    fprintf(f, "    reg  [31:0] ctrl_reg;\n");
    fprintf(f, "    reg  [31:0] status_reg;\n");
    fprintf(f, "    wire [2:0]  state;\n\n");

    fprintf(f, "    // FSM\n");
    fprintf(f, "    localparam S_IDLE    = 3'd0;\n");
    fprintf(f, "    localparam S_LOAD_W  = 3'd1;\n");
    fprintf(f, "    localparam S_LOAD_A  = 3'd2;\n");
    fprintf(f, "    localparam S_COMPUTE = 3'd3;\n");
    fprintf(f, "    localparam S_STORE   = 3'd4;\n");
    fprintf(f, "    localparam S_DONE    = 3'd5;\n\n");

    fprintf(f, "    // Instantiate submodules\n");
    fprintf(f, "    wire [7:0] act_bus [0:ARRAY_ROWS-1];\n");
    fprintf(f, "    wire [7:0] wt_bus  [0:ARRAY_ROWS-1][0:ARRAY_COLS-1];\n");
    fprintf(f, "    wire [31:0] res_bus [0:ARRAY_COLS-1];\n\n");

    fprintf(f, "    systolic_array #(.ROWS(ARRAY_ROWS), .COLS(ARRAY_COLS)) u_sa (\n");
    fprintf(f, "        .clk(clk), .rst_n(rst_n),\n");
    fprintf(f, "        .act_in(act_bus), .weight_in(wt_bus),\n");
    fprintf(f, "        .weight_load(1'b0), .weight_swap('{default: 1'b0}),\n");
    fprintf(f, "        .result_out(res_bus)\n");
    fprintf(f, "    );\n\n");

    fprintf(f, "    assign irq = (state == S_DONE);\n");
    fprintf(f, "    assign status_reg = {29'd0, state};\n\n");

    fprintf(f, "endmodule\n");
    fclose(f);
    printf("  Generated: %s\n", path);
}

static void gen_testbench_v(const char *base, const AccelCfg *cfg)
{
    char path[512];
    snprintf(path, sizeof(path), "%s_tb.v", base);
    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "`timescale 1ns / 1ps\n\n");
    fprintf(f, "module tb_%s;\n\n", base);
    fprintf(f, "    reg clk, rst_n;\n");
    fprintf(f, "    reg [47:0] s_axi_ctrl_awaddr;\n");
    fprintf(f, "    reg        s_axi_ctrl_awvalid;\n");
    fprintf(f, "    reg [31:0] s_axi_ctrl_wdata;\n");
    fprintf(f, "    reg        s_axi_ctrl_wvalid;\n");
    fprintf(f, "    wire       m_axi_awvalid;\n");
    fprintf(f, "    wire       irq;\n\n");

    fprintf(f, "    ai_accelerator_top #(\n");
    fprintf(f, "        .DATA_WIDTH(%u), .ARRAY_ROWS(%u),\n", cfg->data_width, cfg->array_rows);
    fprintf(f, "        .ARRAY_COLS(%u), .BUF_L2_DEPTH(%u),\n", cfg->array_cols, cfg->buf_l2_depth);
    fprintf(f, "        .BUF_L3_DEPTH(%u)\n", cfg->buf_l3_depth);
    fprintf(f, "    ) dut (\n");
    fprintf(f, "        .clk(clk), .rst_n(rst_n),\n");
    fprintf(f, "        .m_axi_awaddr(), .m_axi_awvalid(m_axi_awvalid),\n");
    fprintf(f, "        .m_axi_awready(1'b1),\n");
    fprintf(f, "        .m_axi_wdata(), .m_axi_wvalid(), .m_axi_wready(1'b1),\n");
    fprintf(f, "        .m_axi_bresp(2'b00), .m_axi_bvalid(1'b1), .m_axi_bready(),\n");
    fprintf(f, "        .s_axi_ctrl_awaddr(s_axi_ctrl_awaddr),\n");
    fprintf(f, "        .s_axi_ctrl_awvalid(s_axi_ctrl_awvalid),\n");
    fprintf(f, "        .s_axi_ctrl_awready(),\n");
    fprintf(f, "        .s_axi_ctrl_wdata(s_axi_ctrl_wdata),\n");
    fprintf(f, "        .s_axi_ctrl_wvalid(s_axi_ctrl_wvalid),\n");
    fprintf(f, "        .s_axi_ctrl_wready(),\n");
    fprintf(f, "        .irq(irq)\n");
    fprintf(f, "    );\n\n");

    fprintf(f, "    integer cycle_cnt;\n");
    fprintf(f, "    real    freq_mhz, time_us;\n\n");

    fprintf(f, "    always #5 clk = ~clk;\n\n");

    fprintf(f, "    initial begin\n");
    fprintf(f, "        $display(\"===== AI Accelerator RTL Testbench =====\");\n");
    fprintf(f, "        $display(\"Config: %ux%u, %u MHz, Pipe: %s, ZeroSkip: %s\");\n",
            cfg->array_rows, cfg->array_cols, cfg->freq_mhz,
            cfg->pipe_en ? "ON" : "OFF",
            cfg->zero_skip_en ? "ON" : "OFF");
    fprintf(f, "\n");
    fprintf(f, "        clk   = 0;\n");
    fprintf(f, "        rst_n = 0;\n");
    fprintf(f, "        cycle_cnt = 0;\n");
    fprintf(f, "        freq_mhz = %u.0;\n", cfg->freq_mhz);
    fprintf(f, "        s_axi_ctrl_awaddr  = 48'd0;\n");
    fprintf(f, "        s_axi_ctrl_awvalid = 0;\n");
    fprintf(f, "        s_axi_ctrl_wdata   = 32'd0;\n");
    fprintf(f, "        s_axi_ctrl_wvalid  = 0;\n\n");

    fprintf(f, "        #50 rst_n = 1;\n");
    fprintf(f, "        #20;\n\n");

    fprintf(f, "        // Program start\n");
    fprintf(f, "        $display(\"Programming accelerator...\");\n");
    fprintf(f, "        @(posedge clk);\n");
    fprintf(f, "        s_axi_ctrl_awaddr   = 48'h00;\n");
    fprintf(f, "        s_axi_ctrl_awvalid  = 1;\n");
    fprintf(f, "        s_axi_ctrl_wdata    = 32'h0000_0001;\n");
    fprintf(f, "        s_axi_ctrl_wvalid   = 1;\n");
    fprintf(f, "        @(posedge clk);\n");
    fprintf(f, "        s_axi_ctrl_awvalid  = 0;\n");
    fprintf(f, "        s_axi_ctrl_wvalid   = 0;\n\n");

    fprintf(f, "        // Wait for completion\n");
    fprintf(f, "        wait(irq);\n");
    fprintf(f, "        time_us = $realtime / 1000.0;\n");
    fprintf(f, "        $display(\"Computation complete!\");\n");
    fprintf(f, "        $display(\"Time: %%0.2f us, Cycles: %%d\", time_us, cycle_cnt * 2);\n\n");

    fprintf(f, "        // TOPS calculation\n");
    fprintf(f, "        $display(\"===== Performance =====\");\n");
    fprintf(f, "        $display(\"Peak TOPS: %%0.2f\", %u.0 * %u.0 * 2.0 / 1000.0);\n",
            cfg->array_rows, cfg->array_cols);
    fprintf(f, "        $display(\"Energy: %%0.2f pJ/op\", 0.15 * %u.0 * 0.002 * 0.5625 * 1000.0);\n",
            cfg->array_rows * cfg->array_cols);
    fprintf(f, "\n");
    fprintf(f, "        #100;\n");
    fprintf(f, "        $display(\"===== Testbench Passed =====\");\n");
    fprintf(f, "        $finish;\n");
    fprintf(f, "    end\n\n");

    fprintf(f, "    always @(posedge clk) begin\n");
    fprintf(f, "        cycle_cnt <= cycle_cnt + 1;\n");
    fprintf(f, "    end\n\n");

    fprintf(f, "    // VCD dump\n");
    fprintf(f, "    initial begin\n");
    fprintf(f, "        $dumpfile(\"tb_%s.vcd\");\n", base);
    fprintf(f, "        $dumpvars(0, tb_%s);\n", base);
    fprintf(f, "    end\n\n");

    fprintf(f, "endmodule\n");
    fclose(f);
    printf("  Generated: %s\n", path);
}

int main(void)
{
    printf("====== AI Accelerator Verilog RTL Generator ======\n\n");

    AccelCfg configs[] = {
        {32,  32,  8,  256,       64*1024,   2*1024*1024, 500,  false, false, false, false, BUS_AXI4_LITE, BUS_AXI4_FULL},
        {32,  32,  8,  512,       64*1024,   2*1024*1024, 1000, true,  true,  true,  true,  BUS_AXI4_LITE, BUS_AXI4_FULL},
        {64,  64,  8,  512,      128*1024,   4*1024*1024, 1000, true,  true,  true,  true,  BUS_AXI4_LITE, BUS_AXI4_FULL},
        {128, 128, 8, 1024,      256*1024,   8*1024*1024, 1000, true,  true,  true,  true,  BUS_AXI4_LITE, BUS_AXI4_FULL},
        {256, 256, 8, 2048,      512*1024,  16*1024*1024, 1000, true,  true,  true,  true,  BUS_AXI4_LITE, BUS_AXI4_FULL},
    };
    int n_configs = 5;
    const char *names[] = {"accel_32x32", "accel_32x32_fast", "accel_64x64", "accel_128x128", "accel_256x256"};

    printf("Generating Verilog RTL for %d configurations:\n\n", n_configs);

    for (int i = 0; i < n_configs; i++) {
        AccelCfg *cfg = &configs[i];
        printf("--- %s (%u x %u, %u MHz) ---\n", names[i], cfg->array_rows, cfg->array_cols, cfg->freq_mhz);

        gen_config_v(names[i], cfg);
        gen_systolic_array_v(names[i], cfg);
        gen_axi_top_v(names[i], cfg);
        gen_testbench_v(names[i], cfg);

        TOPS peak = at_peak_tops(cfg);
        Power pwr = at_est_power(cfg);
        printf("  Peak TOPS: %.2f, Power: %.2f mW, Energy: %.4f pJ/op\n",
               peak.tops, pwr.p_tot_mw, pwr.energy_pj_op);
        printf("\n");
    }

    AccelTop accel;
    at_init(&accel, &configs[2]);
    at_gen_verilog(&accel, "accel_64x64_full.v");

    at_print_summary(&accel);
    at_export_report(&accel, "accel_report.csv");

    printf("\n====== RTL Generation Complete ======\n");
    printf("Generated files:\n");
    for (int i = 0; i < n_configs; i++) {
        printf("  %s_pe.v, %s_sa.v, %s_top.v, %s_tb.v\n",
               names[i], names[i], names[i], names[i]);
    }
    printf("  accel_64x64_full.v\n");
    printf("  accel_report.csv\n");

    return 0;
}
