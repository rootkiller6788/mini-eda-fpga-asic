#ifndef STD_CELLS_H
#define STD_CELLS_H

#include <stdint.h>

#define MAX_LUT_ROWS         16
#define MAX_TIMING_ARCS      32
#define MAX_POWER_ENTRIES    32
#define MAX_LIB_CELLS        512
#define TRACK_HEIGHT_NAME_LEN 16

typedef enum {
    VT_LVT = 0,
    VT_SVT = 1,
    VT_HVT = 2,
    VT_ULVT = 3,
    VT_COUNT
} vt_type_t;

typedef enum {
    DRIVE_X1  = 0,
    DRIVE_X2  = 1,
    DRIVE_X4  = 2,
    DRIVE_X8  = 3,
    DRIVE_X16 = 4,
    DRIVE_X32 = 5,
    DRIVE_COUNT
} drive_strength_t;

typedef enum {
    ARC_CELL_RISE   = 0,
    ARC_CELL_FALL   = 1,
    ARC_RISE_RISE   = 2,
    ARC_RISE_FALL   = 3,
    ARC_FALL_RISE   = 4,
    ARC_FALL_FALL   = 5,
    ARC_SETUP_RISE  = 6,
    ARC_SETUP_FALL  = 7,
    ARC_HOLD_RISE   = 8,
    ARC_HOLD_FALL   = 9,
    ARC_COUNT
} timing_arc_type_t;

typedef enum {
    CELL_COMBINATIONAL = 0,
    CELL_SEQUENTIAL    = 1,
    CELL_CLOCK_GATE    = 2,
    CELL_FILLER        = 3,
    CELL_TAP           = 4,
    CELL_ANTENNA       = 5,
    CELL_ECO           = 6,
    CELL_PHYSICAL_ONLY = 7
} cell_category_t;

typedef struct {
    timing_arc_type_t type;
    int    related_pin;
    double cell_rise;
    double cell_fall;
    double rise_transition;
    double fall_transition;
    double rise_constraint;
    double fall_constraint;
    double input_cap_ff;
    double output_cap_ff;
} timing_arc_t;

typedef struct {
    int    index;
    double internal_power_uw;
    double leakage_power_nw;
    double switching_power_uw;
    double total_power_uw;
} power_entry_t;

typedef struct {
    int    index;
    double input_slew_ps;
    double output_load_ff;
    double cell_delay_ps;
    double output_slew_ps;
} delay_lut_entry_t;

typedef struct {
    delay_lut_entry_t entries[MAX_LUT_ROWS];
    int               row_count;
    timing_arc_type_t arc_type;
} delay_lut_t;

typedef struct {
    char            name[32];
    cell_category_t category;
    vt_type_t       vt;
    drive_strength_t drive;
    int             track_height;
    double          cell_height_nm;
    double          cell_width_nm;
    double          area_nm2;

    int             input_count;
    int             output_count;
    int             is_inverting;

    timing_arc_t    timing_arcs[MAX_TIMING_ARCS];
    int             timing_arc_count;

    power_entry_t   power_entries[MAX_POWER_ENTRIES];
    int             power_entry_count;

    delay_lut_t     delay_luts[MAX_TIMING_ARCS];
    int             delay_lut_count;

    double          leakage_power_nw;
    double          internal_power_uw;
    double          total_power_uw;
} std_cell_t;

typedef struct {
    char       name[32];
    int        tech_node_nm;
    double     track_height_nm;
    int        num_tracks;
    double     supply_voltage_v;
    int        fin_count;
    double     fin_pitch_nm;
    double     fin_height_nm;

    double     n_fin_width_nm;
    double     p_fin_width_nm;
    double     gate_length_nm;
    double     gate_pitch_nm;
    double     contacted_poly_pitch_nm;
    double     metal1_pitch_nm;

    std_cell_t cells[MAX_LIB_CELLS];
    int        cell_count;
} std_cell_lib_t;

void std_cell_init(std_cell_t *cell, const char *name, cell_category_t cat,
                   vt_type_t vt, drive_strength_t drive, int track_height);

void std_cell_add_timing_arc(std_cell_t *cell, timing_arc_type_t type,
                             double rise_delay, double fall_delay,
                             double input_cap, double output_cap);

void std_cell_add_power(std_cell_t *cell, int index,
                        double internal_uw, double leakage_nw);
void std_cell_add_delay_lut(std_cell_t *cell, timing_arc_type_t arc_type,
                            double slew_ps, double load_ff,
                            double delay_ps, double out_slew_ps);

void std_cell_lib_init(std_cell_lib_t *lib, const char *name,
                       int tech_node_nm, double track_height_nm,
                       int num_tracks, double supply_v);

void std_cell_lib_add_cell(std_cell_lib_t *lib, const std_cell_t *cell);

const std_cell_t *std_cell_lib_find(const std_cell_lib_t *lib, const char *name);
const std_cell_t *std_cell_lib_find_by_vt(const std_cell_lib_t *lib, vt_type_t vt);

double std_cell_get_delay(const std_cell_t *cell, timing_arc_type_t arc_type,
                          double slew_ps, double load_ff);

double std_cell_get_power(const std_cell_t *cell, double activity_factor,
                          double freq_mhz);

const char *vt_type_name(vt_type_t vt);
const char *drive_strength_name(drive_strength_t drive);
const char *cell_category_name(cell_category_t cat);

void std_cell_lib_report(const std_cell_lib_t *lib);
void std_cell_report(const std_cell_t *cell);

double finfet_drive_current_nA(double fin_width_nm, double fin_height_nm,
                               double gate_length_nm, vt_type_t vt);
double finfet_capacitance_ff(double fin_width_nm, double fin_height_nm,
                             double gate_length_nm);

#endif
