#include "eda_flow.h"
#include <stdio.h>
#include <string.h>

void flow_init(EdaFlow *f) {
    f->stage = FLOW_SYNTH;
    f->success = true;
    memset(f->metrics, 0, sizeof(f->metrics));
    f->log_len = 0;
    f->log[0] = '\0';
    network_init(&f->net, "design");
    cell_lib_init(&f->lib, "default_lib");
}

void flow_log(EdaFlow *f, const char *msg) {
    int len = (int)strlen(msg);
    if (f->log_len + len < (int)sizeof(f->log) - 2) {
        memcpy(f->log + f->log_len, msg, len);
        f->log_len += len;
        f->log[f->log_len++] = '\n';
        f->log[f->log_len] = '\0';
    }
}

bool flow_run_synthesis(EdaFlow *f, LogicNetwork *net) {
    flow_log(f, "[SYNTH] Starting logic synthesis...");
    f->net = *net;
    network_optimize(&f->net);
    network_constant_fold(&f->net);
    network_factor(&f->net);
    f->metrics[0] = f->net.gate_count;
    f->metrics[1] = f->net.wire_count;
    f->stage = FLOW_SYNTH;
    char buf[128];
    snprintf(buf, sizeof(buf), "[SYNTH] Done: %d gates, %d wires",
             f->metrics[0], f->metrics[1]);
    flow_log(f, buf);
    return true;
}

bool flow_run_techmap(EdaFlow *f, CellLibrary *lib) {
    flow_log(f, "[TECHMAP] Starting technology mapping...");
    if (lib) f->lib = *lib;
    techmap_init(&f->mapped, &f->net, &f->lib);
    int matched = techmap_match(&f->mapped);
    int covered = techmap_cover(&f->mapped);
    f->metrics[2] = matched;
    f->metrics[3] = (int)f->mapped.total_area;
    f->stage = FLOW_TECHMAP;
    char buf[128];
    snprintf(buf, sizeof(buf), "[TECHMAP] Done: %d matches, %d covered", matched, covered);
    flow_log(f, buf);
    return matched > 0;
}

bool flow_run_place(EdaFlow *f, double chip_w, double chip_h) {
    flow_log(f, "[PLACE] Starting placement...");
    place_init(&f->placement, chip_w, chip_h);
    for (int i = 0; i < f->net.gate_count; i++) {
        char bname[32];
        snprintf(bname, sizeof(bname), "%s", f->net.gates[i].name);
        place_add_block(&f->placement, bname, 2.0, 2.0);
    }
    for (int i = 0; i < f->net.gate_count; i++) {
        LogicGate *g = &f->net.gates[i];
        if (g->input_count > 0) {
            int nid = place_add_net(&f->placement, g->name, 1.0);
            place_add_pin(&f->placement, nid, i);
            for (int j = 0; j < g->input_count; j++) {
                for (int k = 0; k < i; k++) {
                    if (f->net.gates[k].output_wire == g->inputs[j]) {
                        place_add_pin(&f->placement, nid, k);
                        break;
                    }
                }
            }
        }
    }
    place_simulated_annealing(&f->placement, 1000.0, 0.95, 500);
    f->metrics[4] = (int)place_hpwl(&f->placement);
    f->stage = FLOW_PLACE;
    char buf[128];
    snprintf(buf, sizeof(buf), "[PLACE] Done: HPWL=%d", f->metrics[4]);
    flow_log(f, buf);
    return true;
}

bool flow_run_route(EdaFlow *f) {
    flow_log(f, "[ROUTE] Starting routing...");
    route_global(&f->route_grid, &f->placement);
    f->metrics[5] = route_total_wirelength(&f->route_grid);
    f->stage = FLOW_ROUTE;
    char buf[128];
    snprintf(buf, sizeof(buf), "[ROUTE] Done: wirelength=%d", f->metrics[5]);
    flow_log(f, buf);
    return true;
}

bool flow_run_sta(EdaFlow *f, double clock_period) {
    flow_log(f, "[STA] Starting timing analysis...");
    sta_init(&f->sta, clock_period, "clk");
    for (int i = 0; i < 2; i++) {
        char name[32];
        snprintf(name, sizeof(name), "node_%d", i);
        sta_add_node(&f->sta, name, 0.1, false, true);
    }
    for (int i = 0; i < f->net.gate_count; i++) {
        char name[32];
        snprintf(name, sizeof(name), "%s", f->net.gates[i].name);
        sta_add_node(&f->sta, name, 0.05, false, false);
    }
    for (int i = 0; i < f->net.gate_count - 1; i++) {
        sta_add_edge(&f->sta, i, i + 1, 0.1, 0.02, 0.0);
    }
    if (f->net.gate_count >= 2) {
        f->sta.nodes[0].is_input = true;
        f->sta.nodes[f->sta.node_count - 1].is_output = true;
    }
    sta_compute_arrival(&f->sta);
    sta_compute_required(&f->sta);
    sta_compute_slack(&f->sta);
    f->stage = FLOW_STA;
    char buf[128];
    snprintf(buf, sizeof(buf), "[STA] Done: worst_slack=%.3f, meets_timing=%s",
             sta_worst_slack(&f->sta), sta_meets_timing(&f->sta) ? "YES" : "NO");
    flow_log(f, buf);
    return sta_meets_timing(&f->sta);
}

bool flow_run_all(EdaFlow *f) {
    bool ok = true;
    ok = ok && flow_run_synthesis(f, &f->net);
    ok = ok && flow_run_techmap(f, NULL);
    ok = ok && flow_run_place(f, 100.0, 100.0);
    ok = ok && flow_run_route(f);
    ok = ok && flow_run_sta(f, 10.0);
    f->stage = FLOW_DONE;
    f->success = ok;
    return ok;
}

void flow_print_report(EdaFlow *f) {
    printf("\n========================================\n");
    printf("  EDA Flow Report\n");
    printf("========================================\n");
    printf("Status: %s\n", f->success ? "SUCCESS" : "FAILED");
    printf("\nMetrics:\n");
    printf("  Synthesis gates:  %d\n", f->metrics[0]);
    printf("  Synthesis wires:  %d\n", f->metrics[1]);
    printf("  Techmap matches:  %d\n", f->metrics[2]);
    printf("  Mapped area:      %d\n", f->metrics[3]);
    printf("  Placement HPWL:   %d\n", f->metrics[4]);
    printf("  Route wirelength: %d\n", f->metrics[5]);
    printf("\nLog:\n%s\n", f->log);
    printf("========================================\n");
}
