#ifndef CHIPLET_INTERPOSER_H
#define CHIPLET_INTERPOSER_H
#include <stdbool.h>
#include <stdint.h>

#define MAX_SIGNALS 256
#define INTERPOSER_LAYERS 4

typedef struct { int id; double x, y; int layer; } InterposerPoint;

typedef struct {
    InterposerPoint *points[MAX_SIGNALS]; int point_counts[MAX_SIGNALS];
    int signal_count;
    double width_mm, height_mm;
    double wire_pitch_um;
    double via_resistance;
} Interposer;

void interposer_init(Interposer *ip, double w, double h);
int  interposer_place_bumps(Interposer *ip, int die_id, int num_signals, double offset_x, double offset_y);
int  interposer_route(Interposer *ip, int sig_id, double x1, double y1, double x2, double y2);
double interposer_thermal(Interposer *ip); /* returns max temperature estimate */
double interposer_wirelength(Interposer *ip, int sig_id);
void interposer_print(Interposer *ip);
#endif
