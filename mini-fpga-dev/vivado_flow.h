#ifndef VIVADO_FLOW_H
#define VIVADO_FLOW_H

#include <stdint.h>
#include <stddef.h>
#include "fpga_arch.h"

#define VIVADO_VERSION "2024.1"

/* Flow Stages */
#define FLOW_SYNTHESIS      0
#define FLOW_OPTIMIZATION   1
#define FLOW_PLACE          2
#define FLOW_PHYS_OPT       3
#define FLOW_ROUTE          4
#define FLOW_BITGEN         5
#define FLOW_NUM_STAGES     6

/* XDC Constraint Types */
#define XDC_CLOCK            0
#define XDC_INPUT_DELAY      1
#define XDC_OUTPUT_DELAY     2
#define XDC_CLOCK_GROUP      3
#define XDC_FALSE_PATH       4
#define XDC_MULTICYCLE       5
#define XDC_PIN_LOC          6
#define XDC_IO_STANDARD      7

/* Synthesis Strategies */
#define SYNTH_STRAT_DEFAULT       0
#define SYNTH_STRAT_AREA_OPT      1
#define SYNTH_STRAT_PERF_OPT      2
#define SYNTH_STRAT_PERF_RETIME   3
#define SYNTH_STRAT_FLOW_ALT      4
#define SYNTH_STRAT_AREA_EXPLORE  5

/* Implementation Stages */
#define IMPL_STAGE_INIT_DESIGN   0
#define IMPL_STAGE_OPT_DESIGN    1
#define IMPL_STAGE_POWER_OPT     2
#define IMPL_STAGE_PLACE_DESIGN  3
#define IMPL_STAGE_POST_PLACE_PS 4
#define IMPL_STAGE_PHYS_OPT      5
#define IMPL_STAGE_ROUTE_DESIGN  6
#define IMPL_STAGE_POST_ROUTE_PS 7
#define IMPL_STAGE_NUM           8

/* Timing/Power Report Types */
#define REPORT_TIMING_SUMMARY    0
#define REPORT_TIMING_PATH       1
#define REPORT_UTILIZATION       2
#define REPORT_POWER             3
#define REPORT_CLOCK_NETWORKS    4
#define REPORT_DESIGN_ANALYSIS   5

#define MAX_XDC_CONSTRAINTS     1024
#define MAX_PATH_ENDPOINTS      256

/* -------------------------------------------------------
 * XDC Constraint — clock, I/O, timing exception
 * ------------------------------------------------------- */
typedef struct {
    int     type;                   /* XDC_* constraint type */
    char    name[64];
    double  period_ns;              /* for clocks */
    double  rise_time_ns;
    double  fall_time_ns;
    double  duty_cycle;             /* 0.0 - 1.0 */
    int     is_virtual;             /* virtual clock flag */
    char    target_pin[64];         /* for I/O constraints */
    char    target_clk[64];
    char    iostandard[16];         /* LVCMOS33, LVDS, etc */
    double  delay_ns;               /* input/output delay */
    char    from_path[128];
    char    to_path[128];
    char    through[128];           /* through point for false path */
    int     is_datapath_only;       /* false path on -datapath_only */
    int     multicycle_value;
    int     multicycle_type;        /* setup/hold */
    char    comment[256];
} XdcConstraint;

void  xdc_init(XdcConstraint *xc);
int   xdc_create_clock(XdcConstraint *xc, const char *name,
                        double period_ns, const char *target,
                        double duty_cycle);
int   xdc_create_generated_clock(XdcConstraint *xc, const char *name,
                                  const char *source, double divide_by,
                                  const char *target);
int   xdc_set_false_path(XdcConstraint *xc, const char *from,
                          const char *to, const char *through);
int   xdc_set_io_location(XdcConstraint *xc, const char *pin,
                           const char *iostandard, const char *package_pin);
int   xdc_set_multicycle_path(XdcConstraint *xc, const char *from,
                               const char *to, int value, int is_setup);

/* -------------------------------------------------------
 * Synthesis Engine
 * ------------------------------------------------------- */
typedef struct {
    int     strategy;               /* SYNTH_STRAT_* */
    char    top_module[128];
    char    part_name[64];
    char    board_name[64];
    char    src_files[64][256];
    int     num_src_files;
    int     flatten_hierarchy;
    int     fsm_extraction;          /* 0=off, 1=one-hot, 2=sequential */
    int     resource_sharing;
    int     keep_hierarchy;
    int     num_elapsed_sec;
    double  estimated_fmax_mhz;
    int64_t estimated_luts;
    int64_t estimated_ffs;
    int64_t estimated_dsps;
    int64_t estimated_brams;
} SynthesisEngine;

void  synthesis_init(SynthesisEngine *syn, const char *part);
void  synthesis_add_source(SynthesisEngine *syn, const char *filepath);
int   synthesis_run(SynthesisEngine *syn);
void  synthesis_report(const SynthesisEngine *syn);

/* -------------------------------------------------------
 * Implementation Engine (Place & Route)
 * ------------------------------------------------------- */
typedef struct {
    int     current_stage;
    char    dcp_file[256];
    int     opt_directive;           /* Explore, AggressiveExplore, etc */
    int     place_directive;
    int     route_directive;
    int     phys_opt_directive;
    double  place_estimated_wirelength;
    double  route_estimated_delay_ns;
    int     num_critical_paths;
    int     total_routing_nodes;
    int     num_stages_completed;
    double  stage_progress[IMPL_STAGE_NUM];
} ImplementationEngine;

void  impl_init(ImplementationEngine *impl);
void  impl_set_directive(ImplementationEngine *impl, int stage, int directive);
int   impl_run_stage(ImplementationEngine *impl, int stage);
int   impl_run_all(ImplementationEngine *impl);
void  impl_generate_report(const ImplementationEngine *impl, int report_type);

/* -------------------------------------------------------
 * Vivado Flow Manager — orchestrates synthesis → bitstream
 * ------------------------------------------------------- */
typedef struct {
    SynthesisEngine     synth;
    ImplementationEngine impl;
    XdcConstraint       constraints[MAX_XDC_CONSTRAINTS];
    int                 num_constraints;
    char                project_name[128];
    char                output_dir[256];
    int                 current_flow_step;
    double              flow_progress_pct;
} VivadoFlow;

void  vivado_flow_init(VivadoFlow *flow, const char *project, const char *part);
int   vivado_flow_add_constraint(VivadoFlow *flow, const XdcConstraint *xc);
int   vivado_flow_run_synthesis(VivadoFlow *flow);
int   vivado_flow_run_implementation(VivadoFlow *flow);
int   vivado_flow_generate_bitstream(VivadoFlow *flow, const char *outfile);
int   vivado_flow_run_full(VivadoFlow *flow, const char *bitfile);
void  vivado_flow_report_summary(const VivadoFlow *flow);

/* -------------------------------------------------------
 * Report: timing / utilization
 * ------------------------------------------------------- */
typedef struct {
    char    path_name[128];
    double  slack_ns;
    double  total_delay_ns;
    double  logic_delay_ns;
    double  route_delay_ns;
    int     num_logic_levels;
    int     is_setup;               /* 1=setup check, 0=hold check */
    double  required_time_ns;
    double  arrival_time_ns;
    char    startpoint[128];
    char    endpoint[128];
} TimingPathReport;

typedef struct {
    double  slice_lut_pct;
    double  slice_reg_pct;
    double  dsp_pct;
    double  bram_pct;
    double  io_pct;
    double  bufg_pct;
    double  mmcm_pct;
    double  pcie_pct;
    int64_t slice_lut_used;
    int64_t slice_reg_used;
    int64_t dsp_used;
    int64_t bram_used;
    int64_t io_used;
    double  total_power_w;
    double  static_power_w;
    double  dynamic_power_w;
} UtilizationReport;

void  timing_path_init(TimingPathReport *rpt);
void  utilization_init(UtilizationReport *rpt);
void  timing_path_print(const TimingPathReport *rpt);
void  utilization_print(const UtilizationReport *rpt);

#endif /* VIVADO_FLOW_H */
