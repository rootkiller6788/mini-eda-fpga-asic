/* tests/test_timing.c */
#include "timing_fpga.h"
#include "routing_fabric.h"
#include <stdio.h>
#include <assert.h>
static int p=0,f=0;
#define T(n) printf("  TEST: %s ... ",n)
#define P() do{printf("PASS\n");p++;}while(0)
#define F(m) do{printf("FAIL: %s\n",m);f++;}while(0)

int main(void) {
    printf("=== Test: Timing Analysis ===\n");

    T("Timing graph create");{
        FpgaTimingGraph *tg=timing_graph_create();
        assert(tg); assert(tg->num_nodes==0); assert(tg->num_edges==0);
        timing_graph_destroy(tg); P();
    }

    T("Add timing nodes");{
        FpgaTimingGraph *tg=timing_graph_create();
        int n1=timing_graph_add_node(tg,TNODE_INPUT);
        int n2=timing_graph_add_node(tg,TNODE_COMB);
        int n3=timing_graph_add_node(tg,TNODE_OUTPUT);
        assert(n1==0); assert(n2==1); assert(n3==2);
        timing_graph_destroy(tg); P();
    }

    T("Add timing edge");{
        FpgaTimingGraph *tg=timing_graph_create();
        int a=timing_graph_add_node(tg,TNODE_INPUT);
        int b=timing_graph_add_node(tg,TNODE_COMB);
        int e=timing_graph_add_edge(tg,a,b,0.8,0.2);
        assert(e==0); assert(tg->edges[0].delay==0.8);
        assert(tg->edges[0].wire_delay==0.2);
        assert(tg->nodes[a].num_fanouts==1);
        assert(tg->nodes[b].num_fanins==1);
        timing_graph_destroy(tg); P();
    }

    T("Set node delay");{
        FpgaTimingGraph *tg=timing_graph_create();
        int n=timing_graph_add_node(tg,TNODE_COMB);
        timing_graph_set_node_delay(tg,n,0.5);
        assert(tg->nodes[n].delay==0.5);
        timing_graph_destroy(tg); P();
    }

    T("Arrival time computation");{
        FpgaTimingGraph *tg=timing_graph_create();
        int pi=timing_graph_add_node(tg,TNODE_INPUT);
        int cb=timing_graph_add_node(tg,TNODE_COMB);
        int po=timing_graph_add_node(tg,TNODE_OUTPUT);
        timing_graph_add_edge(tg,pi,cb,1.0,0.2);
        timing_graph_add_edge(tg,cb,po,1.5,0.3);
        assert(sta_compute_arrival(tg)==0);
        assert(tg->nodes[pi].arrival_time==0.0);
        assert(tg->nodes[cb].arrival_time>=1.0);
        assert(tg->nodes[po].arrival_time>=2.5);
        timing_graph_destroy(tg); P();
    }

    T("Required time computation");{
        FpgaTimingGraph *tg=timing_graph_create();
        int pi=timing_graph_add_node(tg,TNODE_INPUT);
        int cb=timing_graph_add_node(tg,TNODE_COMB);
        int po=timing_graph_add_node(tg,TNODE_OUTPUT);
        timing_graph_add_edge(tg,pi,cb,1.0,0.2);
        timing_graph_add_edge(tg,cb,po,1.5,0.3);
        sta_compute_arrival(tg);
        FpgaTimingConstraints tc;
        timing_constraints_init(&tc); tc.default_period=10.0;
        assert(sta_compute_required(tg,&tc)==0);
        assert(tg->nodes[po].required_time==10.0);
        timing_graph_destroy(tg); P();
    }

    T("Slack computation");{
        FpgaTimingGraph *tg=timing_graph_create();
        int pi=timing_graph_add_node(tg,TNODE_INPUT);
        int cb=timing_graph_add_node(tg,TNODE_COMB);
        int po=timing_graph_add_node(tg,TNODE_OUTPUT);
        timing_graph_add_edge(tg,pi,cb,1.0,0.0);
        timing_graph_add_edge(tg,cb,po,1.0,0.0);
        sta_compute_arrival(tg);
        FpgaTimingConstraints tc;
        timing_constraints_init(&tc); tc.default_period=10.0;
        sta_compute_required(tg,&tc);
        assert(sta_compute_slack(tg)==0);
        assert(tg->nodes[po].slack>0);
        timing_graph_destroy(tg); P();
    }

    T("Full STA analysis");{
        FpgaTimingGraph *tg=timing_graph_create();
        int pi=timing_graph_add_node(tg,TNODE_INPUT);
        int cb=timing_graph_add_node(tg,TNODE_COMB);
        int po=timing_graph_add_node(tg,TNODE_OUTPUT);
        timing_graph_add_edge(tg,pi,cb,2.0,0.5);
        timing_graph_add_edge(tg,cb,po,3.0,0.5);
        FpgaTimingConstraints tc;
        timing_constraints_init(&tc); tc.default_period=10.0;
        FpgaStaResult r;
        assert(sta_analyze(tg,&tc,&r)==0);
        assert(r.critical_path_delay>=5.0);
        assert(r.fmax>0);
        timing_result_destroy(&r);
        timing_graph_destroy(tg); P();
    }

    T("Timing violations detected");{
        FpgaTimingGraph *tg=timing_graph_create();
        int pi=timing_graph_add_node(tg,TNODE_INPUT);
        int cb=timing_graph_add_node(tg,TNODE_COMB);
        int po=timing_graph_add_node(tg,TNODE_OUTPUT);
        timing_graph_add_edge(tg,pi,cb,8.0,3.0);
        timing_graph_add_edge(tg,cb,po,5.0,2.0);
        FpgaTimingConstraints tc;
        timing_constraints_init(&tc); tc.default_period=5.0;
        FpgaStaResult r;
        sta_analyze(tg,&tc,&r);
        assert(r.num_setup_violations>0);
        timing_result_destroy(&r);
        timing_graph_destroy(tg); P();
    }

    T("Setup/hold checks");{
        assert(sta_check_setup(5.0,10.0,0.5));
        assert(!sta_check_setup(10.5,10.0,0.0));
        assert(sta_check_hold(1.0,0.5,0.0));
        assert(!sta_check_hold(0.3,0.5,0.0));
        P();
    }

    T("Timing constraints init");{
        FpgaTimingConstraints tc;
        timing_constraints_init(&tc);
        assert(tc.default_period==10.0);
        int id=timing_add_clock(&tc,5.0,"clk_200");
        assert(id==0); assert(tc.num_clocks==1);
        assert(timing_get_clock_period(&tc,0)==5.0);
        assert(timing_get_clock_period(&tc,99)==10.0);
        P();
    }

    T("Elmore delay model");{
        FpgaRoutingPath path; path.length=2;
        double d=elmore_delay(&path,NULL,10.0,1e-14);
        assert(d>0); P();
    }

    T("CDC MTBF");{
        FpgaTimingGraph *tg=timing_graph_create();
        double mtbf=cdc_mtbf(tg,0,0);
        assert(mtbf>0);
        timing_graph_destroy(tg);
        P();
    }

    T("CDC synchronizer check");{
        FpgaTimingGraph *tg=timing_graph_create();
        assert(cdc_check_synchronizer(tg,0,1));
        assert(!cdc_check_synchronizer(tg,0,0));
        timing_graph_destroy(tg);
        P();
    }

    printf("\n=== Results: %d passed, %d failed ===\n",p,f);
    return f>0?1:0;
}
