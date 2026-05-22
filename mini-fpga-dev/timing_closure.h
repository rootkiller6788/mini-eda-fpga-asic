#ifndef TIMING_CLOSURE_H
#define TIMING_CLOSURE_H

#include <stdint.h>
#include <stddef.h>
#include "fpga_arch.h"

#define TIMING_VERSION "1.0.0"

/* Timing Constants */
#define CLK_PERIOD_NS_DEFAULT   10.0
#define SETUP_SLACK_MIN_NS       0.0
#define HOLD_SLACK_MIN_NS        0.01

/* Timing Corner */
#define CORNER_SLOW   0       /* slow process, worst setup */
#define CORNER_FAST   1       /* fast process, worst hold */
#define CORNER_TYP    2       /* typical */

/* Path Types */
#define PATH_TYPE_DATA        0
#define PATH_TYPE_CLOCK       1
#define PATH_TYPE_CLOCK_EN    2
#define PATH_TYPE_ASYNC_SET   3
#define PATH_TYPE_ASYNC_RESET 4

/* Pipeline / Retiming */
#define MAX_PIPELINE_STAGES   32
#define MAX_REG_MOVE_DIST     10

/* -------------------------------------------------------
 * Timing Arc — delay through a single primitive
 * ------------------------------------------------------- */
typedef struct {
    char    arc_name[64];           /* e.g. "A->O" */
    double  delay_rise_ns;          /* rising transition delay */
    double  delay_fall_ns;          /* falling transition delay */
    double  slew_rise_ns;           /* rising output slew */
    double  slew_fall_ns;           /* falling output slew */
    int     is_combinational;       /* 1=combinational, 0=sequential */
    int     is_setup;               /* setup arc for FF */
    int     is_hold;                /* hold arc for FF */
    double  setup_limit_ns;
    double  hold_limit_ns;
} TimingArc;

void  timing_arc_init(TimingArc *arc, const char *name);
void  timing_arc_set_comb(TimingArc *arc, double rise_delay, double fall_delay);
void  timing_arc_set_seq(TimingArc *arc, double setup, double hold);

/* -------------------------------------------------------
 * Timing Node — a point in the timing graph
 * ------------------------------------------------------- */
typedef struct {
    char    node_name[64];
    double  arrival_time_ns;
    double  required_time_ns;
    double  slack_ns;
    int     is_endpoint;            /* sink FF / output port */
    int     is_startpoint;          /* source FF / input port */
    int     is_clock_node;
    double  clock_latency_ns;
    double  clock_skew_ns;
    double  clock_uncertainty_ns;
} TimingNode;

void  timing_node_init(TimingNode *node, const char *name);

/* -------------------------------------------------------
 * Timing Path — a complete start-to-end timing path
 * ------------------------------------------------------- */
typedef struct {
    TimingNode  startpoint;
    TimingNode  endpoint;
    double      total_delay_ns;
    double      logic_delay_ns;
    double      route_delay_ns;
    double      slack_ns;
    int         num_stages;             /* number of logic levels */
    double      clock_period_ns;
    int         is_multicycle;
    int         multicycle_value;
    int         corner;
    TimingArc   arcs[128];
    int         num_arcs;
} TimingPath;

void  timing_path_init(TimingPath *tp, double clk_period);
void  timing_path_add_arc(TimingPath *tp, const TimingArc *arc);
int   timing_path_check_setup(const TimingPath *tp);
int   timing_path_check_hold(const TimingPath *tp);
void  timing_path_report(const TimingPath *tp);

/* -------------------------------------------------------
 * Setup/Hold Analyzer for FPGA paths
 * ------------------------------------------------------- */
typedef struct {
    double  setup_required_ns;
    double  hold_required_ns;
    double  data_path_delay_ns;
    double  clock_path_delay_ns;
    double  clock_skew_ns;
    double  clock_jitter_ns;
    double  setup_slack_ns;
    double  hold_slack_ns;
} SetupHoldCheck;

void  sh_check_init(SetupHoldCheck *sh, double setup_req, double hold_req);
int   sh_check_setup(const SetupHoldCheck *sh, double *slack_out);
int   sh_check_hold(const SetupHoldCheck *sh, double *slack_out);

/* -------------------------------------------------------
 * Pipeline Inserter — inserts FF stages for timing
 * ------------------------------------------------------- */
typedef struct {
    uint8_t stage_count;
    uint8_t stage_position[MAX_PIPELINE_STAGES];  /* logic levels between stages */
    double  target_delay_ns;                       /* target delay per stage */
    int     num_stages_inserted;
    double  original_delay_ns;
    double  new_delay_ns;
    int     pipelined_netlist_size;
} PipelineInserter;

void  pipeline_init(PipelineInserter *pi, double target_delay);
int   pipeline_compute_stages(PipelineInserter *pi, double total_delay,
                               int logic_levels);
int   pipeline_insert_reg(PipelineInserter *pi, int at_logic_level);
int   pipeline_get_throughput(const PipelineInserter *pi, double *tp_mhz);

/* -------------------------------------------------------
 * Retiming Engine — moves registers across comb logic
 * ------------------------------------------------------- */
typedef struct {
    int     num_regs_forward;       /* regs moved forward */
    int     num_regs_backward;      /* regs moved backward */
    double  delay_before_move_ns;
    double  delay_after_move_ns;
    int     max_reg_moves;
    double  weight_forward;
    double  weight_backward;
} RetimingEngine;

void  retiming_init(RetimingEngine *re);
int   retiming_move_forward(RetimingEngine *re, double logic_delay,
                             double reg_setup);
int   retiming_move_backward(RetimingEngine *re, double logic_delay,
                              double reg_delay);
int   retiming_balance(RetimingEngine *re, double *delays, int num_stages,
                        double clk_period);

/* -------------------------------------------------------
 * Logic Duplication — duplicates for high-fanout nets
 * ------------------------------------------------------- */
typedef struct {
    int     fanout_threshold;           /* max fanout before duplication */
    int     num_original_cells;
    int     num_duplicated_cells;
    double  delay_before_ns;
    double  delay_after_ns;
    int     max_duplication_factor;
    int     duplicate_clusters[64];     /* which logic clusters are duplicated */
    int     num_clusters;
} LogicDuplicator;

void  logic_dup_init(LogicDuplicator *ld, int fanout_threshold);
int   logic_dup_analyze(LogicDuplicator *ld, int *fanout_counts, int num_nets);
int   logic_dup_apply(LogicDuplicator *ld, int net_id);
int   logic_dup_estimate_improvement(const LogicDuplicator *ld,
                                      double *improvement_pct);

/* -------------------------------------------------------
 * Clock Domain Crossing (CDC) timing helper
 * ------------------------------------------------------- */
typedef struct {
    double  src_clk_period_ns;
    double  dst_clk_period_ns;
    int     async_fifo_depth;
    double  data_rate_mbps;
    int     need_synchronizer;
    int     sync_stages;
} ClockCrossing;

void  cdc_init(ClockCrossing *cdc, double src_period, double dst_period);
int   cdc_check_safe(const ClockCrossing *cdc);
void  cdc_recommend_fifo_depth(ClockCrossing *cdc, double data_rate_mbps);

#endif /* TIMING_CLOSURE_H */
