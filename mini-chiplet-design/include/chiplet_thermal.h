#ifndef CHIPLET_THERMAL_H
#define CHIPLET_THERMAL_H
#include <stdbool.h>

#define MAX_THERMAL_NODES 64

typedef struct {
    int id; char name[32]; double power_w;  /* heat source */
    double temp_c;              /* computed temperature */
    double r_to_ambient;        /* thermal resistance to ambient */
    double c_thermal;           /* thermal capacitance */
    double x, y;                /* location on chip */
} ThermalNode;

typedef struct {
    ThermalNode nodes[MAX_THERMAL_NODES]; int node_count;
    double ambient_temp_c;
    double convection_coeff;   /* W/(m^2*K) */
} ThermalModel;

void thermal_build_rc_network(ThermalModel *tm);
void thermal_solve(ThermalModel *tm);          /* steady-state temperature solve */
int  thermal_add_node(ThermalModel *tm, const char *name, double power, double x, double y, double r_amb);
int  thermal_hotspot(ThermalModel *tm);        /* returns node id of hottest point */
void thermal_set_ambient(ThermalModel *tm, double temp_c);
void thermal_report(ThermalModel *tm);
#endif
