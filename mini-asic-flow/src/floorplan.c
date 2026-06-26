#include "floorplan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

static double frand(void) { return (double)rand() / RAND_MAX; }

void floorplan_init(Floorplan *fp, double w, double h) {
    fp->module_count = 0;
    fp->node_count = 0;
    fp->die_width = w;
    fp->die_height = h;
    fp->total_area = 0;
    fp->wire_estimate = 0;
    fp->cost = 0;
}

int floorplan_add_module(Floorplan *fp, const char *name, double w, double h, double area) {
    if (fp->module_count >= MAX_MODULES) return -1;
    FpModule *m = &fp->modules[fp->module_count];
    strncpy(m->name, name, sizeof(m->name) - 1);
    m->width = w; m->height = h; m->area = area;
    m->x = frand() * (fp->die_width - w);
    m->y = frand() * (fp->die_height - h);
    m->is_soft = false; m->is_fixed = false; m->is_rotated = false;
    fp->total_area += area;
    return fp->module_count++;
}

void floorplan_bstar(Floorplan *fp) {
    fp->node_count = fp->module_count;
    for (int i = 0; i < fp->module_count; i++) {
        fp->nodes[i].module_id = i;
        fp->nodes[i].is_module = true;
        fp->nodes[i].left = (i * 2 + 1 < fp->node_count) ? i * 2 + 1 : -1;
        fp->nodes[i].right = (i * 2 + 2 < fp->node_count) ? i * 2 + 2 : -1;
        fp->nodes[i].width = fp->modules[i].width;
        fp->nodes[i].height = fp->modules[i].height;
    }
}

void floorplan_evaluate(Floorplan *fp) {
    double x = 0, y = 0;
    double max_y = 0;
    for (int i = 0; i < fp->module_count; i++) {
        fp->modules[i].x = x;
        fp->modules[i].y = y;
        x += fp->modules[i].width * 0.5;
        if (fp->modules[i].height > max_y) max_y = fp->modules[i].height;
        if (x > fp->die_width) { x = 0; y += max_y; max_y = 0; }
    }
    fp->wire_estimate = 0;
    for (int i = 0; i < fp->module_count - 1; i++) {
        double dx = fabs(fp->modules[i].x - fp->modules[i + 1].x);
        double dy = fabs(fp->modules[i].y - fp->modules[i + 1].y);
        fp->wire_estimate += dx + dy;
    }
    fp->cost = fp->wire_estimate + fabs(fp->total_area - fp->die_width * fp->die_height) * 0.01;
}

void floorplan_anneal(Floorplan *fp, double init_temp, double cool_rate, int iters) {
    double temp = init_temp;
    floorplan_evaluate(fp);
    double best_cost = fp->cost;
    Floorplan best = *fp;

    for (int iter = 0; iter < iters; iter++) {
        int i = rand() % fp->module_count;
        double old_x = fp->modules[i].x, old_y = fp->modules[i].y;
        fp->modules[i].x += (frand() - 0.5) * fp->die_width * (temp / init_temp);
        fp->modules[i].y += (frand() - 0.5) * fp->die_height * (temp / init_temp);
        floorplan_evaluate(fp);
        double delta = fp->cost - best_cost;
        if (delta < 0 || frand() < exp(-delta / temp)) {
            best_cost = fp->cost;
            if (fp->cost < best_cost) best = *fp;
        } else {
            fp->modules[i].x = old_x; fp->modules[i].y = old_y;
        }
        temp *= cool_rate;
    }
    *fp = best;
}

void floorplan_print(Floorplan *fp) {
    printf("Floorplan: %.0fx%.0f, %d modules\n", fp->die_width, fp->die_height, fp->module_count);
    for (int i = 0; i < fp->module_count; i++) {
        printf("  %s: (%.1f, %.1f) %.1fx%.1f area=%.1f\n",
               fp->modules[i].name, fp->modules[i].x, fp->modules[i].y,
               fp->modules[i].width, fp->modules[i].height, fp->modules[i].area);
    }
    printf("Wire: %.1f, Cost: %.1f\n", fp->wire_estimate, fp->cost);
}

double floorplan_dead_space(Floorplan *fp) {
    double total = fp->die_width * fp->die_height;
    return total - fp->total_area;
}
