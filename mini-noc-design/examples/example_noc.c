#include "noc_topology.h"
#include "noc_router.h"
#include "noc_routing.h"
#include "noc_flowctrl.h"
#include "noc_perf.h"
#include <stdio.h>

int main(void) {
    printf("=== NoC Design Demo ===\n\n");
    /* Create 4x4 Mesh */
    Topology topo; topo_init(&topo);
    topo_create(&topo, TOPO_MESH, 4, 4);
    topo_print(&topo);
    /* XY Routing example */
    RoutingState rs; routing_init(&rs, ROUTE_XY, 0, 0, 3, 3);
    printf("\nXY Route (0,0)->(3,3): ");
    for (int i = 0; i < 10; i++) {
        int dir = routing_xy(&rs); printf("%s ", dir==2?"E":dir==1?"S":dir==4?"L":"?");
        if (dir == 4) break;
    }
    printf("\n");
    /* Router */
    Router r; router_init(&r, 0, 5);
    router_receive(&r, 0, 0, 0x1234);
    router_print(&r);
    /* Flow control */
    FlowCtrlState fc; flowctrl_init(&fc, FLOW_CREDIT, 8);
    flowctrl_send(&fc, 0x42, false);
    flowctrl_print(&fc);
    /* Performance */
    PerfStats ps; perf_init(&ps);
    perf_inject(&ps, 0, 3, 4, 0);
    perf_deliver(&ps, 0, 5, 3);
    perf_simulate(&ps, 10, 1);
    perf_report(&ps);
    return 0;
}
