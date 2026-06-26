#ifndef FPGA_ARCH_H
#define FPGA_ARCH_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/* ================================================================
 * L1: Core Definitions — FPGA Architecture Primitives
 * References: Xilinx 7-Series CLB, Intel Agilex ALM, VPR arch model
 * L4: Shannon's Expansion Theorem, Elmore Delay Model, Rent's Rule
 * ================================================================ */

/* LUT size */
#define FPGA_MAX_LUT_SIZE   6
#define FPGA_LUT_MASK_BITS  (1 << FPGA_MAX_LUT_SIZE)

/* L1: k-input LUT — implements any k-variable Boolean function */
typedef struct {
    int      k;
    uint64_t mask;
    int      inputs[FPGA_MAX_LUT_SIZE];
    int      output;
    char     name[32];
} FpgaLut;

/* L1: Flip-Flop types */
typedef enum {
    FF_TYPE_DFF,
    FF_TYPE_DFFE,
    FF_TYPE_DFFSR,
    FF_TYPE_DFFAR
} FpgaFfType;

typedef struct {
    int         d_input;
    int         q_output;
    int         clk_net;
    int         en_net;
    int         sr_net;
    FpgaFfType  type;
    bool        init_val;
    char        name[24];
} FpgaFlipFlop;

/* L2: Carry chain for fast arithmetic */
typedef struct {
    int  cin;
    int  cout;
    int  di;
    int  s;
    bool used;
} FpgaCarryChain;

#define FPGA_SLICE_NUM_LUTS  4
#define FPGA_SLICE_NUM_FFS   4

typedef enum { SLICE_TYPE_L, SLICE_TYPE_M } FpgaSliceType;

/* L2: Slice — LUTs + FFs + carry logic */
typedef struct {
    int             slice_id;
    FpgaSliceType   type;
    FpgaLut         luts[FPGA_SLICE_NUM_LUTS];
    FpgaFlipFlop    ffs[FPGA_SLICE_NUM_FFS];
    FpgaCarryChain  carry;
    int             num_luts_used;
    int             num_ffs_used;
    int             x, y;
} FpgaSlice;

#define FPGA_CLB_NUM_SLICES  2

/* L2: CLB — Configurable Logic Block */
typedef struct {
    int         clb_id;
    FpgaSlice   slices[FPGA_CLB_NUM_SLICES];
    int         x, y;
    bool        used;
} FpgaClb;

/* L3: Routing switch patterns */
typedef enum {
    SW_PATTERN_DISJOINT,
    SW_PATTERN_WILTON,
    SW_PATTERN_UNIVERSAL
} FpgaSwitchPattern;

#define FPGA_MAX_SWITCHES  64

/* L3: Switch Box — configurable routing crossbar */
typedef struct {
    int  from_track;
    int  to_track;
    bool enabled;
} FpgaSwitchConfig;

typedef struct {
    FpgaSwitchConfig switches[FPGA_MAX_SWITCHES];
    int               num_switches;
    int               x, y;
    FpgaSwitchPattern pattern;
} FpgaSwitchBox;

#define FPGA_MAX_CONNECTIONS  32

/* L3: Connection Box — connects CLB pins to routing tracks */
typedef struct {
    int  pin_id;
    int  track_id;
    bool enabled;
} FpgaConnBoxEntry;

typedef struct {
    FpgaConnBoxEntry entries[FPGA_MAX_CONNECTIONS];
    int              num_entries;
    int              x, y;
} FpgaConnectionBox;

#define FPGA_MAX_TRACKS  128

typedef enum { CHAN_X, CHAN_Y } FpgaChannelDir;

/* L3: Routing channel */
typedef struct {
    FpgaChannelDir  dir;
    int             track_width;
    int             track_usage[FPGA_MAX_TRACKS];
    int             pos;
} FpgaRoutingChannel;

/* L3: FPGA Tile — the repeatable unit */
typedef struct {
    FpgaClb           clb;
    FpgaSwitchBox     sb;
    FpgaConnectionBox cb_in;
    FpgaConnectionBox cb_out;
    int               x, y;
} FpgaTile;

/* L2: Island-style FPGA Fabric
 * Reference: Betz, Rose, Marquardt "Architecture and CAD for FPGAs" */
typedef struct {
    int             grid_width;
    int             grid_height;
    int             channel_width;
    int             lut_size;
    FpgaTile**      tiles;
    FpgaRoutingChannel* h_channels;
    FpgaRoutingChannel* v_channels;
    int             num_nets;
    int             num_clbs;
    int             num_io_pads;
    double          tile_width_nm;
    double          tile_height_nm;
    int             tech_node_nm;
} FpgaFabric;

#define FPGA_NET_MAX_PINS  16

/* L1: Net — logical connection */
typedef struct {
    int     net_id;
    int     source_node;
    int     sink_nodes[FPGA_NET_MAX_PINS - 1];
    int     num_sinks;
    bool    is_routed;
    bool    is_clock;
    double  timing_criticality;
} FpgaNet;

/* API */
FpgaFabric* fpga_fabric_create(int width, int height, int chan_width, int lut_k);
void        fpga_fabric_destroy(FpgaFabric *f);
void        fpga_lut_init(FpgaLut *lut, int k);
void        fpga_lut_set_bit(FpgaLut *lut, int input_vector, bool value);
bool        fpga_lut_eval(FpgaLut *lut, const bool inputs[]);
bool        fpga_lut_eval_packed(FpgaLut *lut, int inputs);
void        fpga_clb_init(FpgaClb *clb, int x, int y);
void        fpga_ff_init(FpgaFlipFlop *ff, FpgaFfType type);
int         fpga_fabric_add_net(FpgaFabric *f);
void        fpga_net_set_source(FpgaFabric *f, int net_id, int clb_x, int clb_y, int slice, int lut);
void        fpga_net_add_sink(FpgaFabric *f, int net_id, int clb_x, int clb_y, int slice, int lut_input);
int         fpga_fabric_config_bits(const FpgaFabric *f);
int         fpga_fabric_num_nets(const FpgaFabric *f);
FpgaNet*    fpga_fabric_get_net(FpgaFabric *f, int net_id);
void        fpga_tile_init(FpgaTile *tile, int x, int y, int chan_width);
void        fpga_switch_box_generate(FpgaSwitchBox *sb, int w, FpgaSwitchPattern pat);
void        fpga_fabric_print_summary(const FpgaFabric *f);
double      fpga_rent_io_estimate(int num_clbs, double A, double p);
int         fpga_min_channel_width(int grid_w, int grid_h, double rent_p);
double      fpga_estimate_total_wirelength(int num_clbs, double rent_p);

#endif
