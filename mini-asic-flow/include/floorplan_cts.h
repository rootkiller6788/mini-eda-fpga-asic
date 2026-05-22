#ifndef FLOORPLAN_CTS_H
#define FLOORPLAN_CTS_H

#include <stdint.h>

#define MAX_MACROS        64
#define MAX_IO_PADS       256
#define MAX_CLOCK_DOMAINS 16
#define MAX_CTS_BUFFERS   128
#define MAX_GRID_SEGMENTS 512

typedef enum {
    MACRO_MEMORY    = 0,
    MACRO_ANALOG    = 1,
    MACRO_IP        = 2,
    MACRO_PLL       = 3,
    MACRO_PHY       = 4
} macro_type_t;

typedef struct {
    int     id;
    char    name[32];
    macro_type_t type;
    double  width_um;
    double  height_um;
    double  x_um;
    double  y_um;
    int     orientation;
    double  keepout_margin_um;
    int     is_locked;
} macro_block_t;

typedef enum {
    IO_POWER   = 0,
    IO_GROUND  = 1,
    IO_SIGNAL  = 2,
    IO_ANALOG  = 3,
    IO_CLOCK   = 4,
    IO_NC      = 5
} io_pad_type_t;

typedef struct {
    int           id;
    char          name[32];
    io_pad_type_t type;
    double        width_um;
    double        height_um;
    double        x_um;
    double        y_um;
    int           side;
    double        pitch_um;
    double        drive_ma;
} io_pad_t;

typedef enum {
    CLK_H_TREE   = 0,
    CLK_CLOCK_MESH = 1,
    CLK_SPINE    = 2,
    CLK_BALANCED_TREE = 3
} clock_topology_t;

typedef struct {
    int    id;
    char   name[32];
    double period_ns;
    double skew_target_ps;
    double latency_target_ns;
    double insertion_delay_ns;
    double global_skew_ps;
    double local_skew_ps;
    int    sink_count;
    int    buffer_count;
    int    buffer_stages;
} clock_domain_t;

typedef struct {
    int    id;
    double x_um;
    double y_um;
    int    stage;
    int    drive_strength;
    double delay_ps;
    int    is_root;
    int    parent_id;
} cts_buffer_t;

typedef struct {
    int    domain_id;
    double x1_um, y1_um, x2_um, y2_um;
    double wire_rc_ps;
} cts_wire_segment_t;

typedef struct {
    double          core_width_um;
    double          core_height_um;
    double          core_area_um2;
    double          die_width_um;
    double          die_height_um;

    macro_block_t   macros[MAX_MACROS];
    int             macro_count;
    double          macro_utilization;

    io_pad_t        io_pads[MAX_IO_PADS];
    int             io_count;
    double          io_pitch_um;

    double          pg_metal_width_um;
    double          pg_pitch_um;
    int             pg_layers;
    double          pg_vdd_v;
    double          pg_ir_drop_target_mv;

    double          row_height_um;
    int             row_count;
    double          cell_utilization;

    int             metal_layers;
    double          metal_pitch_um[12];
    double          metal_width_um[12];
} floorplan_t;

typedef struct {
    clock_topology_t topology;
    clock_domain_t   domains[MAX_CLOCK_DOMAINS];
    int              domain_count;

    cts_buffer_t     buffers[MAX_CTS_BUFFERS];
    int              buffer_count;

    cts_wire_segment_t wires[MAX_GRID_SEGMENTS];
    int                wire_count;

    double           max_transition_ps;
    double           max_capacitance_ff;
    double           max_fanout;
    int              total_sinks;
} clock_tree_t;

void floorplan_init(floorplan_t *fp, double core_w, double core_h,
                    int metal_layers);
int  floorplan_place_macro(floorplan_t *fp, const macro_block_t *macro);
int  floorplan_add_io_pad(floorplan_t *fp, const io_pad_t *pad);
int  floorplan_auto_place_io_ring(floorplan_t *fp, double pitch_um);
int  floorplan_create_pg_grid(floorplan_t *fp, double width_um, double pitch_um,
                               int layers, double vdd_v);
double floorplan_utilization(const floorplan_t *fp);
double floorplan_chip_area(const floorplan_t *fp);
int    floorplan_check_macro_overlap(const floorplan_t *fp);

void clock_tree_init(clock_tree_t *ct, clock_topology_t topo);
int  clock_tree_add_domain(clock_tree_t *ct, const char *name,
                            double period_ns, double skew_target_ps);
int  clock_tree_build_h_tree(clock_tree_t *ct, int domain_id,
                              double cx_um, double cy_um,
                              double width_um, double height_um,
                              int depth);
int  clock_tree_build_clock_mesh(clock_tree_t *ct, int domain_id,
                                  double x0, double y0,
                                  double x1, double y1,
                                  int rows, int cols);
int  clock_tree_insert_buffers(clock_tree_t *ct, int domain_id,
                                int drive_strength, int stages);
int  clock_tree_compute_skew(const clock_tree_t *ct, int domain_id,
                              double *global_skew_ps, double *local_skew_ps);
int  clock_tree_balance_skew(clock_tree_t *ct, int domain_id,
                              double target_ps);

double cts_elmore_delay(double resistance_ohm, double capacitance_ff,
                         double driver_res_ohm);
double cts_wire_rc(double length_um, double width_um);
int    cts_optimal_buffer_depth(int sink_count, double max_fanout);

void floorplan_report(const floorplan_t *fp);
void clock_tree_report(const clock_tree_t *ct);

#endif
