#include "chiplet_thermal.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int thermal_add_node(ThermalModel *tm, const char *name, double power, double x, double y, double r_amb) {
    if (tm->node_count >= MAX_THERMAL_NODES) return -1;
    ThermalNode *n = &tm->nodes[tm->node_count];
    n->id = tm->node_count; strncpy(n->name, name, 31); n->name[31] = '\0';
    n->power_w = power; n->x = x; n->y = y; n->r_to_ambient = r_amb; n->c_thermal = 1.0; n->temp_c = tm->ambient_temp_c;
    return tm->node_count++;
}

void thermal_set_ambient(ThermalModel *tm, double temp_c) { tm->ambient_temp_c = temp_c; }

void thermal_build_rc_network(ThermalModel *tm) {
    for (int i = 0; i < tm->node_count; i++) {
        double r_lateral = 2.0; /* K/W between adjacent blocks */
        for (int j = i+1; j < tm->node_count; j++) {
            double dx = tm->nodes[i].x - tm->nodes[j].x;
            double dy = tm->nodes[i].y - tm->nodes[j].y;
            double dist = sqrt(dx*dx + dy*dy);
            if (dist < 5.0) {
                /* Coupling through proximity reduces effective resistance */
                tm->nodes[i].r_to_ambient *= 0.95;
            }
        }
        (void)r_lateral;
    }
}

void thermal_solve(ThermalModel *tm) {
    thermal_build_rc_network(tm);
    for (int iter = 0; iter < 100; iter++) {
        for (int i = 0; i < tm->node_count; i++) {
            ThermalNode *n = &tm->nodes[i];
            double neighbor_heat = 0;
            for (int j = 0; j < tm->node_count; j++) {
                if (i == j) continue;
                double dx = n->x - tm->nodes[j].x;
                double dy = n->y - tm->nodes[j].y;
                double dist = sqrt(dx*dx + dy*dy) + 0.01;
                neighbor_heat += tm->nodes[j].power_w / (dist * 10.0);
            }
            n->temp_c = tm->ambient_temp_c + (n->power_w + neighbor_heat * 0.1) * n->r_to_ambient;
        }
    }
}

int thermal_hotspot(ThermalModel *tm) {
    double max_t = -999; int max_i = -1;
    for (int i = 0; i < tm->node_count; i++) if (tm->nodes[i].temp_c > max_t) { max_t = tm->nodes[i].temp_c; max_i = i; }
    return max_i;
}

void thermal_report(ThermalModel *tm) {
    printf("=== Thermal Report ===\n");
    printf("  Ambient: %.1f C, Nodes: %d\n", tm->ambient_temp_c, tm->node_count);
    for (int i = 0; i < tm->node_count; i++) {
        printf("    %s: %.2f W, %.1f C\n", tm->nodes[i].name, tm->nodes[i].power_w, tm->nodes[i].temp_c);
    }
    int hot = thermal_hotspot(tm);
    if (hot >= 0) printf("  HOTSPOT: %s at %.1f C\n", tm->nodes[hot].name, tm->nodes[hot].temp_c);
}
