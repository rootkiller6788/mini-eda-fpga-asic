/* tests/test_flow.c - End-to-end FPGA flow test */
#include "fpga_flow.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
static int p=0,f=0;
#define T(n) printf("  TEST: %s ... ",n)
#define P() do{printf("PASS\n");p++;}while(0)
#define F(m) do{printf("FAIL: %s\n",m);f++;}while(0)

int main(void) {
    printf("=== Test: Full FPGA Flow ===\n");

    T("Flow config defaults");{
        FpgaFlowConfig cfg;
        flow_config_defaults(&cfg);
        assert(cfg.grid_width==8); assert(cfg.grid_height==8);
        assert(cfg.lut_size==4); assert(cfg.target_freq_mhz==100.0);
        P();
    }

    T("Flow init/destroy");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        FpgaFlow flow;
        flow_init(&flow,&cfg);
        assert(flow.fabric!=NULL); assert(flow.nets!=NULL);
        assert(flow.stage==FLOW_INPUT);
        flow_destroy(&flow);
        P();
    }

    T("Flow log");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        FpgaFlow flow; flow_init(&flow,&cfg);
        flow_log_msg(&flow,"test message");
        assert(strlen(flow.log)>0);
        flow_destroy(&flow);
        P();
    }

    T("Synthesis stage");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        FpgaFlow flow; flow_init(&flow,&cfg);
        bool ok=flow_run_synthesis(&flow);
        assert(ok); assert(flow.bool_net.num_nodes>0);
        assert(flow.stage==FLOW_SYNTHESIS);
        flow_destroy(&flow);
        P();
    }

    T("Technology mapping stage");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        FpgaFlow flow; flow_init(&flow,&cfg);
        flow_run_synthesis(&flow);
        bool ok=flow_run_techmap(&flow);
        assert(ok); assert(flow.lut_map.num_entries>0);
        assert(flow.stage==FLOW_TECHMAP);
        flow_destroy(&flow);
        P();
    }

    T("CLB packing stage");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        FpgaFlow flow; flow_init(&flow,&cfg);
        flow_run_synthesis(&flow);
        flow_run_techmap(&flow);
        bool ok=flow_run_packing(&flow);
        assert(ok); assert(flow.total_clbs>0);
        assert(flow.stage==FLOW_PACKING);
        flow_destroy(&flow);
        P();
    }

    T("Placement stage");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        FpgaFlow flow; flow_init(&flow,&cfg);
        flow_run_synthesis(&flow);
        flow_run_techmap(&flow);
        flow_run_packing(&flow);
        bool ok=flow_run_placement(&flow);
        assert(ok); assert(flow.stage==FLOW_PLACEMENT);
        flow_destroy(&flow);
        P();
    }

    T("Routing stage");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        FpgaFlow flow; flow_init(&flow,&cfg);
        flow_run_synthesis(&flow);
        flow_run_techmap(&flow);
        flow_run_packing(&flow);
        flow_run_placement(&flow);
        bool ok=flow_run_routing(&flow);
        assert(ok); assert(flow.stage==FLOW_ROUTING);
        flow_destroy(&flow);
        P();
    }

    T("Timing stage");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        FpgaFlow flow; flow_init(&flow,&cfg);
        flow_run_synthesis(&flow);
        flow_run_techmap(&flow);
        flow_run_packing(&flow);
        flow_run_placement(&flow);
        flow_run_routing(&flow);
        bool ok=flow_run_timing(&flow);
        assert(ok); assert(flow.stage==FLOW_TIMING);
        flow_destroy(&flow);
        P();
    }

    T("Bitstream stage");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        FpgaFlow flow; flow_init(&flow,&cfg);
        flow_run_synthesis(&flow); flow_run_techmap(&flow);
        flow_run_packing(&flow); flow_run_placement(&flow);
        flow_run_routing(&flow); flow_run_timing(&flow);
        bool ok=flow_run_bitstream(&flow);
        assert(ok); assert(flow.bitstream!=NULL);
        assert(flow.stage==FLOW_BITSTREAM);
        flow_destroy(&flow);
        P();
    }

    T("IO planning stage");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        FpgaFlow flow; flow_init(&flow,&cfg);
        flow_run_synthesis(&flow); flow_run_techmap(&flow);
        flow_run_packing(&flow); flow_run_placement(&flow);
        bool ok=flow_run_io_planning(&flow);
        assert(ok); assert(flow.io_ring.num_pads>0);
        flow_destroy(&flow);
        P();
    }

    T("Full flow end-to-end");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        cfg.grid_width=6; cfg.grid_height=6;
        FpgaFlow flow; flow_init(&flow,&cfg);
        bool ok=flow_run_all(&flow);
        assert(ok); assert(flow.stage==FLOW_COMPLETE);
        assert(flow.success);
        flow_destroy(&flow);
        P();
    }

    T("Flow report");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        cfg.grid_width=6; cfg.grid_height=6;
        FpgaFlow flow; flow_init(&flow,&cfg);
        flow_run_all(&flow);
        flow_print_report(&flow);
        flow_print_power_estimate(&flow);
        flow_destroy(&flow);
        P();
    }

    T("Download package generation");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        cfg.grid_width=6; cfg.grid_height=6;
        FpgaFlow flow; flow_init(&flow,&cfg);
        flow_run_all(&flow);
        bool ok=flow_generate_download(&flow,"test_design");
        assert(ok);
        /* Clean up generated files */
        remove("test_design.bin");
        remove("test_design.xdc");
        flow_destroy(&flow);
        P();
    }

    T("Partial reconfiguration disabled");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        cfg.enable_partial_reconfig=false;
        FpgaFlow flow; flow_init(&flow,&cfg);
        bool ok=flow_partial_reconfig(&flow,0,0,2,2);
        assert(!ok); /* disabled by config */
        flow_destroy(&flow);
        P();
    }

    T("Auto-tune placement params");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        cfg.grid_width=4; cfg.grid_height=4;
        FpgaFlow flow; flow_init(&flow,&cfg);
        for(int i=0;i<4;i++) placement_add_block(&flow.placement,i);
        flow_auto_tune_place_params(&flow);
        assert(flow.config.place_params.T_start>0);
        flow_destroy(&flow);
        P();
    }

    T("Flow config verbose");{
        FpgaFlowConfig cfg; flow_config_defaults(&cfg);
        cfg.verbose=true;
        cfg.grid_width=4; cfg.grid_height=4;
        FpgaFlow flow; flow_init(&flow,&cfg);
        flow_run_all(&flow);
        flow_destroy(&flow);
        P();
    }

    printf("\n=== Results: %d passed, %d failed ===\n",p,f);
    return f>0?1:0;
}
