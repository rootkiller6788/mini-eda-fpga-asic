#ifndef ROUTING_FABRIC_H
#define ROUTING_FABRIC_H

#include <stdbool.h>

#define MAX_TRACKS 128
#define MAX_SWITCH_BOXES 256
#define MAX_CONNECTION_BOXES 256

typedef enum { TRACK_HORIZ, TRACK_VERT } TrackDir;

typedef struct {
    int      id;
    TrackDir dir;
    int      start_x, start_y;
    int      end_x, end_y;
    int      channel;
    bool     used;
    int      net_id;
} RouteTrack;

typedef struct {
    int      id;
    int      x, y;
    int      tracks_in[MAX_TRACKS];
    int      tracks_out[MAX_TRACKS];
    int      in_count;
    int      out_count;
    int      pattern[8][8];   /* connectivity matrix */
} SwitchBox;

typedef struct {
    int      id;
    int      x, y;
    int      clb_id;
    int      tracks[MAX_TRACKS];
    int      track_count;
    int      pin_connections[16];
    int      pin_conn_count;
} ConnectionBox;

typedef struct {
    RouteTrack tracks[MAX_TRACKS];
    int        track_count;
    SwitchBox  switches[MAX_SWITCH_BOXES];
    int        switch_count;
    ConnectionBox cboxes[MAX_CONNECTION_BOXES];
    int        cbox_count;
    int        grid_w, grid_h;
    int        channel_width;
} RoutingFabric;

void fabric_init(RoutingFabric *fab, int w, int h, int channel_width);
int  fabric_add_track(RoutingFabric *fab, TrackDir dir, int sx, int sy, int ex, int ey);
int  fabric_add_switch(RoutingFabric *fab, int x, int y);
int  fabric_add_cbox(RoutingFabric *fab, int x, int y, int clb_id);
bool fabric_route_path(RoutingFabric *fab, int src_x, int src_y, int dst_x, int dst_y, int net_id);
double fabric_congestion(RoutingFabric *fab);
void fabric_print(RoutingFabric *fab);

#endif
