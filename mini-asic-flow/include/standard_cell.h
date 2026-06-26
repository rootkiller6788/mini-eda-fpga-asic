#ifndef STANDARD_CELL_H
#define STANDARD_CELL_H

#include <stdbool.h>

#define MAX_CELL_TYPES 64
#define MAX_PINS_PER_CELL 8

typedef enum { GATE_INV, GATE_BUF, GATE_NAND2, GATE_NOR2, GATE_AND2, GATE_OR2, GATE_XOR2, GATE_MUX2, GATE_DFF, GATE_DLATCH } LibCellType;

typedef struct {
    char        name[32];
    LibCellType type;
    double      area;
    double      width, height;
    double      delay_rise;
    double      delay_fall;
    double      leakage_power;
    double      dynamic_power;
    int         pin_count;
    int         pin_names[MAX_PINS_PER_CELL];
} CellDef;

typedef struct {
    CellDef cells[MAX_CELL_TYPES];
    int     cell_count;
    char    name[64];
    double  voltage;
    int     tech_nm;
} StdCellLib;

void cell_lib_init(StdCellLib *lib, const char *name, int tech_nm, double voltage);
int  cell_lib_add_cell(StdCellLib *lib, const char *name, LibCellType type, double w, double h, double area);
CellDef *cell_lib_find(StdCellLib *lib, const char *name);
CellDef *cell_lib_find_type(StdCellLib *lib, LibCellType type);
void cell_lib_print(StdCellLib *lib);
double cell_lib_total_width(StdCellLib *lib);

#endif
