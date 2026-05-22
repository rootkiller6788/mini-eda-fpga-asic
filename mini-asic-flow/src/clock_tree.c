#include "clock_tree.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void cts_init(ClockTree *ct, double period) {
    ct->period = period;
    ct->max_skew = period * 0.1;
    ct->node_count = 0;
    ct->sink_count = 0;
    ct->buffer_count = 0;
}

int cts_add_sink(ClockTree *ct, double x, double y, double load) {
    if (ct->sink_count >= MAX_CTS_SINKS) return -1;
    CtsSink *s = &ct->sinks[ct->sink_count];
    s->x = x; s->y = y; s->cap_load = load; s->delay = 0;
    s->node_id = ct->node_count;
    CtsNode *n = &ct->nodes[ct->node_count++];
    n->id = s->node_id; n->x = x; n->y = y;
    n->is_sink = true; n->is_buffer = false; n->is_root = false;
    n->delay = 0; n->skew = 0; n->parent = -1; n->child_count = 0;
    return ct->sink_count++;
}

int cts_add_buffer_type(ClockTree *ct, const char *name, double delay, double drive, double cap) {
    if (ct->buffer_count >= MAX_CTS_BUFFERS) return -1;
    CtsBuffer *b = &ct->buffers[ct->buffer_count];
    strncpy(b->name, name, sizeof(b->name) - 1);
    b->delay = delay; b->drive_strength = drive; b->input_cap = cap;
    return ct->buffer_count++;
}

void cts_build_htree(ClockTree *ct, double cx, double cy, double size) {
    int root_id = ct->node_count;
    CtsNode *root = &ct->nodes[ct->node_count++];
    root->id = root_id; root->x = cx; root->y = cy;
    root->is_root = true; root->is_buffer = true; root->is_sink = false;
    root->delay = 0; root->skew = 0; root->parent = -1; root->child_count = 0;

    double hs = size / 2;
    int quad_positions[4][2] = {
        {(int)(cx - hs), (int)(cy - hs)},
        {(int)(cx + hs), (int)(cy - hs)},
        {(int)(cx - hs), (int)(cy + hs)},
        {(int)(cx + hs), (int)(cy + hs)}
    };
    for (int q = 0; q < 4; q++) {
        int child_id = ct->node_count;
        CtsNode *child = &ct->nodes[ct->node_count++];
        child->id = child_id; child->x = (double)quad_positions[q][0];
        child->y = (double)quad_positions[q][1];
        child->is_sink = true; child->is_buffer = false;
        child->delay = 0; child->skew = 0;
        child->parent = root_id; child->child_count = 0;
        if (root->child_count < 4) root->children[root->child_count++] = child_id;
    }
}

void cts_insert_buffers(ClockTree *ct) {
    if (ct->buffer_count < 1) {
        cts_add_buffer_type(ct, "BUF_X1", 0.05, 4.0, 0.01);
    }
    for (int i = 0; i < ct->node_count; i++) {
        if (!ct->nodes[i].is_buffer && !ct->nodes[i].is_root) {
            int buf_id = ct->node_count;
            if (buf_id >= MAX_CTS_NODES) break;
            CtsNode *buf = &ct->nodes[buf_id];
            *buf = ct->nodes[i];
            buf->id = buf_id; buf->is_buffer = true;
            buf->delay += ct->buffers[0].delay;
            ct->node_count++;
        }
    }
}

void cts_skew_analysis(ClockTree *ct) {
    for (int i = 0; i < ct->node_count; i++) {
        if (ct->nodes[i].is_sink) {
            double delay = 0;
            int cur = i;
            while (cur >= 0 && cur < ct->node_count) {
                delay += ct->nodes[cur].delay;
                cur = ct->nodes[cur].parent;
            }
            ct->nodes[i].delay = delay;
            if (i < ct->sink_count) ct->sinks[i].delay = delay;
        }
    }
    double min_delay = 1e9, max_delay = -1;
    for (int i = 0; i < ct->sink_count; i++) {
        if (ct->sinks[i].delay < min_delay) min_delay = ct->sinks[i].delay;
        if (ct->sinks[i].delay > max_delay) max_delay = ct->sinks[i].delay;
    }
    if (min_delay < 1e9) {
        for (int i = 0; i < ct->node_count; i++) {
            if (ct->nodes[i].is_sink)
                ct->nodes[i].skew = ct->nodes[i].delay - min_delay;
        }
        for (int i = 0; i < ct->sink_count; i++)
            ct->sinks[i].delay -= min_delay;
    }
}

double cts_total_skew(ClockTree *ct) { return cts_global_skew(ct); }

double cts_global_skew(ClockTree *ct) {
    double min_d = 1e9, max_d = 0;
    for (int i = 0; i < ct->sink_count; i++) {
        if (ct->sinks[i].delay < min_d) min_d = ct->sinks[i].delay;
        if (ct->sinks[i].delay > max_d) max_d = ct->sinks[i].delay;
    }
    return (min_d < 1e9) ? max_d - min_d : 0;
}

void cts_print(ClockTree *ct) {
    printf("Clock Tree: period=%.2f, %d sinks, %d nodes, %d buffers\n",
           ct->period, ct->sink_count, ct->node_count, ct->buffer_count);
    printf("Global skew: %.4f\n", cts_global_skew(ct));
    for (int i = 0; i < ct->sink_count; i++) {
        printf("  Sink %d @(%.0f,%.0f): delay=%.4f load=%.3f\n",
               i, ct->sinks[i].x, ct->sinks[i].y,
               ct->sinks[i].delay, ct->sinks[i].cap_load);
    }
}

bool cts_meets_constraints(ClockTree *ct, double max_skew) {
    return cts_global_skew(ct) <= max_skew;
}
