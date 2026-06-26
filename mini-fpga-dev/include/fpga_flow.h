#ifndef FPGA_FLOW_H
#define FPGA_FLOW_H

#include "fpga_arch.h"
#include "techmap_fpga.h"
#include "clb_pack.h"
#include "place_fpga.h"
#include "routing_fabric.h"
#include "timing_fpga.h"
#include "bitstream_gen.h"
#include "io_planning.h"
#include <stdbool.h>

/* ================================================================
 * L3/L6: Complete FPGA Implementation Flow
 * Reference: Vivado Design Suite, Quartus Prime, VPR flow
 * L6: End-to-end: RTL → Synthesis → Tech Map → Pack → Place → Route →
 *     Timing → Bitstream → Download
 * ================================================================ */

/* --- Flow Stage --- */
typedef enum {
    FLOW_INPUT,
    FLOW_SYNTHESIS,
    FLOW_TECHMAP,
    FLOW_PACKING,
    FLOW_PLACEMENT,
    FLOW_ROUTING,
    FLOW_TIMING,
    FLOW_BITSTREAM,
    FLOW_PROGRAMMING,
    FLOW_COMPLETE,
    FLOW_ERROR
} FpgaFlowStage;

/* --- Design Source Type --- */
typedef enum {
    SOURCE_VERILOG,
    SOURCE_VHDL,
    SOURCE_BOOL_NETLIST,  /* boolean network (our internal format) */
    SOURCE_EDIF,
    SOURCE_BLIF           /* Berkeley Logic Interchange Format */
} FpgaDesignSource;

/* --- Flow Configuration ---
 * Controls the entire FPGA implementation flow.
 */
typedef struct {
    /* Target Architecture */
    int    grid_width;
    int    grid_height;
    int    channel_width;
    int    lut_size;           /* K */
    int    tech_node_nm;

    /* Constraints */
    double target_freq_mhz;
    double clock_period_ns;

    /* Placement */
    FpgaPlaceParams place_params;

    /* Routing */
    double pres_fac;
    double hist_fac;
    int    route_max_iter;

    /* I/O */
    double die_width_um;
    double die_height_um;
    double pad_pitch_um;
    int    io_banks;

    /* Options */
    bool   enable_timing_driven;
    bool   enable_bitstream_compress;
    bool   enable_partial_reconfig;
    bool   verbose;

    char   design_name[64];
    char   part_name[32];
    char   output_dir[256];
} FpgaFlowConfig;

/* --- Flow State ---
 * Complete state of the implementation flow
 */
typedef struct {
    FpgaFlowStage       stage;
    FpgaFlowConfig      config;

    /* Intermediate representations */
    FpgaBoolNetwork     bool_net;
    FpgaLutMapping      lut_map;
    FpgaAtomNetlist     atom_nl;
    FpgaFabric*         fabric;
    FpgaPlacement       placement;
    FpgaRrGraph*        rr_graph;
    FpgaTimingGraph*    timing_graph;
    FpgaBitstream*      bitstream;
    FpgaIoRing          io_ring;

    /* Results */
    FpgaStaResult       sta_result;
    bool                success;
    double              achieved_freq_mhz;
    int                 total_luts;
    int                 total_ffs;
    int                 total_clbs;
    int                 routed_nets;
    double              wirelength;

    char                log[4096];
    int                 log_len;

    FpgaNet*            nets;
    int                 num_nets;
    int                 max_nets;
} FpgaFlow;

/* L1 API */
void        flow_init(FpgaFlow *flow, const FpgaFlowConfig *cfg);
void        flow_destroy(FpgaFlow *flow);
void        flow_log_msg(FpgaFlow *flow, const char *msg);

/* L6: Flow stage execution */
bool        flow_run_synthesis(FpgaFlow *flow);
bool        flow_run_techmap(FpgaFlow *flow);
bool        flow_run_packing(FpgaFlow *flow);
bool        flow_run_placement(FpgaFlow *flow);
bool        flow_run_routing(FpgaFlow *flow);
bool        flow_run_timing(FpgaFlow *flow);
bool        flow_run_bitstream(FpgaFlow *flow);
bool        flow_run_io_planning(FpgaFlow *flow);

/* Run entire flow end-to-end */
bool        flow_run_all(FpgaFlow *flow);

/* Load design from BLIF file */
bool        flow_load_blif(FpgaFlow *flow, const char *filename);

/* Load design from simple boolean netlist file */
bool        flow_load_bool(FpgaFlow *flow, const char *filename);

/* Generate reports */
void        flow_print_report(const FpgaFlow *flow);
void        flow_print_area_report(const FpgaFlow *flow);
void        flow_print_timing_report(const FpgaFlow *flow);
void        flow_print_power_estimate(const FpgaFlow *flow);

/* L7: Application — generate complete download package */
bool        flow_generate_download(FpgaFlow *flow, const char *filename);

/* L8: Partial Reconfiguration support */
bool        flow_partial_reconfig(FpgaFlow *flow, int region_x, int region_y,
                                  int region_w, int region_h);

/* L9: AI-assisted parameter tuning */
void        flow_auto_tune_place_params(FpgaFlow *flow);

/* Flow config defaults */
void        flow_config_defaults(FpgaFlowConfig *cfg);

#endif
