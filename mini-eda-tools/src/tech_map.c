#include "tech_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void cell_lib_init(CellLibrary *lib, const char *name) {
    strncpy(lib->name, name, sizeof(lib->name) - 1);
    lib->cell_count = 0;
}

int cell_lib_add(CellLibrary *lib, const char *name, GateType type,
                 double area, double delay, double power, int pin_count) {
    if (lib->cell_count >= MAX_CELLS) return -1;
    StdCell *c = &lib->cells[lib->cell_count];
    strncpy(c->name, name, sizeof(c->name) - 1);
    c->type = type;
    c->area = area;
    c->delay = delay;
    c->power = power;
    c->pin_count = pin_count;
    for (int i = 0; i < pin_count && i < MAX_CELL_PINS; i++) c->pins[i] = i;
    return lib->cell_count++;
}

StdCell *cell_lib_lookup(CellLibrary *lib, const char *name) {
    for (int i = 0; i < lib->cell_count; i++) {
        if (strcmp(lib->cells[i].name, name) == 0) return &lib->cells[i];
    }
    return NULL;
}

void cell_lib_print(CellLibrary *lib) {
    printf("Cell Library: %s (%d cells)\n", lib->name, lib->cell_count);
    for (int i = 0; i < lib->cell_count; i++) {
        StdCell *c = &lib->cells[i];
        printf("  %-12s type=%d area=%.2f delay=%.3f power=%.3f pins=%d\n",
               c->name, c->type, c->area, c->delay, c->power, c->pin_count);
    }
}

void techmap_init(TechMappedDesign *design, LogicNetwork *net, CellLibrary *lib) {
    design->net = *net;
    design->lib = *lib;
    design->match_count = 0;
    design->total_area = 0;
    design->total_delay = 0;
    design->total_power = 0;
    memset(design->matches, 0, sizeof(design->matches));
}

int techmap_match(TechMappedDesign *design) {
    int matches = 0;
    for (int g = 0; g < design->net.gate_count && matches < MAX_MATCHES; g++) {
        LogicGate *gate = &design->net.gates[g];
        for (int c = 0; c < design->lib.cell_count; c++) {
            StdCell *cell = &design->lib.cells[c];
            if (cell->type == gate->type) {
                TechMatch *m = &design->matches[matches];
                m->cell_id = c;
                m->mapped_nodes[0] = g;
                for (int p = 0; p < gate->input_count && p < MAX_CELL_PINS; p++)
                    m->mapped_nodes[p + 1] = gate->inputs[p];
                m->node_count = gate->input_count + 1;
                m->cost = cell->area;
                matches++;
            }
        }
    }
    design->match_count = matches;
    return matches;
}

int techmap_cover(TechMappedDesign *design) {
    bool covered[MAX_GATES] = {false};
    int cover_count = 0;

    for (int i = 0; i < design->match_count; i++) {
        TechMatch *m = &design->matches[i];
        int gate_id = m->mapped_nodes[0];
        if (!covered[gate_id]) {
            covered[gate_id] = true;
            StdCell *cell = &design->lib.cells[m->cell_id];
            design->total_area += cell->area;
            design->total_delay += cell->delay;
            design->total_power += cell->power;
            cover_count++;
        }
    }
    return cover_count;
}

void techmap_report(TechMappedDesign *design) {
    printf("=== Technology Mapping Report ===\n");
    printf("Total gates mapped: %d / %d\n", techmap_cover(design), design->net.gate_count);
    printf("Total area:  %.2f\n", design->total_area);
    printf("Total delay: %.3f\n", design->total_delay);
    printf("Total power: %.3f\n", design->total_power);
}

void techmap_print_mapping(TechMappedDesign *design) {
    printf("=== Gate Mapping ===\n");
    bool seen[MAX_GATES] = {false};
    for (int i = 0; i < design->match_count; i++) {
        int gid = design->matches[i].mapped_nodes[0];
        if (!seen[gid]) {
            seen[gid] = true;
            printf("  %s -> %s\n", design->net.gates[gid].name,
                   design->lib.cells[design->matches[i].cell_id].name);
        }
    }
}
