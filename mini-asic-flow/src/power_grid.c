#include "power_grid.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void pgrid_init(PowerGrid *pg, double vdd) {
    pg->vdd = vdd;
    pg->vss = 0;
    pg->max_ir_drop = vdd * 0.05;
    pg->node_count = 0;
    pg->strap_count = 0;
}

int pgrid_add_node(PowerGrid *pg, double x, double y, double current, bool is_src) {
    if (pg->node_count >= MAX_PG_NODES) return -1;
    PgNode *n = &pg->nodes[pg->node_count];
    n->id = pg->node_count; n->x = x; n->y = y;
    n->current = current; n->is_source = is_src;
    n->voltage = is_src ? pg->vdd : 0;
    n->is_connected = is_src;
    return pg->node_count++;
}

int pgrid_add_strap(PowerGrid *pg, int from, int to, double r, int layer) {
    if (pg->strap_count >= MAX_PG_STRAPS) return -1;
    PgStrap *s = &pg->straps[pg->strap_count];
    s->id = pg->strap_count; s->from = from; s->to = to;
    s->resistance = r; s->metal_layer = layer;
    s->current = 0; s->voltage_drop = 0;
    return pg->strap_count++;
}

void pgrid_analyze_ir(PowerGrid *pg) {
    for (int i = 0; i < pg->strap_count; i++) {
        PgStrap *s = &pg->straps[i];
        s->current = pg->nodes[s->to].current;
        s->voltage_drop = s->current * s->resistance;
    }
    for (int i = 0; i < pg->node_count; i++) {
        if (!pg->nodes[i].is_source) {
            double drop = 0;
            for (int j = 0; j < pg->strap_count; j++) {
                if (pg->straps[j].to == i) drop += pg->straps[j].voltage_drop;
            }
            pg->nodes[i].voltage = pg->vdd - drop;
        }
    }
}

void pgrid_report(PowerGrid *pg) {
    printf("=== Power Grid Report ===\n");
    printf("VDD=%.3fV, Max IR drop spec=%.3fV\n", pg->vdd, pg->max_ir_drop);
    printf("%-8s %8s %8s %8s\n", "Node", "X", "Y", "Voltage");
    for (int i = 0; i < pg->node_count; i++) {
        PgNode *n = &pg->nodes[i];
        printf("%-8d %8.1f %8.1f %8.4f\n", n->id, n->x, n->y, n->voltage);
    }
    printf("Max IR drop: %.4fV\n", pgrid_max_ir_drop(pg));
    printf("Meets spec: %s\n", pgrid_meets_spec(pg) ? "YES" : "NO");
}

bool pgrid_meets_spec(PowerGrid *pg) { return pgrid_max_ir_drop(pg) <= pg->max_ir_drop; }

double pgrid_max_ir_drop(PowerGrid *pg) {
    double max_drop = 0;
    for (int i = 0; i < pg->node_count; i++) {
        double drop = pg->vdd - pg->nodes[i].voltage;
        if (drop > max_drop) max_drop = drop;
    }
    return max_drop;
}
