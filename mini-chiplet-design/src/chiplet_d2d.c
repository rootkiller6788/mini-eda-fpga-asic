#include "chiplet_d2d.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void d2d_channel_model(D2DInterface *d2d, double length_mm, double rate_gbps) {
    d2d->channel_length_mm = length_mm;
    d2d->data_rate_gbps = rate_gbps;
    /* Simplified frequency-dependent loss model */ double freq = rate_gbps / 2.0;
    d2d->insertion_loss_db = d2d_insertion_loss(d2d, freq);
    d2d->crosstalk_db = -30.0 + 3.0 * log10(rate_gbps);
    d2d->power_per_bit_pj = 0.5 + 0.1 * length_mm * rate_gbps;
}

double d2d_insertion_loss(D2DInterface *d2d, double freq_ghz) {
    /* Simple sqrt(f) loss model for microstrip */
    return -3.0 * sqrt(freq_ghz) * d2d->channel_length_mm;
}

double d2d_crosstalk(D2DInterface *d2d) { return d2d->crosstalk_db; }

double d2d_power_estimate(D2DInterface *d2d, double utilization) {
    return d2d->power_per_bit_pj * d2d->data_rate_gbps * 1e9 * utilization * 1e-12; /* Watts */
}

int d2d_place_bump(D2DInterface *d2d, double x, double y) {
    if (d2d->bump_count >= D2D_MAX_BUMPS) return -1;
    int id = d2d->bump_count++;
    d2d->bumps[id].id = id; d2d->bumps[id].x = x; d2d->bumps[id].y = y;
    d2d->bumps[id].connected = true; d2d->bumps[id].resistance = 0.05; d2d->bumps[id].capacitance = 0.1;
    return id;
}

void d2d_print(D2DInterface *d2d) {
    printf("=== D2D Interface ===\n");
    printf("  Bumps: %d, Pitch: %.1f um\n", d2d->bump_count, d2d->pitch_um);
    printf("  Channel: %.1f mm, Rate: %.1f Gbps\n", d2d->channel_length_mm, d2d->data_rate_gbps);
    printf("  IL: %.1f dB, Xtalk: %.1f dB\n", d2d->insertion_loss_db, d2d->crosstalk_db);
    printf("  Power: %.2f pJ/bit, Total: %.2f mW\n", d2d->power_per_bit_pj, d2d_power_estimate(d2d, 0.5)*1000);
}
