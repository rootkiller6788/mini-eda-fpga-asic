#ifndef RISCV_VERILOG_H
#define RISCV_VERILOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum {
    VERILOG_2001 = 0,
    VERILOG_SV,
    VERILOG_2012
} VerilogStd;

typedef enum {
    CTRL_FSM       = 0,
    CTRL_HARDWIRED = 1,
    CTRL_MICROCODE = 2
} ControlStyle;

typedef enum {
    HAZARD_PASSTHROUGH = 0,
    HAZARD_STALL_FWD   = 1,
    HAZARD_FULL_FWD    = 2
} HazardStyle;

typedef struct {
    VerilogStd    standard;
    ControlStyle  control;
    HazardStyle   hazard;
    uint32_t      addr_width;
    uint32_t      data_width;
    bool          include_imem;
    bool          include_dmem;
    bool          include_debug;
    bool          include_uart;
    bool          include_csr;
    bool          include_fpu;
    bool          single_cycle;
    bool          multi_cycle;
    bool          pipeline;
    bool          out_of_order;
    char          module_name[64];
    char          top_module[64];
} VerilogConfig;

typedef struct {
    FILE  *fp;
    VerilogConfig cfg;
    uint32_t indent_level;
    uint32_t line_count;
} VerilogWriter;

void verilog_config_default(VerilogConfig *cfg);
void verilog_writer_init(VerilogWriter *vw, const char *path,
                         const VerilogConfig *cfg);
void verilog_writer_close(VerilogWriter *vw);

void verilog_emit_header(VerilogWriter *vw);
void verilog_emit_footer(VerilogWriter *vw);

void verilog_emit_module_decl(VerilogWriter *vw, const char *name,
                              const char **ports, uint32_t port_count);
void verilog_emit_ports(VerilogWriter *vw, const char **inputs,
                        uint32_t in_count, const char **outputs,
                        uint32_t out_count);

void verilog_emit_wire(VerilogWriter *vw, const char *name, uint32_t width);
void verilog_emit_reg(VerilogWriter *vw, const char *name, uint32_t width);
void verilog_emit_assign(VerilogWriter *vw, const char *lhs, const char *rhs);
void verilog_emit_always_comb_begin(VerilogWriter *vw);
void verilog_emit_always_ff_begin(VerilogWriter *vw, const char *edge,
                                  const char *clock);
void verilog_emit_always_end(VerilogWriter *vw);
void verilog_emit_if_begin(VerilogWriter *vw, const char *cond);
void verilog_emit_if_end(VerilogWriter *vw);
void verilog_emit_case_begin(VerilogWriter *vw, const char *sel);
void verilog_emit_case_item(VerilogWriter *vw, const char *value);
void verilog_emit_case_end(VerilogWriter *vw);

void verilog_emit_pc_logic(VerilogWriter *vw);
void verilog_emit_imem(VerilogWriter *vw, uint32_t depth);
void verilog_emit_register_file(VerilogWriter *vw);
void verilog_emit_alu(VerilogWriter *vw);
void verilog_emit_dmem(VerilogWriter *vw, uint32_t depth);
void verilog_emit_wb_mux(VerilogWriter *vw);
void verilog_emit_control_fsm(VerilogWriter *vw);
void verilog_emit_control_hardwired(VerilogWriter *vw);
void verilog_emit_hazard_unit(VerilogWriter *vw);
void verilog_emit_forwarding_logic(VerilogWriter *vw);
void verilog_emit_stall_logic(VerilogWriter *vw);
void verilog_emit_csr_block(VerilogWriter *vw);
void verilog_emit_timer(VerilogWriter *vw);

void verilog_emit_pipeline_reg(VerilogWriter *vw, const char *name,
                               uint32_t width);
void verilog_emit_datapath(VerilogWriter *vw);
void verilog_emit_top_module(VerilogWriter *vw, const VerilogConfig *cfg);

void verilog_comment(VerilogWriter *vw, const char *fmt, ...);
void verilog_stmt(VerilogWriter *vw, const char *fmt, ...);
void verilog_indent(VerilogWriter *vw);
void verilog_dedent(VerilogWriter *vw);

void verilog_generate_processor(const char *path, const VerilogConfig *cfg);
void verilog_generate_testbench(const char *path, const VerilogConfig *cfg);

#endif
