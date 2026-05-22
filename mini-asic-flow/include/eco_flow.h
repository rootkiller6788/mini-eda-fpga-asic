#ifndef ECO_FLOW_H
#define ECO_FLOW_H

#include <stdint.h>

#define MAX_ECO_OPS        128
#define MAX_SPARE_CELLS    256
#define MAX_NET_CHANGES    128
#define MAX_ECO_ITERATIONS 16

typedef enum {
    ECO_FUNCTIONAL      = 0,
    ECO_METAL_ONLY      = 1,
    ECO_TIMING          = 2,
    ECO_POWER           = 3,
    ECO_DRC             = 4
} eco_type_t;

typedef enum {
    ECO_OP_ADD_CELL     = 0,
    ECO_OP_REMOVE_CELL  = 1,
    ECO_OP_RECONNECT    = 2,
    ECO_OP_CHANGE_DRIVE = 3,
    ECO_OP_INSERT_BUFFER = 4,
    ECO_OP_SPLIT_NET    = 5,
    ECO_OP_MERGE_NET    = 6,
    ECO_OP_TIE_HIGH     = 7,
    ECO_OP_TIE_LOW      = 8
} eco_op_type_t;

typedef enum {
    ECO_STATE_PENDING   = 0,
    ECO_STATE_APPLIED   = 1,
    ECO_STATE_VERIFIED  = 2,
    ECO_STATE_REJECTED  = 3,
    ECO_STATE_REVERTED  = 4
} eco_state_t;

typedef struct {
    int          id;
    eco_op_type_t op_type;
    char         cell_name[32];
    char         from_net[32];
    char         to_net[32];
    char         from_cell[32];
    char         to_cell[32];
    int          pin_index;
    double       x_um;
    double       y_um;
    int          new_drive_strength;
    int          metal_layer;
    eco_state_t  state;
    int          iteration;
} eco_operation_t;

typedef struct {
    int    id;
    char   cell_name[32];
    int    is_used;
    int    is_broken;
    double x_um;
    double y_um;
    int    input_pin_count;
    int    output_pin_count;
} spare_cell_t;

typedef struct {
    spare_cell_t spare_cells[MAX_SPARE_CELLS];
    int          spare_count;
    int          spare_strategy;
    double       spare_density_pct;
    double       column_spacing_um;
    double       row_spacing_um;
} spare_cell_array_t;

typedef struct {
    eco_operation_t operations[MAX_ECO_OPS];
    int             operation_count;
    int             next_id;
    eco_type_t      eco_type;
    int             current_iteration;
    int             max_iterations;

    int             cells_added;
    int             cells_removed;
    int             nets_changed;
    int             buffers_inserted;

    spare_cell_array_t spares;
} eco_flow_t;

void eco_flow_init(eco_flow_t *eco, eco_type_t type);
void eco_flow_set_max_iterations(eco_flow_t *eco, int max_iter);

int  eco_add_cell(eco_flow_t *eco, const char *cell_name,
                   double x_um, double y_um);
int  eco_remove_cell(eco_flow_t *eco, const char *cell_name);
int  eco_reconnect_net(eco_flow_t *eco, const char *from_net,
                        const char *to_net, const char *cell, int pin);
int  eco_insert_buffer(eco_flow_t *eco, const char *net_name,
                        int stage, int drive_strength);
int  eco_change_drive_strength(eco_flow_t *eco, const char *cell_name,
                                int new_strength);
int  eco_tie_pin(eco_flow_t *eco, const char *cell, int pin, int tie_high);

int  eco_apply_operations(eco_flow_t *eco);
int  eco_verify_operations(eco_flow_t *eco);
int  eco_revert_operation(eco_flow_t *eco, int op_index);

int  eco_metal_only_add_route(eco_flow_t *eco, const char *cell,
                               int layer, double x1, double y1,
                               double x2, double y2);

void spare_cell_init(spare_cell_array_t *spa, double density_pct,
                     double col_spacing, double row_spacing);
int  spare_cell_place(spare_cell_array_t *spa, const floorplan_t *fp,
                      int cell_count);
int  spare_cell_allocate(spare_cell_array_t *spa, const char *cell_name,
                          double x_um, double y_um);
int  spare_cell_release(spare_cell_array_t *spa, int spare_id);
int  spare_cell_count_available(const spare_cell_array_t *spa);
int  spare_cell_count_used(const spare_cell_array_t *spa);

int  eco_flow_save(const eco_flow_t *eco, const char *filename);
int  eco_flow_load(eco_flow_t *eco, const char *filename);
int  eco_flow_signoff(const eco_flow_t *eco);

void eco_flow_report(const eco_flow_t *eco);
void spare_cell_report(const spare_cell_array_t *spa);

#endif
