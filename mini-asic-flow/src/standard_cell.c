#include "standard_cell.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void cell_lib_init(StdCellLib *lib, const char *name, int tech_nm, double voltage) {
    strncpy(lib->name, name, sizeof(lib->name) - 1);
    lib->tech_nm = tech_nm;
    lib->voltage = voltage;
    lib->cell_count = 0;
}

int cell_lib_add_cell(StdCellLib *lib, const char *name, LibCellType type, double w, double h, double area) {
    if (lib->cell_count >= MAX_CELL_TYPES) return -1;
    CellDef *c = &lib->cells[lib->cell_count];
    strncpy(c->name, name, sizeof(c->name) - 1);
    c->type = type;
    c->width = w;
    c->height = h;
    c->area = area > 0 ? area : w * h;
    c->delay_rise = 0.02 + w * 0.01;
    c->delay_fall = 0.02 + w * 0.01;
    c->leakage_power = area * 0.001;
    c->dynamic_power = area * 0.01;
    c->pin_count = 2;
    return lib->cell_count++;
}

CellDef *cell_lib_find(StdCellLib *lib, const char *name) {
    for (int i = 0; i < lib->cell_count; i++)
        if (strcmp(lib->cells[i].name, name) == 0) return &lib->cells[i];
    return NULL;
}

CellDef *cell_lib_find_type(StdCellLib *lib, LibCellType type) {
    for (int i = 0; i < lib->cell_count; i++)
        if (lib->cells[i].type == type) return &lib->cells[i];
    return NULL;
}

void cell_lib_print(StdCellLib *lib) {
    printf("Standard Cell Library: %s (%dnm)\n", lib->name, lib->tech_nm);
    printf("%-12s %8s %8s %8s %8s %8s\n", "Name", "Width", "Height", "Area", "Delay(r)", "Leak");
    for (int i = 0; i < lib->cell_count; i++) {
        CellDef *c = &lib->cells[i];
        printf("%-12s %8.3f %8.3f %8.3f %8.3f %8.4f\n",
               c->name, c->width, c->height, c->area, c->delay_rise, c->leakage_power);
    }
}

double cell_lib_total_width(StdCellLib *lib) {
    double total = 0;
    for (int i = 0; i < lib->cell_count; i++) total += lib->cells[i].width;
    return total;
}
