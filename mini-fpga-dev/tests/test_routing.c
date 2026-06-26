/* tests/test_routing.c */
#include "routing_fabric.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static int p=0,f=0;
#define T(n) printf("  TEST: %s ... ",n)
#define P() do{printf("PASS\n");p++;}while(0)
#define F(m) do{printf("FAIL: %s\n",m);f++;}while(0)

int main(void) {
    printf("=== Test: Routing Fabric ===\n");

    T("RR graph create");{
        FpgaFabric *fb=fpga_fabric_create(3,3,4,4);
        FpgaRrGraph *g=rr_graph_create(fb);
        assert(g); assert(g->num_nodes==0);
        rr_graph_destroy(g); fpga_fabric_destroy(fb);
        P();
    }

    T("RR graph add node");{
        FpgaFabric *fb=fpga_fabric_create(3,3,4,4);
        FpgaRrGraph *g=rr_graph_create(fb);
        int n1=rr_graph_add_node(g,RR_SOURCE,0,0);
        int n2=rr_graph_add_node(g,RR_SINK,2,2);
        assert(n1==0); assert(n2==1);
        assert(g->num_nodes==2);
        rr_graph_destroy(g); fpga_fabric_destroy(fb);
        P();
    }

    T("RR graph add edge");{
        FpgaFabric *fb=fpga_fabric_create(3,3,4,4);
        FpgaRrGraph *g=rr_graph_create(fb);
        int a=rr_graph_add_node(g,RR_SOURCE,0,0);
        int b=rr_graph_add_node(g,RR_CHANX,1,0);
        int c=rr_graph_add_node(g,RR_SINK,2,0);
        rr_graph_add_edge(g,a,b); rr_graph_add_edge(g,b,c);
        assert(g->nodes[a].fan_out==1);
        assert(g->nodes[b].fan_in==1);
        assert(rr_graph_is_connected(g,a,c));
        rr_graph_destroy(g); fpga_fabric_destroy(fb);
        P();
    }

    T("Connectivity check");{
        FpgaFabric *fb=fpga_fabric_create(3,3,4,4);
        FpgaRrGraph *g=rr_graph_create(fb);
        int a=rr_graph_add_node(g,RR_SOURCE,0,0);
        int b=rr_graph_add_node(g,RR_SINK,0,0);
        assert(!rr_graph_is_connected(g,a,b));
        rr_graph_destroy(g); fpga_fabric_destroy(fb);
        P();
    }

    T("Maze BFS routing");{
        FpgaFabric *fb=fpga_fabric_create(3,3,4,4);
        FpgaRrGraph *g=rr_graph_create(fb);
        int src=rr_graph_add_node(g,RR_SOURCE,0,0);
        int ch=rr_graph_add_node(g,RR_CHANX,1,0);
        int cv=rr_graph_add_node(g,RR_CHANY,1,1);
        int snk=rr_graph_add_node(g,RR_SINK,2,0);
        rr_graph_add_edge(g,src,ch);
        rr_graph_add_edge(g,ch,cv);
        rr_graph_add_edge(g,cv,snk);
        FpgaRoutingPath path;
        int ret=route_maze_bfs(g,src,snk,&path);
        assert(ret==0); assert(path.length>0);
        rr_graph_destroy(g); fpga_fabric_destroy(fb);
        P();
    }

    T("A* routing");{
        FpgaFabric *fb=fpga_fabric_create(4,4,4,4);
        FpgaRrGraph *g=rr_graph_create(fb);
        int src=rr_graph_add_node(g,RR_SOURCE,0,0);
        int c1=rr_graph_add_node(g,RR_CHANX,1,0);
        int c2=rr_graph_add_node(g,RR_CHANY,2,1);
        int snk=rr_graph_add_node(g,RR_SINK,3,2);
        rr_graph_add_edge(g,src,c1);
        rr_graph_add_edge(g,c1,c2);
        rr_graph_add_edge(g,c2,snk);
        FpgaRoutingPath path;
        int ret=route_astar(g,src,snk,&path,false);
        assert(ret==0); assert(path.length>0);
        assert(path.total_cost>=0);
        rr_graph_destroy(g); fpga_fabric_destroy(fb);
        P();
    }

    T("Timing-driven A*");{
        FpgaFabric *fb=fpga_fabric_create(4,4,4,4);
        FpgaRrGraph *g=rr_graph_create(fb);
        int src=rr_graph_add_node(g,RR_SOURCE,0,0);
        int ch=rr_graph_add_node(g,RR_CHANX,1,0);
        int snk=rr_graph_add_node(g,RR_SINK,2,0);
        rr_graph_add_edge(g,src,ch);
        rr_graph_add_edge(g,ch,snk);
        FpgaRoutingPath path;
        int ret=route_astar(g,src,snk,&path,true);
        assert(ret==0); P();
        rr_graph_destroy(g); fpga_fabric_destroy(fb);
    }

    T("PathFinder state");{
        FpgaPathfinderState st;
        pathfinder_state_init(&st);
        assert(st.pres_fac==0.5); assert(st.max_iterations==50);
        pathfinder_state_destroy(&st);
        P();
    }

    T("PathFinder node cost");{
        FpgaRrNode n; memset(&n,0,sizeof(n)); n.base_cost=1.0;
        n.occupancy=3; n.capacity=1;
        FpgaPathfinderState st; pathfinder_state_init(&st);
        st.history=(FpgaPathfinderHist*)calloc(1,sizeof(FpgaPathfinderHist));
        st.num_hist_entries=1;
        double cost=pathfinder_node_cost(&n,&st,0);
        assert(cost>1.0); /* congested -> higher cost */
        free(st.history);
        P();
    }

    T("PathFinder update history");{
        FpgaPathfinderState st; pathfinder_state_init(&st);
        st.history=(FpgaPathfinderHist*)calloc(3,sizeof(FpgaPathfinderHist));
        st.num_hist_entries=3;
        st.history[0].times_overused=2;
        pathfinder_update_hist(&st);
        assert(st.history[0].hist_cost>0);
        assert(st.pres_fac>0.5);
        free(st.history);
        P();
    }

    T("Congestion detection");{
        FpgaFabric *fb=fpga_fabric_create(3,3,4,4);
        FpgaRrGraph *g=rr_graph_create(fb);
        int n=rr_graph_add_node(g,RR_CHANX,0,0);
        g->nodes[n].capacity=1; g->nodes[n].occupancy=0;
        assert(!pathfinder_is_congested(g));
        g->nodes[n].occupancy=2;
        assert(pathfinder_is_congested(g));
        rr_graph_destroy(g); fpga_fabric_destroy(fb);
        P();
    }

    T("Elmore delay routing path");{
        FpgaRoutingPath path;
        path.length=3; path.nodes[0]=0; path.nodes[1]=1; path.nodes[2]=2;
        FpgaRrGraph g; g.nodes=(FpgaRrNode*)calloc(3,sizeof(FpgaRrNode));
        g.nodes[0].r=10; g.nodes[0].c=1e-14;
        g.nodes[1].r=10; g.nodes[1].c=1e-14;
        g.nodes[2].r=10; g.nodes[2].c=1e-14;
        double d=route_path_delay(&path,&g,50.0);
        assert(d>0);
        free(g.nodes);
        P();
    }

    T("Redundant routing");{
        FpgaFabric *fb=fpga_fabric_create(4,4,4,4);
        FpgaRrGraph *g=rr_graph_create(fb);
        int src=rr_graph_add_node(g,RR_SOURCE,0,0);
        int c1=rr_graph_add_node(g,RR_CHANX,1,0);
        int c2=rr_graph_add_node(g,RR_CHANY,2,1);
        int snk=rr_graph_add_node(g,RR_SINK,3,2);
        rr_graph_add_edge(g,src,c1);
        rr_graph_add_edge(g,c1,c2);
        rr_graph_add_edge(g,c2,snk);
        FpgaNet net; net.source_node=src; net.sink_nodes[0]=snk; net.num_sinks=1;
        FpgaRoutingPath prim,backup;
        int ret=route_with_redundancy(g,&net,&prim,&backup);
        assert(ret==0);
        rr_graph_destroy(g); fpga_fabric_destroy(fb);
        P();
    }

    T("All nets routing");{
        FpgaFabric *fb=fpga_fabric_create(4,4,4,4);
        FpgaRrGraph *g=rr_graph_create(fb);
        int src=rr_graph_add_node(g,RR_SOURCE,0,0);
        int ch=rr_graph_add_node(g,RR_CHANX,1,0);
        int snk=rr_graph_add_node(g,RR_SINK,2,0);
        rr_graph_add_edge(g,src,ch); rr_graph_add_edge(g,ch,snk);
        FpgaNet nets[1]; nets[0].net_id=0; nets[0].source_node=src;
        nets[0].sink_nodes[0]=snk; nets[0].num_sinks=1;
        nets[0].is_routed=false;
        int r=route_all_nets(g,fb,nets,1);
        assert(r==1);
        rr_graph_destroy(g); fpga_fabric_destroy(fb);
        P();
    }

    printf("\n=== Results: %d passed, %d failed ===\n",p,f);
    return f>0?1:0;
}
