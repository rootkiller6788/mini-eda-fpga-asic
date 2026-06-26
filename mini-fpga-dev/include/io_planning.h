#ifndef IO_PLANNING_H
#define IO_PLANNING_H

#include "fpga_arch.h"
#include "place_fpga.h"
#include <stdbool.h>

/* ================================================================
 * L2/L6: FPGA I/O Planning & Pad Ring Design
 * References: Xilinx UG571 (SelectIO), Intel I/O Planning
 * L4: Transmission Line Theory — reflections, termination
 * L6: Complete I/O ring planning with SSO analysis
 * ================================================================ */

/* --- I/O Standard ---
 * Electrical signaling standard
 */
typedef enum {
    IO_LVCMOS33, IO_LVCMOS25, IO_LVCMOS18, IO_LVCMOS15,
    IO_LVCMOS12, IO_LVTTL,
    IO_SSTL18_I, IO_SSTL18_II, IO_SSTL15_I, IO_SSTL15_II,
    IO_HSTL_I, IO_HSTL_II, IO_HSTL_III,
    IO_LVDS, IO_LVDS_EXT, IO_BLVDS,
    IO_TMDS, IO_RSDS,
    IO_HSUL_12, IO_POD12,
    IO_DIFF_SSTL18_I, IO_DIFF_SSTL18_II,
    IO_CUSTOM
} FpgaIoStandard;

/* Default voltage levels for standards (V) */
typedef struct {
    double vcco;      /* output supply voltage */
    double vref;      /* reference voltage */
    double vih_min;   /* input high threshold */
    double vil_max;   /* input low threshold */
    double voh_min;   /* output high min */
    double vol_max;   /* output low max */
} FpgaIoElectrical;

/* --- I/O Pad ---
 * Physical I/O pad on the FPGA
 */
typedef enum {
    PAD_INPUT, PAD_OUTPUT, PAD_BIDIR, PAD_TRISTATE,
    PAD_CLK_IN, PAD_CLK_OUT,
    PAD_POWER, PAD_GROUND,
    PAD_NC  /* no connect */
} FpgaPadType;

typedef enum {
    BANK_HP,  /* High Performance (1.2V-1.8V) */
    BANK_HR,  /* High Range (1.2V-3.3V) */
    BANK_HD   /* High Density */
} FpgaBankType;

typedef struct {
    int            pad_id;
    FpgaPadType    type;
    FpgaIoStandard standard;
    int            bank;           /* I/O bank number */
    FpgaBankType   bank_type;
    int            location;       /* position on die perimeter */
    bool           is_clock_capable;
    bool           is_differential;
    int            differential_pair; /* paired pad ID, -1 if single-ended */
    /* Internal connection */
    int            connected_net;    /* net ID in fabric */
    int            connected_clb_x;
    int            connected_clb_y;
    /* Electrical */
    double         drive_strength_ma;
    double         slew_rate;        /* fast/slow */
    bool           pullup;
    bool           pulldown;
    bool           termination;      /* on-die termination */
    double         termination_ohms;
    /* Constraints */
    bool           is_fixed;         /* pad location locked */
    bool           is_vref;          /* used as voltage reference */
} FpgaIoPad;

/* --- I/O Bank ---
 * Group of I/O pads sharing Vcco and Vref
 */
#define FPGA_MAX_PADS_PER_BANK  52

typedef struct {
    int             bank_id;
    FpgaBankType    type;
    double          vcco;
    double          vref;
    FpgaIoStandard  standard;       /* all pads in bank use same standard */
    int             pads[FPGA_MAX_PADS_PER_BANK];
    int             num_pads;
    int             start_loc;      /* starting die perimeter location */
    int             end_loc;
    bool            is_powered;
} FpgaIoBank;

/* --- I/O Ring (Pad Ring) ---
 * Complete pad ring around the die perimeter.
 * Modern FPGAs have pads on all 4 sides.
 */
typedef enum {
    RING_TOP, RING_BOTTOM, RING_LEFT, RING_RIGHT
} FpgaRingSide;

#define FPGA_MAX_PADS  512
#define FPGA_MAX_BANKS 16

typedef struct {
    FpgaIoPad       pads[FPGA_MAX_PADS];
    int             num_pads;
    FpgaIoBank      banks[FPGA_MAX_BANKS];
    int             num_banks;
    int             pads_per_side[4];   /* T,B,L,R */
    int             total_pads;
    int             total_power_pads;
    int             total_ground_pads;
    int             total_signal_pads;
    double          die_width_um;
    double          die_height_um;
    double          pad_pitch_um;       /* center-to-center spacing */
} FpgaIoRing;

/* --- SSO Analysis (Simultaneous Switching Output) ---
 * When many outputs switch simultaneously, ground bounce occurs.
 * SSO limit = max number of simultaneously switching outputs
 *   before induced noise exceeds Vih/Vil thresholds.
 * Reference: S.H. Hall et al., "High-Speed Digital System Design" */
typedef struct {
    int     max_sso_per_bank;      /* maximum allowed SSO */
    int     actual_sso;            /* design's worst SSO */
    double  noise_margin_mv;       /* remaining noise margin */
    double  ground_bounce_est_mv;  /* estimated ground bounce */
    bool    is_violated;
} FpgaSsoAnalysis;

/* --- Pinout File Format ---
 */
typedef enum {
    PINOUT_CSV,
    PINOUT_UCF,     /* Xilinx User Constraints File */
    PINOUT_XDC,     /* Xilinx Design Constraints */
    PINOUT_SDC,     /* Synopsys Design Constraints */
    PINOUT_QSF      /* Intel Quartus Settings File */
} FpgaPinoutFormat;

/* L1 API: I/O Ring */
void         io_ring_init(FpgaIoRing *ring, double die_w, double die_h,
                          double pitch);
int          io_ring_add_pad(FpgaIoRing *ring, FpgaPadType type,
                             FpgaIoStandard std, int location);
int          io_ring_add_bank(FpgaIoRing *ring, FpgaBankType type,
                              double vcco, int start_loc, int end_loc);
void         io_ring_assign_bank(FpgaIoRing *ring, int pad_id, int bank_id);
void         io_ring_destroy(FpgaIoRing *ring);

/* L4: IBIS Model-based I/O characterization */
FpgaIoElectrical io_get_electrical(FpgaIoStandard std);
double           io_propagation_delay(FpgaIoStandard std, double load_cap_pf);
double           io_reflection_coefficient(double z0, double z_load);

/* L6: Full I/O Planning Flow */
int          io_plan_full_ring(FpgaIoRing *ring, const FpgaPlacement *p,
                               const FpgaNet* nets, int num_nets,
                               int num_io_pads_needed);

/* Pin assignment — assign logical I/Os to physical pad locations */
int          io_assign_pins(FpgaIoRing *ring, const FpgaPlacement *p,
                            const FpgaNet* nets, int num_nets);

/* SSO Analysis */
void         io_sso_analyze(const FpgaIoRing *ring, FpgaSsoAnalysis *sso);
bool         io_sso_check(const FpgaIoRing *ring, int switching_count);

/* Power pad estimation — Rent's Rule based
 * N_power_pads = K * (N_signal_pads) ^ p
 * where p ≈ 0.5 (Rent exponent for I/O) */
int          io_estimate_power_pads(int num_signal_pads, double rent_exponent);

/* Pinout file generation */
int          io_write_pinout(const FpgaIoRing *ring, const char *filename,
                             FpgaPinoutFormat fmt);

/* Bank voltage compatibility check */
bool         io_bank_compatible(const FpgaIoBank *bank, FpgaIoStandard std);

/* Differential pair validation */
bool         io_validate_diff_pairs(const FpgaIoRing *ring);

/* Print I/O ring summary */
void         io_ring_print_summary(const FpgaIoRing *ring);

#endif
