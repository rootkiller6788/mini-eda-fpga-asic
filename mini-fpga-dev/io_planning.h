#ifndef IO_PLANNING_H
#define IO_PLANNING_H

#include <stdint.h>
#include <stddef.h>

#define IO_PLANNING_VERSION "1.0.0"

/* I/O Standard Types */
#define IOSTD_LVCMOS33    0
#define IOSTD_LVCMOS25    1
#define IOSTD_LVCMOS18    2
#define IOSTD_LVCMOS15    3
#define IOSTD_LVCMOS12    4
#define IOSTD_LVDS        5
#define IOSTD_LVDS_EXT    6    /* LVDS with external termination */
#define IOSTD_SSTL15      7
#define IOSTD_SSTL18_I    8
#define IOSTD_SSTL18_II   9
#define IOSTD_HSTL_I      10
#define IOSTD_HSTL_II     11
#define IOSTD_HSUL_12     12
#define IOSTD_POD12       13
#define IOSTD_DIFF_SSTL15 14
#define IOSTD_TMDS_33     15
#define IOSTD_NUM         16

/* I/O Direction */
#define IO_DIR_INPUT      0
#define IO_DIR_OUTPUT     1
#define IO_DIR_BIDIR      2

/* Drive Strength (mA) */
#define DRIVE_2MA         0
#define DRIVE_4MA         1
#define DRIVE_6MA         2
#define DRIVE_8MA         3
#define DRIVE_12MA        4
#define DRIVE_16MA        5
#define DRIVE_24MA        6

/* Slew Rate */
#define SLEW_SLOW         0
#define SLEW_FAST         1

/* Termination Types */
#define TERM_NONE         0
#define TERM_INTERNAL_50  1    /* DCI internal 50-ohm */
#define TERM_INTERNAL_60  2
#define TERM_INTERNAL_75  3
#define TERM_EXTERNAL     4
#define TERM_SPLIT_50     5    /* split termination (Thevenin) */

/* Bank Limits */
#define MAX_IO_BANKS          16
#define MAX_PINS_PER_BANK     52
#define MAX_TOTAL_PINS        832

/* Clock Capable Pin Mask */
#define CLK_CAP_MRCC     (1 << 0)  /* Multi-Region Clock Capable */
#define CLK_CAP_SRCC     (1 << 1)  /* Single-Region Clock Capable */
#define CLK_CAP_GC       (1 << 2)  /* Global Clock Capable */
#define CLK_CAP_HDGC     (1 << 3)  /* Horizontal Dist GC */

/* Characteristic Impedance (ohms) */
#define Z0_50_OHMS        50.0
#define Z0_60_OHMS        60.0
#define Z0_75_OHMS        75.0
#define Z0_100_OHMS      100.0

#define SSN_MAX_VICTIMS    32

/* -------------------------------------------------------
 * I/O Pin — a single physical I/O pad
 * ------------------------------------------------------- */
typedef struct {
    char    pin_name[16];           /* e.g. "Y11", "AB22" */
    char    signal_name[64];
    int     bank_number;
    int     direction;              /* IO_DIR_* */
    int     io_standard;            /* IOSTD_* */
    int     drive_strength;         /* DRIVE_* */
    int     slew_rate;              /* SLEW_* */
    double  bank_vcc_aux_v;         /* auxiliary supply voltage */
    double  bank_vcco_v;            /* output buffer supply voltage */
    double  vref_v;                 /* reference voltage if needed */
    int     has_pullup;
    int     has_pulldown;
    int     has_keeper;
    int     differential_pair;     /* pin index of pair mate, -1 if none */
    int     clk_capable_mask;      /* CLK_CAP_* flags */
    int     is_assigned;
    int     is_fixed;              /* locked by user constraints */
    int     is_dedicated_in;       /* dedicated input only (no OBUF) */
} IoPin;

void  io_pin_init(IoPin *pin, const char *name, int bank);
int   io_pin_assign_signal(IoPin *pin, const char *signal, int dir,
                            int iostd);
void  io_pin_set_drive(IoPin *pin, int drive);
void  io_pin_set_term(IoPin *pin, int termination);

/* -------------------------------------------------------
 * I/O Bank — a group of I/O pins sharing Vcco
 * ------------------------------------------------------- */
typedef struct {
    int     bank_number;
    double  vcco_target_v;         /* target Vcco */
    int     io_standard;           /* dominant I/O standard for bank */
    IoPin   pins[MAX_PINS_PER_BANK];
    int     num_pins;
    int     num_used_pins;
    int     num_clock_pins;
    double  total_ssn_current_ma;  /* total simultaneous switching current */
    int     is_dedicated_cfg;      /* configuration bank flag */
} IoBank;

void  io_bank_init(IoBank *bank, int bank_num, double vcco);
int   io_bank_add_pin(IoBank *bank, const IoPin *pin);
int   io_bank_assign(IoBank *bank, int pin_idx, const char *signal,
                      int dir, int iostd);
void  io_bank_recalc_ssn(IoBank *bank);

/* -------------------------------------------------------
 * Simultaneous Switching Noise (SSN) Analyzer
 * ------------------------------------------------------- */
typedef struct {
    int     aggressor_count;        /* number of simultaneously switching */
    double  aggressor_slew_ns;      /* slew rate of aggressors */
    double  victim_noise_mv;        /* induced noise on victim pin */
    double  noise_margin_mv;        /* noise margin before glitch */
    int     victim_pin_ids[SSN_MAX_VICTIMS];
    int     num_victims;
    double  per_pin_inductance_nh;  /* package inductance per pin */
    double  dI_dt_ma_per_ns;        /* current slew rate */
    int     is_safe;
    int     error_pin_index;        /* first failing pin, -1 if none */
} SsnAnalyzer;

void  ssn_init(SsnAnalyzer *ssn);
void  ssn_add_aggressor(SsnAnalyzer *ssn);
void  ssn_set_slew(SsnAnalyzer *ssn, double slew_ns);
int   ssn_check(const SsnAnalyzer *ssn, double noise_margin_mv);
double ssn_estimate_noise(const SsnAnalyzer *ssn);

/* -------------------------------------------------------
 * Pin Swapping — swaps equivalent pins to ease routing
 * ------------------------------------------------------- */
typedef struct {
    int     is_swappable[4];        /* [0]=DATA, [1]=ADDR, [2]=CTRL, [3]=CLK */
    int     pin_group_a[32];
    int     pin_group_b[32];
    int     group_size;
    int     swaps_performed;
    double  wirelength_before;
    double  wirelength_after;
    double  improvement_pct;
} PinSwapper;

void  pin_swap_init(PinSwapper *ps);
int   pin_swap_add_pair(PinSwapper *ps, int pin_a, int pin_b);
int   pin_swap_evaluate(PinSwapper *ps);
int   pin_swap_apply(PinSwapper *ps);

/* -------------------------------------------------------
 * I/O Planner — full chip-level I/O assignment
 * ------------------------------------------------------- */
typedef struct {
    IoBank      banks[MAX_IO_BANKS];
    IoPin       all_pins[MAX_TOTAL_PINS];
    int         num_banks;
    int         num_pins_total;
    int         num_pins_assigned;
    char        part_name[64];
    char        board_name[64];
    SsnAnalyzer  ssn;
    PinSwapper   swapper;
    int         warn_ssn;
    int         idle_delay_ps;
} IoPlanner;

void  io_planner_init(IoPlanner *ip, const char *part);
int   io_planner_add_bank(IoPlanner *ip, const IoBank *bank);
int   io_planner_assign_pin(IoPlanner *ip, const char *package_pin,
                             const char *signal, int dir, int iostd);
int   io_planner_auto_assign(IoPlanner *ip, char **signal_list, int count);
int   io_planner_validate(IoPlanner *ip);
void  io_planner_report(const IoPlanner *ip);
void  io_planner_export_xdc(const IoPlanner *ip, const char *filename);

/* -------------------------------------------------------
 * LVDS Interface Helper
 * ------------------------------------------------------- */
typedef struct {
    int     data_rate_mbps;
    int     bit_width;
    int     is_serialized;       /* OSERDES/ISERDES usage */
    double  clk_freq_mhz;
    double  setup_ns;
    double  hold_ns;
    int     termination;         /* internal/external/diff */
    char    p_pin[16];
    char    n_pin[16];
    double  vcm_v;              /* common-mode voltage */
    double  vod_mv;             /* differential output voltage */
} LvdsInterface;

void  lvds_init(LvdsInterface *lvds, double clk_mhz, int width);
int   lvds_assign_pins(LvdsInterface *lvds, const char *p, const char *n);
void  lvds_calc_timing(LvdsInterface *lvds);

#endif /* IO_PLANNING_H */
