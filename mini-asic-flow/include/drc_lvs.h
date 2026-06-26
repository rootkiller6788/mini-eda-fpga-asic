#ifndef DRC_LVS_H
#define DRC_LVS_H

#include <stdint.h>

#define MAX_DRC_RULES      128
#define MAX_DRC_VIOLATIONS 512
#define MAX_LVS_DEVICES    1024
#define MAX_LVS_NETS       2048
#define MAX_ANTENNA_NODES  256
#define MAX_ERC_RULES      64

typedef enum {
    DRC_SPACING   = 0,
    DRC_WIDTH     = 1,
    DRC_ENCLOSURE = 2,
    DRC_OVERLAP   = 3,
    DRC_VIA       = 4,
    DRC_DENSITY   = 5,
    DRC_OFFGRID   = 6,
    DRC_NOTCH     = 7,
    DRC_AREA      = 8,
    DRC_EXTENSION = 9
} drc_rule_type_t;

typedef enum {
    DRC_SEVERITY_INFO     = 0,
    DRC_SEVERITY_WARNING  = 1,
    DRC_SEVERITY_ERROR    = 2,
    DRC_SEVERITY_CRITICAL = 3
} drc_severity_t;

typedef enum {
    LVS_MATCH      = 0,
    LVS_MISMATCH   = 1,
    LVS_MISSING    = 2,
    LVS_EXTRA      = 3,
    LVS_SHORT      = 4,
    LVS_OPEN       = 5
} lvs_status_t;

typedef enum {
    ERC_WELL_TIE    = 0,
    ERC_FLOATING    = 1,
    ERC_HOT_WELL    = 2,
    ERC_POWER_STRAP = 3,
    ERC_ESD_PATH    = 4,
    ERC_LATCHUP     = 5
} erc_rule_type_t;

typedef struct {
    int            id;
    drc_rule_type_t type;
    int            layer;
    double         min_width_nm;
    double         min_spacing_nm;
    double         min_enclosure_nm;
    double         min_area_nm2;
    double         max_density_pct;
    drc_severity_t severity;
    char           name[64];
} drc_rule_t;

typedef struct {
    int            id;
    drc_rule_type_t type;
    int            rule_id;
    int            layer;
    double         x1_nm, y1_nm, x2_nm, y2_nm;
    double         measured_value;
    double         required_value;
    drc_severity_t severity;
    char           description[128];
    int            is_fixed;
} drc_violation_t;

typedef struct {
    int          net_id;
    char         net_name[64];
    int          device_count;
    int          pin_count;
    double       capacitance_ff;
    double       resistance_ohm;
} lvs_net_t;

typedef struct {
    char         name[64];
    char         type[32];
    double       width_nm;
    double       length_nm;
    int          source_net;
    int          drain_net;
    int          gate_net;
    int          body_net;
    double       w_nm;
    double       l_nm;
    int          fingers;
    int          multiplier;
    lvs_status_t match_status;
    char         mismatch_reason[128];
} lvs_device_t;

typedef struct {
    int          id;
    int          net_id;
    double       gate_area_nm2;
    double       antenna_ratio;
    double       max_antenna_ratio;
    int          is_violation;
    char         fix_method[64];
} antenna_check_t;

typedef struct {
    erc_rule_type_t type;
    int             severity;
    int             device_id;
    int             net_id;
    char            description[128];
} erc_violation_t;

typedef struct {
    drc_rule_t       rules[MAX_DRC_RULES];
    int              rule_count;

    drc_violation_t  violations[MAX_DRC_VIOLATIONS];
    int              violation_count;

    lvs_device_t     schematic_devices[MAX_LVS_DEVICES];
    int              schematic_device_count;
    lvs_net_t        schematic_nets[MAX_LVS_NETS];
    int              schematic_net_count;

    lvs_device_t     layout_devices[MAX_LVS_DEVICES];
    int              layout_device_count;
    lvs_net_t        layout_nets[MAX_LVS_NETS];
    int              layout_net_count;

    antenna_check_t  antenna_checks[MAX_ANTENNA_NODES];
    int              antenna_check_count;

    erc_violation_t  erc_violations[MAX_ERC_RULES];
    int              erc_violation_count;

    int              total_drc_errors;
    int              total_lvs_errors;
    int              total_antenna_errors;
    int              total_erc_errors;
} drc_lvs_engine_t;

void drc_lvs_init(drc_lvs_engine_t *eng);

int  drc_add_rule(drc_lvs_engine_t *eng, drc_rule_type_t type, int layer,
                  double min_width, double min_spacing, drc_severity_t severity);

int  drc_check_spacing(drc_lvs_engine_t *eng, int layer,
                       double x1, double y1, double x2, double y2,
                       double other_x1, double other_y1,
                       double other_x2, double other_y2);
int  drc_check_width(drc_lvs_engine_t *eng, int layer,
                      double x1, double y1, double x2, double y2);
int  drc_check_enclosure(drc_lvs_engine_t *eng, int inner_layer,
                          int outer_layer, double ix, double iy,
                          double iw, double ih, double ox, double oy,
                          double ow, double oh);
int  drc_check_area(drc_lvs_engine_t *eng, int layer,
                     double width, double height);
int  drc_check_density(drc_lvs_engine_t *eng, int layer,
                        double window_w, double window_h,
                        double filled_area);

int  drc_fix_push_cell(drc_lvs_engine_t *eng, int violation_id,
                        double dx_nm, double dy_nm);
int  drc_fix_widen_wire(drc_lvs_engine_t *eng, int violation_id,
                         double width_nm);
int  drc_fix_reduce_spacing(drc_lvs_engine_t *eng, int violation_id,
                             double factor);

int  lvs_add_schematic_device(drc_lvs_engine_t *eng, const char *name,
                               const char *type, double w, double l, int fingers);
int  lvs_add_schematic_net(drc_lvs_engine_t *eng, const char *name,
                            int device_count);
int  lvs_add_layout_device(drc_lvs_engine_t *eng, const char *name,
                            const char *type, double w, double l, int fingers);
int  lvs_add_layout_net(drc_lvs_engine_t *eng, const char *name,
                         int device_count);

int  lvs_compare(drc_lvs_engine_t *eng);
int  lvs_match_devices(drc_lvs_engine_t *eng, int s_idx, int l_idx);
int  lvs_report_mismatches(const drc_lvs_engine_t *eng);

int  antenna_check(drc_lvs_engine_t *eng, int net_id,
                    double gate_area_nm2, double max_ratio);
int  antenna_fix_add_diode(drc_lvs_engine_t *eng, int check_id);
int  antenna_fix_metal_jump(drc_lvs_engine_t *eng, int check_id,
                             int upper_layer);

int  erc_check_well_ties(drc_lvs_engine_t *eng, int device_id, int well_net);
int  erc_check_floating(drc_lvs_engine_t *eng, int net_id);
int  erc_check_hot_well(drc_lvs_engine_t *eng, int device_id);

int  drc_lvs_clean(drc_lvs_engine_t *eng);
int  drc_lvs_signoff(const drc_lvs_engine_t *eng);

void drc_lvs_report(const drc_lvs_engine_t *eng);
void drc_violation_report(const drc_violation_t *v);
void lvs_device_report(const lvs_device_t *d);
void antenna_report(const antenna_check_t *a);

double drc_rect_distance(double x1a, double y1a, double x2a, double y2a,
                          double x1b, double y1b, double x2b, double y2b);
int    drc_rect_overlap(double x1a, double y1a, double x2a, double y2a,
                         double x1b, double y1b, double x2b, double y2b);

#endif
