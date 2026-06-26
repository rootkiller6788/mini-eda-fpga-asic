#ifndef TECH_MAP_H
#define TECH_MAP_H

#include "logic_synth.h"
#include <stdbool.h>

#define MAX_CELLS         64
#define MAX_CELL_PINS     8
#define MAX_MATCHES       256
#define MAX_COVER_NODES   256

typedef struct {
    char     name[32];
    GateType type;
    double   area;
    double   delay;
    double   power;
    int      pin_count;
    int      pins[MAX_CELL_PINS];
} StdCell;

typedef struct {
    StdCell  cells[MAX_CELLS];
    int      cell_count;
    char     name[64];
} CellLibrary;

typedef struct {
    int      cell_id;
    int      mapped_nodes[MAX_CELL_PINS];
    int      node_count;
    double   cost;
} TechMatch;

typedef struct {
    LogicNetwork net;
    CellLibrary  lib;
    TechMatch    matches[MAX_MATCHES];
    int          match_count;
    double       total_area;
    double       total_delay;
    double       total_power;
} TechMappedDesign;

void cell_lib_init(CellLibrary *lib, const char *name);
int  cell_lib_add(CellLibrary *lib, const char *name, GateType type,
                  double area, double delay, double power, int pin_count);
StdCell *cell_lib_lookup(CellLibrary *lib, const char *name);
void cell_lib_print(CellLibrary *lib);

void techmap_init(TechMappedDesign *design, LogicNetwork *net, CellLibrary *lib);
int  techmap_match(TechMappedDesign *design);
int  techmap_cover(TechMappedDesign *design);
void techmap_report(TechMappedDesign *design);
void techmap_print_mapping(TechMappedDesign *design);

#endif
