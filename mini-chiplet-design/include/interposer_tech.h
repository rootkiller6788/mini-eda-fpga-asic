#ifndef INTERPOSER_TECH_H
#define INTERPOSER_TECH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define INTERPOSER_MAX_DIES      16
#define INTERPOSER_MAX_TSVS      65536
#define INTERPOSER_MAX_ROUTES    4096
#define RDL_MAX_LAYERS           6
#define MICROBUMP_PITCH_UM       55
#define TSV_DIAMETER_UM          10
#define TSV_ASPECT_RATIO         10

typedef enum {
    INTERPOSER_SILICON = 0,
    INTERPOSER_ORGANIC_EMIB,
    INTERPOSER_GLASS,
    INTERPOSER_SILICON_BRIDGE,
    INTERPOSER_RDL_FANOUT
} interposer_type_t;

typedef enum {
    MICROBUMP_CU_PILLAR = 0,
    MICROBUMP_SN_AG,
    MICROBUMP_AU_STUD,
    MICROBUMP_HYBRID_BOND
} microbump_type_t;

typedef struct {
    double x_um;
    double y_um;
    double width_um;
    double height_um;
    double thickness_um;
    double power_w;
    char   name[32];
} die_geometry_t;

typedef struct {
    uint32_t  id;
    double    x_um;
    double    y_um;
    uint32_t  die_src;
    uint32_t  die_dst;
    uint32_t  signal_count;
    double    pitch_um;
    microbump_type_t bump_type;
} microbump_array_t;

typedef struct {
    uint32_t    id;
    double      x_um;
    double      y_um;
    double      capacitance_ff;
    double      resistance_mohm;
    uint8_t     rdl_layer;
} tsv_site_t;

typedef struct {
    uint32_t id;
    uint32_t layer;
    double   start_x_um;
    double   start_y_um;
    double   end_x_um;
    double   end_y_um;
    double   width_um;
    double   length_um;
    uint32_t signal_id;
} rdl_trace_t;

typedef struct {
    interposer_type_t type;
    double  width_mm;
    double  height_mm;
    double  thickness_um;
    double  warpage_um;
    uint32_t num_rdl_layers;
    double  dielectric_constant;
    double  loss_tangent;
    double  youngs_modulus_gpa;
    double  cte_ppm_per_k;
} interposer_spec_t;

typedef struct {
    uint32_t          num_dies;
    die_geometry_t    dies[INTERPOSER_MAX_DIES];
    uint32_t          num_microbumps;
    microbump_array_t microbumps[INTERPOSER_MAX_ROUTES];
    uint32_t          num_tsvs;
    tsv_site_t        tsvs[INTERPOSER_MAX_TSVS];
    uint32_t          num_traces;
    rdl_trace_t       traces[INTERPOSER_MAX_ROUTES];
    interposer_spec_t spec;
    double            total_power_w;
    double            ir_drop_mv;
} interposer_t;

void interposer_init(interposer_t *ip, interposer_type_t type,
                     double width_mm, double height_mm);
int  interposer_place_die(interposer_t *ip, const die_geometry_t *die);
int  interposer_remove_die(interposer_t *ip, uint32_t die_index);
int  interposer_add_microbump(interposer_t *ip, const microbump_array_t *mb);
int  interposer_add_tsv(interposer_t *ip, const tsv_site_t *tsv);
int  interposer_add_rdl_trace(interposer_t *ip, const rdl_trace_t *trace);

double interposer_calc_warpage(const interposer_t *ip, double delta_t_k);
double interposer_calc_thermal_resistance(const interposer_t *ip);
double interposer_calc_ir_drop(const interposer_t *ip, double total_current_a);
double interposer_calc_signal_delay(const rdl_trace_t *trace);

int  interposer_route_die_to_die(interposer_t *ip,
                                 uint32_t die_a, uint32_t die_b,
                                 uint32_t signal_count, uint8_t rdl_layer);
void interposer_optimize_placement(interposer_t *ip);

int  interposer_verify_drc(const interposer_t *ip);
void interposer_print_summary(const interposer_t *ip);
void interposer_export_3d_view(const interposer_t *ip, const char *filename);

double emib_bridge_capacity_gbps(double bridge_width_mm, uint32_t layer_count);

#ifdef __cplusplus
}
#endif

#endif
