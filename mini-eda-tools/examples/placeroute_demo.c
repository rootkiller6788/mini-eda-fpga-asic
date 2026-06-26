/**
 * mini-eda-tools Place & Route Demo
 * 演示模拟退火布局和迷宫布线
 */
#include "place_route.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    printf("====== Place & Route Demo ======\n\n");
    srand((unsigned)time(NULL));

    printf("--- Placement ---\n");
    Placement p;
    place_init(&p, 100.0, 100.0);
    place_add_block(&p, "u1", 5.0, 5.0);
    place_add_block(&p, "u2", 5.0, 5.0);
    place_add_block(&p, "u3", 5.0, 5.0);
    place_add_block(&p, "u4", 5.0, 5.0);

    int n1 = place_add_net(&p, "net1", 1.0);
    place_add_pin(&p, n1, 0); place_add_pin(&p, n1, 1);
    int n2 = place_add_net(&p, "net2", 1.5);
    place_add_pin(&p, n2, 1); place_add_pin(&p, n2, 2);
    int n3 = place_add_net(&p, "net3", 1.0);
    place_add_pin(&p, n3, 2); place_add_pin(&p, n3, 3);

    printf("Initial HPWL: %.2f\n", place_hpwl(&p));
    printf("Initial cost: %.2f\n", place_total_cost(&p));

    place_simulated_annealing(&p, 1000.0, 0.95, 1000);
    printf("After SA:\n");
    printf("Final HPWL: %.2f\n", place_hpwl(&p));
    printf("Final cost: %.2f\n", place_total_cost(&p));
    place_print(&p);

    printf("\n--- Routing ---\n");
    RouteGrid g;
    route_global(&g, &p);
    printf("Total wirelength: %d\n", route_total_wirelength(&g));
    route_print(&g);

    return 0;
}
