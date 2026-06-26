#include "rtl_to_gds.h"
#include "std_cells.h"
#include <stdio.h>

int main(void)
{
    asic_flow_t flow;
    design_milestone_t ms;
    int i;

    asic_flow_init(&flow, 7, 11);
    asic_flow_set_targets(&flow, 2000.0, 500000.0, 150.0);

    printf("=== ASIC Flow: 7nm, 11-metal, 2GHz Target ===\n\n");

    for (i = 0; i < ASIC_STAGE_COUNT; i++) {
        memset(&ms, 0, sizeof(ms));
        snprintf(ms.milestone, sizeof(ms.milestone), "%s milestone",
                 asic_stage_name((asic_stage_t)i));
        ms.total_cells     = 500000 + i * 100000;
        ms.total_nets      = ms.total_cells * 2;
        ms.area_um2        = 300000.0 + i * 20000.0;
        ms.power_mw        = 80.0 + i * 10.0;
        ms.clk_period_ns   = 0.5;
        ms.max_slack_ns    = 0.15 - i * 0.01;
        ms.min_slack_ns    = -0.05 + i * 0.005;
        ms.drc_errors      = (i > 7) ? 0 : 50 - i * 6;
        ms.lvs_errors      = 0;
        ms.setup_violations = (i > 6) ? 0 : 10 - i;
        ms.hold_violations  = (i > 5) ? 0 : 5 - i / 2;

        asic_flow_advance_stage(&flow, (asic_stage_t)i, &ms);

        {
            design_checkpoint_t cp;
            cp.stage          = (asic_stage_t)i;
            cp.checkpoint_id  = i * 10 + 1;
            snprintf(cp.name, sizeof(cp.name), "%s_check",
                     asic_stage_name((asic_stage_t)i));
            cp.status         = CHECKPOINT_PASS;
            cp.metric_value   = 95.0 - i * 0.5;
            cp.metric_target  = 90.0;
            cp.timestamp      = 1000 + i * 3600;
            asic_flow_add_checkpoint(&flow, (asic_stage_t)i, &cp);
        }
    }

    asic_flow_report(&flow);

    printf("\nTotal timing slack: %.3f ns\n", asic_flow_total_timing_slack(&flow));
    printf("Total DRC errors:   %d\n", asic_flow_total_drc_errors(&flow));
    printf("Signed off:         %s\n",
           asic_flow_is_signed_off(&flow) ? "YES" : "NO");

    return 0;
}
