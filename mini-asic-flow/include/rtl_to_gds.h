#ifndef RTL_TO_GDS_H
#define RTL_TO_GDS_H

#include <stdint.h>
#include <stddef.h>

#define ASIC_STAGE_COUNT   10
#define MAX_CHECKPOINTS    64
#define MILESTONE_NAME_LEN 64

typedef enum {
    ASIC_STAGE_RTL_DESIGN       = 0,
    ASIC_STAGE_SYNTHESIS        = 1,
    ASIC_STAGE_DFT              = 2,
    ASIC_STAGE_FLOORPLAN        = 3,
    ASIC_STAGE_PLACEMENT        = 4,
    ASIC_STAGE_CTS              = 5,
    ASIC_STAGE_ROUTING          = 6,
    ASIC_STAGE_RC_EXTRACTION    = 7,
    ASIC_STAGE_STA              = 8,
    ASIC_STAGE_SIGNOFF          = 9
} asic_stage_t;

typedef enum {
    CHECKPOINT_PASS  = 0,
    CHECKPOINT_FAIL  = 1,
    CHECKPOINT_WARN  = 2,
    CHECKPOINT_SKIP  = 3
} checkpoint_status_t;

typedef struct {
    asic_stage_t      stage;
    int               checkpoint_id;
    char              name[MILESTONE_NAME_LEN];
    checkpoint_status_t status;
    double            metric_value;
    double            metric_target;
    int64_t           timestamp;
} design_checkpoint_t;

typedef struct {
    asic_stage_t      stage;
    char              milestone[MILESTONE_NAME_LEN];
    int               total_cells;
    int               total_nets;
    double            area_um2;
    double            power_mw;
    double            clk_period_ns;
    double            max_slack_ns;
    double            min_slack_ns;
    int               drc_errors;
    int               lvs_errors;
    int               hold_violations;
    int               setup_violations;
} design_milestone_t;

typedef struct {
    int               stage_index;
    asic_stage_t      stage_id;
    design_milestone_t milestone;
    design_checkpoint_t checkpoints[MAX_CHECKPOINTS];
    int               checkpoint_count;
    int               completed;
} asic_flow_stage_t;

typedef struct {
    asic_flow_stage_t stages[ASIC_STAGE_COUNT];
    int               current_stage;
    int               total_stages;
    double            target_freq_mhz;
    double            target_area_um2;
    double            target_power_mw;
    int               tech_node_nm;
    int               metal_layers;
} asic_flow_t;

void asic_flow_init(asic_flow_t *flow, int tech_node_nm, int metal_layers);
void asic_flow_set_targets(asic_flow_t *flow, double freq_mhz, double area_um2, double power_mw);

int  asic_flow_advance_stage(asic_flow_t *flow, asic_stage_t stage,
                             const design_milestone_t *milestone);
int  asic_flow_add_checkpoint(asic_flow_t *flow, asic_stage_t stage,
                              const design_checkpoint_t *cp);
int  asic_flow_verify_stage(const asic_flow_t *flow, asic_stage_t stage);
int  asic_flow_is_signed_off(const asic_flow_t *flow);

const char              *asic_stage_name(asic_stage_t stage);
const design_milestone_t *asic_flow_get_milestone(const asic_flow_t *flow, asic_stage_t stage);
void                      asic_flow_report(const asic_flow_t *flow);

int  asic_flow_write_checkpoints(const asic_flow_t *flow, const char *filename);
int  asic_flow_read_checkpoints(asic_flow_t *flow, const char *filename);

double asic_flow_total_timing_slack(const asic_flow_t *flow);
int    asic_flow_total_drc_errors(const asic_flow_t *flow);

#endif
