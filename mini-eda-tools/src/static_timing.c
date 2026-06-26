#include "static_timing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

bool sta_create_graph(timing_graph_t *graph, const netlist_t *nl) {
    if (!graph || !nl) return false;
    memset(graph, 0, sizeof(*graph));
    graph->num_nodes = nl->num_gates;
    graph->num_arcs = 0;
    graph->nodes = (timing_node_t*)calloc(graph->num_nodes,
                                          sizeof(timing_node_t));
    graph->arcs = NULL;
    for (int i = 0; i < nl->num_gates; i++) {
        timing_node_t *tn = &graph->nodes[i];
        tn->id = i;
        strncpy(tn->name, nl->gates[i].name, sizeof(tn->name) - 1);
        tn->is_flop = (nl->gates[i].type == GATE_DFF);
        tn->is_latch = (nl->gates[i].type == GATE_DLAT);
        tn->is_port = nl->gates[i].is_pi || nl->gates[i].is_po;
        tn->is_pin = true;
        if (nl->gates[i].is_pi) graph->num_pi++;
        if (nl->gates[i].is_po) graph->num_po++;
        if (tn->is_flop || tn->is_latch) graph->num_ff++;
        tn->arrival_rise_ps = -1e12;
        tn->arrival_fall_ps = -1e12;
        tn->required_rise_ps = 1e12;
        tn->required_fall_ps = 1e12;
        tn->cap_load_ff = 1.0;
    }
    for (int i = 0; i < nl->num_gates; i++) {
        for (int j = 0; j < nl->gates[i].num_inputs; j++) {
            int src = nl->gates[i].inputs[j];
            if (src >= 0 && src < nl->num_gates) {
                sta_add_arc(graph, src, i, ARC_COMB, 20.0, 5.0);
            }
        }
    }
    return true;
}

void sta_free_graph(timing_graph_t *graph) {
    if (!graph) return;
    free(graph->nodes);
    free(graph->arcs);
    memset(graph, 0, sizeof(*graph));
}

int sta_add_node(timing_graph_t *graph, const char *name, bool is_ff) {
    if (!graph) return -1;
    graph->num_nodes++;
    graph->nodes = (timing_node_t*)realloc(graph->nodes,
                    graph->num_nodes * sizeof(timing_node_t));
    timing_node_t *n = &graph->nodes[graph->num_nodes - 1];
    memset(n, 0, sizeof(*n));
    n->id = graph->num_nodes - 1;
    strncpy(n->name, name ? name : "", sizeof(n->name) - 1);
    n->is_flop = is_ff;
    n->is_pin = true;
    if (is_ff) graph->num_ff++;
    return n->id;
}

int sta_add_arc(timing_graph_t *graph, int from, int to,
                arc_type_e type, double gate_delay, double wire_delay) {
    if (!graph) return -1;
    graph->num_arcs++;
    graph->arcs = (timing_arc_t*)realloc(graph->arcs,
                    graph->num_arcs * sizeof(timing_arc_t));
    timing_arc_t *a = &graph->arcs[graph->num_arcs - 1];
    memset(a, 0, sizeof(*a));
    a->id = graph->num_arcs - 1;
    a->from_node = from; a->to_node = to;
    a->type = type;
    a->gate_delay_ps = gate_delay;
    a->wire_delay_ps = wire_delay;
    a->total_delay_ps = gate_delay + wire_delay;
    a->slew_ps = 5.0;
    graph->nodes[from].num_arcs_out++;
    graph->nodes[to].num_arcs_in++;
    return a->id;
}

bool sta_set_node_slew(timing_graph_t *graph, int node_id, double slew) {
    if (!graph || node_id < 0 || node_id >= graph->num_nodes) return false;
    graph->nodes[node_id].input_slew_ps = slew;
    return true;
}

bool sta_set_node_load(timing_graph_t *graph, int node_id, double cap) {
    if (!graph || node_id < 0 || node_id >= graph->num_nodes) return false;
    graph->nodes[node_id].cap_load_ff = cap;
    return true;
}

bool sta_add_clock(constraints_t *con, const char *name,
                   double period_ps, double skew, double jitter) {
    if (!con || !name) return false;
    con->num_clocks++;
    con->clocks = (clock_def_t*)realloc(con->clocks,
                    con->num_clocks * sizeof(clock_def_t));
    clock_def_t *c = &con->clocks[con->num_clocks - 1];
    memset(c, 0, sizeof(*c));
    strncpy(c->name, name, sizeof(c->name) - 1);
    c->period_ps = period_ps;
    c->skew_ps = skew;
    c->jitter_ps = jitter;
    c->latency_ps = 0;
    c->transition_ps = 10.0;
    c->duty_cycle = 0.5;
    c->pulse_width_ps = period_ps * 0.5;
    return true;
}

bool sta_set_io_delay(constraints_t *con, const char *port_name,
                      bool is_input, double delay, double slew) {
    if (!con || !port_name) return false;
    con->num_io_constraints++;
    con->io_constraints = (io_constraint_t*)realloc(con->io_constraints,
                          con->num_io_constraints * sizeof(io_constraint_t));
    io_constraint_t *io = &con->io_constraints[con->num_io_constraints - 1];
    memset(io, 0, sizeof(*io));
    strncpy(io->name, port_name, sizeof(io->name) - 1);
    io->is_input = is_input;
    io->input_delay_ps = is_input ? delay : 0;
    io->output_delay_ps = is_input ? 0 : delay;
    io->slew_rise_ps = slew;
    io->slew_fall_ps = slew;
    return true;
}

double sta_calc_gate_delay(const std_cell_t *cell, double slew_in,
                           double cap_out, edge_type_e edge) {
    if (!cell) return 50.0;
    double base = (edge == RISE) ? cell->delay_rising_ps :
                                   cell->delay_falling_ps;
    double slew_factor = 1.0 + slew_in * 0.01;
    double cap_factor = 1.0 + cap_out / (cell->cap_output_ff + 0.001) * 0.5;
    return base * slew_factor * cap_factor;
}

double sta_calc_wire_delay(double length_um, double cap_per_um,
                           double res_per_um) {
    return 0.5 * res_per_um * cap_per_um * length_um * length_um;
}

double sta_calc_elmore_delay(double r, double c_tot, double c_term) {
    return r * (c_tot + c_term);
}

bool sta_run_delay_calc(timing_graph_t *graph, const delay_model_t *model) {
    if (!graph) return false;
    for (int i = 0; i < graph->num_arcs; i++) {
        timing_arc_t *a = &graph->arcs[i];
        double gate_w = sta_calc_gate_delay(NULL,
                          graph->nodes[a->from_node].input_slew_ps,
                          graph->nodes[a->to_node].cap_load_ff, RISE);
        double gate_f = sta_calc_gate_delay(NULL,
                          graph->nodes[a->from_node].input_slew_ps,
                          graph->nodes[a->to_node].cap_load_ff, FALL);
        a->gate_delay_ps = (gate_w + gate_f) * 0.5;
        a->wire_delay_ps = model ? model->wire_cap_per_um * 10.0 : 5.0;
        a->total_delay_ps = a->gate_delay_ps + a->wire_delay_ps;
    }
    return true;
}

bool sta_forward_propagate(timing_graph_t *graph, const clock_def_t *clk,
                           const timing_derate_t *derate) {
    if (!graph) return false;
    double derate_factor = derate ? derate->derate_late : 1.0;
    for (int i = 0; i < graph->num_nodes; i++) {
        if (graph->nodes[i].is_port && graph->num_pi > 0 &&
            graph->nodes[i].num_arcs_out > 0) {
            graph->nodes[i].arrival_rise_ps = 0;
            graph->nodes[i].arrival_fall_ps = 0;
        }
    }
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < graph->num_arcs; i++) {
            timing_arc_t *a = &graph->arcs[i];
            if (a->is_disabled) continue;
            timing_node_t *from = &graph->nodes[a->from_node];
            timing_node_t *to = &graph->nodes[a->to_node];
            double new_arrival_r = from->arrival_rise_ps + a->total_delay_ps *
                                   derate_factor;
            double new_arrival_f = from->arrival_fall_ps + a->total_delay_ps *
                                   derate_factor;
            if (new_arrival_r > to->arrival_rise_ps)
                to->arrival_rise_ps = new_arrival_r;
            if (new_arrival_f > to->arrival_fall_ps)
                to->arrival_fall_ps = new_arrival_f;
        }
    }
    return true;
}

bool sta_backward_propagate(timing_graph_t *graph, const clock_def_t *clk,
                            const timing_derate_t *derate) {
    if (!graph) return false;
    double period = clk ? clk->period_ps : 1000.0;
    double derate_factor = derate ? derate->derate_early : 1.0;
    for (int i = 0; i < graph->num_nodes; i++) {
        if (graph->nodes[i].is_po || (graph->nodes[i].is_flop &&
            graph->nodes[i].num_arcs_in == 1)) {
            graph->nodes[i].required_rise_ps = period;
            graph->nodes[i].required_fall_ps = period;
        }
    }
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < graph->num_arcs; i++) {
            timing_arc_t *a = &graph->arcs[i];
            if (a->is_disabled) continue;
            timing_node_t *from = &graph->nodes[a->from_node];
            timing_node_t *to = &graph->nodes[a->to_node];
            double new_req_r = to->required_rise_ps - a->total_delay_ps *
                               derate_factor;
            double new_req_f = to->required_fall_ps - a->total_delay_ps *
                               derate_factor;
            if (new_req_r < from->required_rise_ps)
                from->required_rise_ps = new_req_r;
            if (new_req_f < from->required_fall_ps)
                from->required_fall_ps = new_req_f;
        }
    }
    return true;
}

bool sta_calc_slack(timing_graph_t *graph) {
    if (!graph) return false;
    for (int i = 0; i < graph->num_nodes; i++) {
        timing_node_t *n = &graph->nodes[i];
        n->slack_rise_ps = n->required_rise_ps - n->arrival_rise_ps;
        n->slack_fall_ps = n->required_fall_ps - n->arrival_fall_ps;
    }
    return true;
}

double sta_get_slack(timing_graph_t *graph, int node_id, edge_type_e edge) {
    if (!graph || node_id < 0 || node_id >= graph->num_nodes) return -1e12;
    return (edge == RISE) ? graph->nodes[node_id].slack_rise_ps :
                            graph->nodes[node_id].slack_fall_ps;
}

bool sta_get_critical_path(timing_graph_t *graph, sta_result_t *r) {
    if (!graph || !r) return false;
    double min_slack = 1e12;
    int worst_node = -1;
    for (int i = 0; i < graph->num_nodes; i++) {
        double sl = graph->nodes[i].slack_rise_ps;
        if (sl < min_slack) {
            min_slack = sl;
            worst_node = i;
        }
    }
    memset(&r->worst_setup, 0, sizeof(r->worst_setup));
    r->worst_setup.slack_setup_ps = min_slack;
    r->worst_setup.end_node = worst_node;
    r->wns_setup = min_slack;
    return true;
}

bool sta_get_worst_paths(timing_graph_t *graph, sta_result_t *r, int num_paths) {
    if (!graph || !r) return false;
    r->num_paths = num_paths;
    r->paths = (timing_path_t*)calloc(num_paths, sizeof(timing_path_t));
    for (int p = 0; p < num_paths; p++) {
        r->paths[p].type = PATH_REG2REG;
        r->paths[p].slack_setup_ps = r->wns_setup + p * 10.0;
        r->paths[p].num_stages = 5 + p;
    }
    return true;
}

bool sta_run_analysis(timing_graph_t *graph, const constraints_t *con,
                      sta_result_t *result) {
    if (!graph || !result) return false;
    memset(result, 0, sizeof(*result));
    sta_run_delay_calc(graph, NULL);
    const clock_def_t *clk = (con && con->num_clocks > 0) ?
                              &con->clocks[0] : NULL;
    sta_forward_propagate(graph, clk, con ? &con->derate : NULL);
    sta_backward_propagate(graph, clk, con ? &con->derate : NULL);
    sta_calc_slack(graph);
    sta_get_critical_path(graph, result);
    result->num_endpoints = 0;
    result->num_violated_setup = 0;
    result->num_violated_hold = 0;
    for (int i = 0; i < graph->num_nodes; i++) {
        if (graph->nodes[i].is_flop || graph->nodes[i].is_po) {
            result->num_endpoints++;
            if (graph->nodes[i].slack_rise_ps < 0)
                result->num_violated_setup++;
        }
    }
    result->tns_setup = 0;
    for (int i = 0; i < graph->num_nodes; i++) {
        if (graph->nodes[i].slack_rise_ps < 0)
            result->tns_setup += graph->nodes[i].slack_rise_ps;
    }
    result->wns_setup = -1e12;
    for (int i = 0; i < graph->num_nodes; i++) {
        if (graph->nodes[i].slack_rise_ps > result->wns_setup)
            result->wns_setup = graph->nodes[i].slack_rise_ps;
    }
    if (result->num_violated_setup == 0) result->wns_setup = 0;
    return true;
}

bool sta_report_timing(timing_graph_t *graph, const sta_result_t *r,
                       int num_paths) {
    if (!r) return false;
    printf("STA Report: %d paths\n", num_paths);
    printf("  WNS(setup): %.2f ps\n", r->wns_setup);
    printf("  TNS(setup): %.2f ps\n", r->tns_setup);
    printf("  Endpoints:  %d\n", r->num_endpoints);
    printf("  Violated:   setup=%d hold=%d\n",
           r->num_violated_setup, r->num_violated_hold);
    return true;
}

void sta_print_path(const timing_path_t *path) {
    if (!path) return;
    printf("  Path: type=%d stages=%d delay=%.2f ps slack=%.2f ps\n",
           path->type, path->num_stages,
           path->total_delay_ps, path->slack_setup_ps);
}

void sta_print_result(const sta_result_t *r) {
    if (!r) return;
    printf("STA Result:\n");
    printf("  WNS setup: %.2f  hold: %.2f\n", r->wns_setup, r->wns_hold);
    printf("  TNS setup: %.2f  hold: %.2f\n", r->tns_setup, r->tns_hold);
    printf("  Violated setup: %d  hold: %d\n",
           r->num_violated_setup, r->num_violated_hold);
    printf("  Runtime: %.2f ms\n", r->runtime_ms);
}

void sta_init_constraints(constraints_t *con) {
    if (!con) return;
    memset(con, 0, sizeof(*con));
    con->default_input_delay = 100.0;
    con->default_output_delay = 100.0;
    con->derate.derate_late = 1.1;
    con->derate.derate_early = 0.9;
}

void sta_free_constraints(constraints_t *con) {
    if (!con) return;
    free(con->clocks);
    free(con->io_constraints);
    memset(con, 0, sizeof(*con));
}

bool sta_build_timing_graph(timing_graph_t *graph, const netlist_t *nl,
                            const constraints_t *con) {
    if (!sta_create_graph(graph, nl)) return false;
    if (con) {
        for (int i = 0; i < con->num_clocks; i++) {
            for (int j = 0; j < graph->num_nodes; j++) {
                if (graph->nodes[j].is_flop) {
                    graph->nodes[j].is_clock = true;
                }
            }
        }
    }
    return true;
}
