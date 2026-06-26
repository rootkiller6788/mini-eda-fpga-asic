/* tests/test_place.c */
#include "place_fpga.h"
#include "timing_fpga.h"
#include <stdio.h>
#include <assert.h>
static int p=0,f=0;
#define T(n) printf("  TEST: %s ... ",n)
#define P() do{printf("PASS\n");p++;}while(0)
#define F(m) do{printf("FAIL: %s\n",m);f++;}while(0)

int main(void) {
    printf("=== Test: FPGA Placement ===\n");

    T("Placement init");{
        FpgaPlacement pl; placement_init(&pl,8,8);
        assert(pl.grid_w==8); assert(pl.grid_h==8);
        assert(pl.num_blocks==0); P();
    }

    T("Add blocks");{
        FpgaPlacement pl; placement_init(&pl,4,4);
        for(int i=0;i<10;i++){
            int idx=placement_add_block(&pl,i);
            assert(idx==i);
        }
        assert(pl.num_blocks==10);
        assert(pl.blocks[5].clb_id==5);
        P();
    }

    T("Fix block");{
        FpgaPlacement pl; placement_init(&pl,4,4);
        placement_add_block(&pl,0);
        placement_fix_block(&pl,0,2,3);
        assert(pl.blocks[0].x==2); assert(pl.blocks[0].y==3);
        assert(pl.blocks[0].is_fixed); P();
    }

    T("HPWL computation");{
        FpgaPlacement pl; placement_init(&pl,4,4);
        placement_add_block(&pl,0); pl.blocks[0].x=0; pl.blocks[0].y=0;
        placement_add_block(&pl,1); pl.blocks[1].x=3; pl.blocks[1].y=3;
        FpgaNet nets[1];
        nets[0].net_id=0; nets[0].source_node=0;
        nets[0].sink_nodes[0]=1; nets[0].num_sinks=1;
        double wl=placement_hpwl(&pl,nets,1);
        assert(wl==6.0); /* |3-0|+|3-0|=6 */
        P();
    }

    T("Wirelength single net");{
        FpgaPlacement pl; placement_init(&pl,4,4);
        placement_add_block(&pl,0); pl.blocks[0].x=1; pl.blocks[0].y=2;
        placement_add_block(&pl,1); pl.blocks[1].x=3; pl.blocks[1].y=5;
        FpgaNet net; net.source_node=0; net.sink_nodes[0]=1; net.num_sinks=1;
        double wl=placement_wirelength_net(&pl,&net);
        assert(wl==5.0);
        P();
    }

    T("Bounding box");{
        FpgaPlacement pl; placement_init(&pl,4,4);
        placement_add_block(&pl,0); pl.blocks[0].x=1; pl.blocks[0].y=1;
        placement_add_block(&pl,1); pl.blocks[1].x=3; pl.blocks[1].y=4;
        FpgaNet net; net.source_node=0; net.sink_nodes[0]=1; net.num_sinks=1;
        FpgaBoundingBox bb=placement_net_bbox(&pl,&net);
        assert(bb.xmin==1); assert(bb.xmax==3);
        assert(bb.ymin==1); assert(bb.ymax==4);
        P();
    }

    T("Simulated annealing");{
        FpgaPlacement pl; placement_init(&pl,4,4);
        for(int i=0;i<8;i++) placement_add_block(&pl,i);
        FpgaNet nets[4];
        for(int i=0;i<4;i++){
            nets[i].net_id=i; nets[i].source_node=i;
            nets[i].sink_nodes[0]=i+4; nets[i].num_sinks=1;
        }
        FpgaPlaceParams params;
        params.T_start=10.0; params.T_end=0.01; params.alpha=0.9;
        params.moves_per_temp=10; params.max_iterations=500;
        params.timing_tradeoff=0.0; params.seed=42;
        int ret=place_simulated_annealing(&pl,nets,4,&params);
        assert(ret==0); assert(pl.wirelength>=0);
        P();
    }

    T("Quadratic placement");{
        FpgaPlacement pl; placement_init(&pl,4,4);
        for(int i=0;i<6;i++) placement_add_block(&pl,i);
        FpgaNet nets[3];
        for(int i=0;i<3;i++){
            nets[i].net_id=i; nets[i].source_node=i;
            nets[i].sink_nodes[0]=i+3; nets[i].num_sinks=1;
        }
        int ret=place_quadratic(&pl,nets,3);
        assert(ret==0);
        P();
    }

    T("Timing-driven placement");{
        FpgaPlacement pl; placement_init(&pl,4,4);
        for(int i=0;i<8;i++) placement_add_block(&pl,i);
        FpgaNet nets[4];
        for(int i=0;i<4;i++){
            nets[i].net_id=i; nets[i].source_node=i;
            nets[i].sink_nodes[0]=i+4; nets[i].num_sinks=1;
            nets[i].timing_criticality=0.5;
        }
        FpgaPlaceParams params;
        params.T_start=10.0; params.T_end=0.1; params.alpha=0.9;
        params.moves_per_temp=10; params.max_iterations=200;
        params.timing_tradeoff=0.5; params.seed=42;
        int ret=place_timing_driven(&pl,nets,4,&params,NULL);
        assert(ret==0);
        P();
    }

    T("Multi-FPGA partition");{
        FpgaPlacement pl; placement_init(&pl,8,8);
        for(int i=0;i<16;i++) placement_add_block(&pl,i);
        FpgaNet nets[8];
        for(int i=0;i<8;i++){
            nets[i].net_id=i; nets[i].source_node=i;
            nets[i].sink_nodes[0]=i+8; nets[i].num_sinks=1;
        }
        int cuts=place_partition_multi_fpga(&pl,nets,8,2);
        assert(cuts>=0);
        P();
    }

    T("Move propose/apply");{
        FpgaPlacement pl; placement_init(&pl,4,4);
        for(int i=0;i<8;i++) placement_add_block(&pl,i);
        int a,b; FpgaPlaceMove type;
        place_propose_move(&pl,&a,&b,&type);
        assert(a>=0&&a<8); assert(b>=0&&b<8);
        place_apply_move(&pl,a,b,type);
        P();
    }

    T("Legal placement");{
        FpgaPlacement pl; placement_init(&pl,2,2);
        placement_add_block(&pl,0); pl.blocks[0].x=0; pl.blocks[0].y=0;
        placement_add_block(&pl,1); pl.blocks[1].x=0; pl.blocks[1].y=0;
        assert(!placement_is_legal(&pl));
        pl.blocks[1].x=1;
        assert(placement_is_legal(&pl));
        P();
    }

    printf("\n=== Results: %d passed, %d failed ===\n",p,f);
    return f>0?1:0;
}
