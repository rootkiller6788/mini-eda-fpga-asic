#include "routing_fabric.h"
#include <stdio.h>
#include <string.h>

void fabric_init(RoutingFabric *fab, int w, int h, int channel_width) {
    fab->grid_w = w;
    fab->grid_h = h;
    fab->channel_width = channel_width;
    fab->track_count = 0;
    fab->switch_count = 0;
    fab->cbox_count = 0;
    memset(fab->tracks, 0, sizeof(fab->tracks));
    memset(fab->switches, 0, sizeof(fab->switches));
    memset(fab->cboxes, 0, sizeof(fab->cboxes));

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            fabric_add_switch(fab, x, y);
            fabric_add_cbox(fab, x, y, y * w + x);
        }
    }
}

int fabric_add_track(RoutingFabric *fab, TrackDir dir, int sx, int sy, int ex, int ey) {
    if (fab->track_count >= MAX_TRACKS) return -1;
    RouteTrack *t = &fab->tracks[fab->track_count];
    t->id = fab->track_count;
    t->dir = dir;
    t->start_x = sx; t->start_y = sy;
    t->end_x = ex; t->end_y = ey;
    t->used = false;
    t->net_id = 0;
    return fab->track_count++;
}

int fabric_add_switch(RoutingFabric *fab, int x, int y) {
    if (fab->switch_count >= MAX_SWITCH_BOXES) return -1;
    SwitchBox *s = &fab->switches[fab->switch_count];
    s->id = fab->switch_count;
    s->x = x; s->y = y;
    s->in_count = 0; s->out_count = 0;
    memset(s->pattern, 0, sizeof(s->pattern));
    return fab->switch_count++;
}

int fabric_add_cbox(RoutingFabric *fab, int x, int y, int clb_id) {
    if (fab->cbox_count >= MAX_CONNECTION_BOXES) return -1;
    ConnectionBox *c = &fab->cboxes[fab->cbox_count];
    c->id = fab->cbox_count;
    c->x = x; c->y = y;
    c->clb_id = clb_id;
    c->track_count = 0;
    c->pin_conn_count = 0;
    return fab->cbox_count++;
}

bool fabric_route_path(RoutingFabric *fab, int src_x, int src_y, int dst_x, int dst_y, int net_id) {
    int dx = dst_x - src_x;
    int dy = dst_y - src_y;
    int path_len = 0;

    int cx = src_x, cy = src_y;
    while (cy != dst_y && path_len < 10) {
        int ny = cy + (dy > 0 ? 1 : -1);
        int tid = fabric_add_track(fab, TRACK_VERT, cx, cy, cx, ny);
        if (tid >= 0) { fab->tracks[tid].used = true; fab->tracks[tid].net_id = net_id; }
        cy = ny; path_len++;
    }
    while (cx != dst_x && path_len < 10) {
        int nx = cx + (dx > 0 ? 1 : -1);
        int tid = fabric_add_track(fab, TRACK_HORIZ, cx, cy, nx, cy);
        if (tid >= 0) { fab->tracks[tid].used = true; fab->tracks[tid].net_id = net_id; }
        cx = nx; path_len++;
    }
    return path_len > 0;
}

double fabric_congestion(RoutingFabric *fab) {
    int used = 0;
    for (int i = 0; i < fab->track_count; i++)
        if (fab->tracks[i].used) used++;
    if (fab->track_count == 0) return 0;
    return (double)used / (double)fab->track_count;
}

void fabric_print(RoutingFabric *fab) {
    printf("Routing Fabric: %dx%d, %d tracks, %d switches, %d cboxes\n",
           fab->grid_w, fab->grid_h, fab->track_count, fab->switch_count, fab->cbox_count);
    printf("Congestion: %.2f\n", fabric_congestion(fab));
}
