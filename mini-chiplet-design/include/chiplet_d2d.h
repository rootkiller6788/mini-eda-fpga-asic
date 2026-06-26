#ifndef CHIPLET_D2D_H
#define CHIPLET_D2D_H
#include <stdint.h>
#include <stdbool.h>

#define D2D_MAX_BUMPS 1024

typedef struct { int id; double x, y; bool connected; double resistance; double capacitance; } MicroBump;

typedef struct {
    MicroBump bumps[D2D_MAX_BUMPS]; int bump_count;
    double pitch_um;         /* bump pitch in microns */
    double channel_length_mm;
    double insertion_loss_db;
    double crosstalk_db;
    double data_rate_gbps;
    double power_per_bit_pj;
} D2DInterface;

void d2d_channel_model(D2DInterface *d2d, double length_mm, double rate_gbps);
double d2d_insertion_loss(D2DInterface *d2d, double freq_ghz);
double d2d_crosstalk(D2DInterface *d2d);
double d2d_power_estimate(D2DInterface *d2d, double utilization);
int  d2d_place_bump(D2DInterface *d2d, double x, double y);
void d2d_print(D2DInterface *d2d);
#endif
