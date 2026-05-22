#include "chiplet_interposer.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

void interposer_init(Interposer *ip, double w, double h) {
    memset(ip, 0, sizeof(*ip));
    ip->width_mm = w; ip->height_mm = h;
    ip->wire_pitch_um = 2.0; ip->via_resistance = 0.1;
}

int interposer_place_bumps(Interposer *ip, int die_id, int num_signals, double offset_x, double offset_y) {
    (void)die_id;
    int start = ip->signal_count;
    for (int s = 0; s < num_signals && ip->signal_count < MAX_SIGNALS; s++) {
        int sid = ip->signal_count++;
        ip->points[sid] = (InterposerPoint*)malloc((size_t)(INTERPOSER_LAYERS * sizeof(InterposerPoint)));
        ip->point_counts[sid] = 0;
        if (ip->points[sid]) {
            ip->points[sid][0].x = offset_x + s * 0.04;
            ip->points[sid][0].y = offset_y;
            ip->points[sid][0].layer = 0;
            ip->point_counts[sid] = 1;
        }
    }
    return start;
}

int interposer_route(Interposer *ip, int sig_id, double x1, double y1, double x2, double y2) {
    if (sig_id >= ip->signal_count || !ip->points[sig_id]) return -1;
    int idx = ip->point_counts[sig_id];
    if (idx + 2 > INTERPOSER_LAYERS) return -1;
    ip->points[sig_id][idx].x = x1; ip->points[sig_id][idx].y = y1; ip->points[sig_id][idx].layer = 0; idx++;
    ip->points[sig_id][idx].x = x2; ip->points[sig_id][idx].y = y2; ip->points[sig_id][idx].layer = 0; idx++;
    ip->point_counts[sig_id] = idx;
    return idx;
}

double interposer_thermal(Interposer *ip) {
    /* Simple thermal estimate: 1W per 20 signals, uniform heat */
    double power = ip->signal_count * 0.05;
    double r_spread = 5.0; /* K/W spreading resistance */
    return 25.0 + power * r_spread;
}

double interposer_wirelength(Interposer *ip, int sig_id) {
    if (sig_id >= ip->signal_count || !ip->points[sig_id]) return 0;
    double len = 0;
    for (int i = 1; i < ip->point_counts[sig_id]; i++) {
        double dx = ip->points[sig_id][i].x - ip->points[sig_id][i-1].x;
        double dy = ip->points[sig_id][i].y - ip->points[sig_id][i-1].y;
        len += sqrt(dx*dx + dy*dy);
    }
    return len;
}

void interposer_print(Interposer *ip) {
    printf("=== Interposer ===\n");
    printf("  Size: %.1f x %.1f mm, Signals: %d\n", ip->width_mm, ip->height_mm, ip->signal_count);
    double total_len = 0;
    for (int i = 0; i < ip->signal_count; i++) total_len += interposer_wirelength(ip, i);
    printf("  Total wirelength: %.2f mm\n", total_len);
    printf("  Max temp: %.1f C\n", interposer_thermal(ip));
}
