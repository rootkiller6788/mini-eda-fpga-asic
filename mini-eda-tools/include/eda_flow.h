#ifndef EDA_FLOW_H
#define EDA_FLOW_H

#include "logic_synth.h"
#include "tech_map.h"
#include "place_route.h"
#include "static_timing.h"
#include <stdbool.h>

typedef enum {
    FLOW_SYNTH, FLOW_TECHMAP, FLOW_PLACE, FLOW_ROUTE, FLOW_STA, FLOW_DONE
} FlowStage;

typedef struct {
    FlowStage         stage;
    LogicNetwork      net;
    CellLibrary       lib;
    TechMappedDesign  mapped;
    Placement         placement;
    RouteGrid         route_grid;
    StaGraph          sta;
    bool              success;
    int               metrics[6];
    char              log[2048];
    int               log_len;
} EdaFlow;

void flow_init(EdaFlow *f);
bool flow_run_synthesis(EdaFlow *f, LogicNetwork *net);
bool flow_run_techmap(EdaFlow *f, CellLibrary *lib);
bool flow_run_place(EdaFlow *f, double chip_w, double chip_h);
bool flow_run_route(EdaFlow *f);
bool flow_run_sta(EdaFlow *f, double clock_period);
bool flow_run_all(EdaFlow *f);
void flow_print_report(EdaFlow *f);
void flow_log(EdaFlow *f, const char *msg);

#endif
