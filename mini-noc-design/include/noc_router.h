#ifndef NOC_ROUTER_H
#define NOC_ROUTER_H
#include <stdbool.h>
#include <stdint.h>

#define MAX_PORTS 8
#define MAX_VC    4
#define VC_BUFFER_DEPTH 8

typedef struct { uint32_t flits[VC_BUFFER_DEPTH]; int head, tail; int count; } VcBuffer;

typedef struct { VcBuffer vcs[MAX_VC]; int active_vc; int credits[MAX_VC]; } InputPort;

typedef struct { int x, y; int id; InputPort in_ports[MAX_PORTS]; int out_req[MAX_PORTS][MAX_VC]; int port_count; bool busy; } Router;

typedef struct {
    int src_port, dst_port; int vc; bool granted;
} SwitchAlloc;

void router_init(Router *r, int id, int num_ports);
bool router_receive(Router *r, int port, int vc, uint32_t flit);
bool router_route(Router *r, int in_port, int vc, int dst_x, int dst_y, int *out_port);
int  router_vc_alloc(Router *r, int in_port, int out_port);
bool router_switch_traverse(Router *r, SwitchAlloc *sa);
void router_credit_return(Router *r, int port, int vc);
void router_print(Router *r);
#endif
