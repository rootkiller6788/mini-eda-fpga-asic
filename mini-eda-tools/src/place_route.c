#include "place_route.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

bool pr_create_place(place_instance_t *inst, int num_cells) {
    if (!inst) return false;
    memset(inst, 0, sizeof(*inst));
    inst->num_cells = num_cells;
    inst->cells = (place_cell_t*)calloc(num_cells, sizeof(place_cell_t));
    if (!inst->cells && num_cells > 0) return false;
    inst->nets = NULL;
    inst->num_nets = 0;
    inst->params.algo = PLACE_QUADRATIC;
    inst->params.max_iter = 100;
    inst->params.temperature = 100.0;
    inst->params.cooling_rate = 0.95;
    inst->params.wire_length_weight = 1.0;
    inst->params.density_weight = 0.5;
    inst->params.target_util = 0.7;
    return true;
}

bool pr_set_die(place_instance_t *inst, double w, double h,
                double site_w, double site_h, int layers) {
    if (!inst) return false;
    inst->die.x = 0; inst->die.y = 0;
    inst->die.width = w; inst->die.height = h;
    inst->die.site_width = site_w; inst->die.site_height = site_h;
    inst->die.num_sites_x = (int)(w / site_w);
    inst->die.num_sites_y = (int)(h / site_h);
    inst->die.num_metal_layers = layers;
    inst->die.metal_pitch = 0.1;
    return true;
}

bool pr_add_place_cell(place_instance_t *inst, int id, double w, double h,
                       bool is_macro) {
    if (!inst || id < 0 || id >= inst->num_cells) return false;
    inst->cells[id].id = id;
    inst->cells[id].width = w;
    inst->cells[id].height = h;
    inst->cells[id].area = w * h;
    inst->cells[id].is_macro = is_macro;
    return true;
}

bool pr_add_place_net(place_instance_t *inst, int src, int dst, double w) {
    if (!inst) return false;
    inst->num_nets++;
    inst->nets = (place_net_t*)realloc(inst->nets,
                    inst->num_nets * sizeof(place_net_t));
    place_net_t *n = &inst->nets[inst->num_nets - 1];
    n->id = inst->num_nets - 1;
    n->src_cell = src; n->dst_cell = dst;
    n->src_pin = 0; n->dst_pin = 0;
    n->weight = w;
    n->is_clock = false;
    return true;
}

bool pr_set_fixed_cell(place_instance_t *inst, int id, double x, double y) {
    if (!inst || id < 0 || id >= inst->num_cells) return false;
    inst->cells[id].is_fixed = true;
    inst->cells[id].x = x;
    inst->cells[id].y = y;
    return true;
}

void pr_free_place(place_instance_t *inst) {
    if (!inst) return;
    free(inst->cells);
    free(inst->nets);
    memset(inst, 0, sizeof(*inst));
}

static double place_hpwl_between(const place_cell_t *a, const place_cell_t *b) {
    return fabs(a->x - b->x) + fabs(a->y - b->y);
}

static void place_solve_quadratic(place_instance_t *inst) {
    double cx = inst->die.width / 2.0;
    double cy = inst->die.height / 2.0;
    int cols = (int)ceil(sqrt(inst->num_cells * inst->die.width /
                              inst->die.height));
    if (cols < 1) cols = 1;
    for (int i = 0; i < inst->num_cells; i++) {
        if (inst->cells[i].is_fixed) continue;
        double fx = 0, fy = 0, fw = 0;
        for (int j = 0; j < inst->num_nets; j++) {
            int s = inst->nets[j].src_cell;
            int d = inst->nets[j].dst_cell;
            if (s == i && d >= 0 && d < inst->num_cells) {
                fx += inst->cells[d].x * inst->nets[j].weight;
                fy += inst->cells[d].y * inst->nets[j].weight;
                fw += inst->nets[j].weight;
            }
            if (d == i && s >= 0 && s < inst->num_cells) {
                fx += inst->cells[s].x * inst->nets[j].weight;
                fy += inst->cells[s].y * inst->nets[j].weight;
                fw += inst->nets[j].weight;
            }
        }
        fx += cx * 0.01; fy += cy * 0.01; fw += 0.01;
        if (fw > 0) {
            inst->cells[i].x = fx / fw;
            inst->cells[i].y = fy / fw;
        } else {
            inst->cells[i].x = cx;
            inst->cells[i].y = cy;
        }
    }
}

static void place_spread(place_instance_t *inst) {
    for (int i = 0; i < inst->num_cells; i++) {
        if (inst->cells[i].x < 0) inst->cells[i].x = 0;
        if (inst->cells[i].y < 0) inst->cells[i].y = 0;
        if (inst->cells[i].x + inst->cells[i].width > inst->die.width)
            inst->cells[i].x = inst->die.width - inst->cells[i].width;
        if (inst->cells[i].y + inst->cells[i].height > inst->die.height)
            inst->cells[i].y = inst->die.height - inst->cells[i].height;
    }
}

bool pr_global_place_quadratic(place_instance_t *inst, place_result_t *r) {
    if (!inst || !r) return false;
    memset(r, 0, sizeof(*r));
    for (int iter = 0; iter < inst->params.max_iter; iter++) {
        place_solve_quadratic(inst);
        place_spread(inst);
    }
    r->total_hpwl = pr_calc_hpwl(inst);
    r->total_overflow = 0;
    r->illegal_cells = 0;
    r->num_iterations = inst->params.max_iter;
    return true;
}

bool pr_global_place_force(place_instance_t *inst, place_result_t *r) {
    if (!inst || !r) return false;
    memset(r, 0, sizeof(*r));
    for (int iter = 0; iter < inst->params.max_iter; iter++) {
        for (int i = 0; i < inst->num_cells; i++) {
            if (inst->cells[i].is_fixed) continue;
            double f_rep_x = 0, f_rep_y = 0;
            for (int j = 0; j < inst->num_cells; j++) {
                if (i == j) continue;
                double dx = inst->cells[i].x - inst->cells[j].x;
                double dy = inst->cells[i].y - inst->cells[j].y;
                double dist = sqrt(dx * dx + dy * dy) + 0.001;
                double force = 100.0 / (dist * dist);
                f_rep_x += dx / dist * force;
                f_rep_y += dy / dist * force;
            }
            double step = 10.0 / (iter + 1);
            inst->cells[i].x += f_rep_x * step;
            inst->cells[i].y += f_rep_y * step;
        }
        place_spread(inst);
    }
    r->total_hpwl = pr_calc_hpwl(inst);
    r->num_iterations = inst->params.max_iter;
    return true;
}

bool pr_global_place_sa(place_instance_t *inst, place_result_t *r) {
    if (!inst || !r) return false;
    memset(r, 0, sizeof(*r));
    double T = inst->params.temperature;
    double cr = inst->params.cooling_rate;
    double best_hpwl = pr_calc_hpwl(inst);
    for (int iter = 0; iter < inst->params.max_iter && T > 1e-6; iter++) {
        int a = rand() % inst->num_cells;
        int b = rand() % inst->num_cells;
        if (inst->cells[a].is_fixed || inst->cells[b].is_fixed) continue;
        double tx = inst->cells[a].x, ty = inst->cells[a].y;
        inst->cells[a].x = inst->cells[b].x;
        inst->cells[a].y = inst->cells[b].y;
        inst->cells[b].x = tx; inst->cells[b].y = ty;
        double new_hpwl = pr_calc_hpwl(inst);
        double delta = new_hpwl - best_hpwl;
        if (delta < 0 || (rand() / (double)RAND_MAX) < exp(-delta / T)) {
            best_hpwl = new_hpwl;
        } else {
            inst->cells[b].x = inst->cells[a].x;
            inst->cells[b].y = inst->cells[a].y;
            inst->cells[a].x = tx; inst->cells[a].y = ty;
        }
        T *= cr;
    }
    r->total_hpwl = best_hpwl;
    r->num_iterations = inst->params.max_iter;
    return true;
}

bool pr_run_global_place(place_instance_t *inst, place_result_t *r) {
    switch (inst->params.algo) {
        case PLACE_QUADRATIC: return pr_global_place_quadratic(inst, r);
        case PLACE_FORCE_DIRECTED: return pr_global_place_force(inst, r);
        case PLACE_SIMULATED_ANN: return pr_global_place_sa(inst, r);
        default: return pr_global_place_quadratic(inst, r);
    }
}

bool pr_legalize(place_instance_t *inst) {
    if (!inst) return false;
    double cur_x = 0;
    int row = 0;
    for (int i = 0; i < inst->num_cells; i++) {
        if (inst->cells[i].is_fixed) continue;
        if (cur_x + inst->cells[i].width > inst->die.width) {
            cur_x = 0;
            row++;
        }
        inst->cells[i].x = cur_x;
        inst->cells[i].y = row * inst->die.site_height * 3;
        cur_x += inst->cells[i].width + 0.2;
    }
    return true;
}

bool pr_detailed_place(place_instance_t *inst, place_result_t *r) {
    if (!inst) return false;
    bool vis[1024] = {0};
    for (int i = 0; i < inst->num_cells && i < 1024; i++) vis[i] = false;
    for (int i = 0; i < inst->num_cells; i++) {
        if (inst->cells[i].is_fixed || vis[i]) continue;
        vis[i] = true;
        double best_x = inst->cells[i].x;
        double best_y = inst->cells[i].y;
        double best_cost = 1e12;
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                double nx = inst->cells[i].x + dx * inst->die.site_width;
                double ny = inst->cells[i].y + dy * inst->die.site_height;
                if (nx >= 0 && ny >= 0 &&
                    nx + inst->cells[i].width <= inst->die.width &&
                    ny + inst->cells[i].height <= inst->die.height) {
                    double oldx = inst->cells[i].x;
                    double oldy = inst->cells[i].y;
                    inst->cells[i].x = nx; inst->cells[i].y = ny;
                    double c = pr_calc_hpwl(inst);
                    if (c < best_cost) { best_cost = c; best_x = nx; best_y = ny; }
                    inst->cells[i].x = oldx; inst->cells[i].y = oldy;
                }
            }
        }
        inst->cells[i].x = best_x;
        inst->cells[i].y = best_y;
    }
    if (r) r->total_hpwl = pr_calc_hpwl(inst);
    return true;
}

double pr_calc_hpwl(const place_instance_t *inst) {
    double hpwl = 0;
    for (int i = 0; i < inst->num_nets; i++) {
        int s = inst->nets[i].src_cell;
        int d = inst->nets[i].dst_cell;
        if (s >= 0 && s < inst->num_cells && d >= 0 && d < inst->num_cells) {
            hpwl += place_hpwl_between(&inst->cells[s], &inst->cells[d]) *
                    inst->nets[i].weight;
        }
    }
    return hpwl;
}

double pr_calc_rsmt(const place_instance_t *inst) {
    return pr_calc_hpwl(inst) * 0.85;
}

bool pr_create_congestion_map(place_instance_t *inst, congestion_map_t *cmap) {
    if (!inst || !cmap) return false;
    cmap->grid_x = inst->die.num_sites_x;
    cmap->grid_y = inst->die.num_sites_y;
    cmap->num_layers = inst->die.num_metal_layers;
    cmap->pitch_x = inst->die.metal_pitch;
    cmap->pitch_y = inst->die.metal_pitch;
    int total = cmap->grid_x * cmap->grid_y * cmap->num_layers;
    cmap->cells = (route_grid_cell_t*)calloc(total, sizeof(route_grid_cell_t));
    return cmap->cells != NULL || total == 0;
}

bool pr_update_congestion_map(congestion_map_t *cmap, route_result_t *rr) {
    if (!cmap || !rr) return false;
    for (int n = 0; n < rr->num_nets; n++) {
        for (int s = 0; s < rr->nets[n].num_segments; s++) {
            int x = rr->nets[n].seg_x[s];
            int y = rr->nets[n].seg_y[s];
            int l = rr->nets[n].seg_layer[s];
            if (x >= 0 && x < cmap->grid_x &&
                y >= 0 && y < cmap->grid_y && l >= 0 && l < cmap->num_layers) {
                int idx = (l * cmap->grid_y + y) * cmap->grid_x + x;
                cmap->cells[idx].density += 0.1;
            }
        }
    }
    return true;
}

double pr_query_congestion(const congestion_map_t *cmap, int x, int y,
                           int layer) {
    if (!cmap || x < 0 || y < 0 || layer < 0 ||
        x >= cmap->grid_x || y >= cmap->grid_y || layer >= cmap->num_layers)
        return -1;
    int idx = (layer * cmap->grid_y + y) * cmap->grid_x + x;
    return cmap->cells[idx].density;
}

void pr_print_congestion_map(const congestion_map_t *cmap) {
    if (!cmap) return;
    printf("Congestion Map: %dx%d  layers=%d\n",
           cmap->grid_x, cmap->grid_y, cmap->num_layers);
    for (int l = 0; l < cmap->num_layers; l++) {
        double max_d = 0;
        for (int y = 0; y < cmap->grid_y && y < 20; y++) {
            for (int x = 0; x < cmap->grid_x && x < 20; x++) {
                double d = pr_query_congestion(cmap, x, y, l);
                if (d > max_d) max_d = d;
            }
        }
        printf("  Layer %d: max_density=%.2f\n", l, max_d);
    }
}

void pr_free_congestion_map(congestion_map_t *cmap) {
    if (!cmap) return;
    free(cmap->cells);
    memset(cmap, 0, sizeof(*cmap));
}

static int route_maze_expand(place_instance_t *inst, route_result_t *rr,
                             int net_id, int sx, int sy, int dx, int dy) {
    int steps = 0;
    int cx = sx, cy = sy;
    int max_steps = 100;
    while ((cx != dx || cy != dy) && steps < max_steps) {
        if (cx < dx) cx++;
        else if (cx > dx) cx--;
        else if (cy < dy) cy++;
        else if (cy > dy) cy--;
        steps++;
    }
    return steps;
}

bool pr_route_maze(place_instance_t *inst, route_result_t *rr) {
    if (!inst || !rr) return false;
    memset(rr, 0, sizeof(*rr));
    rr->num_nets = inst->num_nets;
    rr->nets = (route_net_t*)calloc(inst->num_nets, sizeof(route_net_t));
    rr->num_vias = 0;
    rr->total_wirelength = 0;
    for (int i = 0; i < inst->num_nets; i++) {
        route_net_t *rn = &rr->nets[i];
        rn->id = i;
        rn->src_cell = inst->nets[i].src_cell;
        rn->dst_cell = inst->nets[i].dst_cell;
        rn->is_routed = true;
        rn->num_segments = 1;
        rn->seg_x = (int*)calloc(1, sizeof(int));
        rn->seg_y = (int*)calloc(1, sizeof(int));
        rn->seg_layer = (int*)calloc(1, sizeof(int));
        int sx = (int)(inst->cells[inst->nets[i].src_cell].x /
                       inst->die.site_width);
        int sy = (int)(inst->cells[inst->nets[i].src_cell].y /
                       inst->die.site_height);
        int dx = (int)(inst->cells[inst->nets[i].dst_cell].x /
                       inst->die.site_width);
        int dy = (int)(inst->cells[inst->nets[i].dst_cell].y /
                       inst->die.site_height);
        rn->seg_x[0] = (sx + dx) / 2;
        rn->seg_y[0] = (sy + dy) / 2;
        rn->seg_layer[0] = 0;
        rn->wirelength_um = route_maze_expand(inst, rr, i, sx, sy, dx, dy) *
                            inst->die.metal_pitch;
        rr->total_wirelength += rn->wirelength_um;
        rn->num_vias = 1;
        rr->num_vias++;
    }
    return true;
}

bool pr_route_astar(place_instance_t *inst, route_result_t *rr) {
    return pr_route_maze(inst, rr);
}

bool pr_run_global_route(place_instance_t *inst, route_result_t *rr) {
    return pr_route_maze(inst, rr);
}

bool pr_run_detail_route(place_instance_t *inst, route_result_t *rr) {
    if (!inst || !rr) return false;
    for (int i = 0; i < rr->num_nets; i++) {
        rr->nets[i].num_vias += 1;
    }
    return true;
}

bool pr_ripup_reroute(place_instance_t *inst, route_result_t *rr, int max_iter) {
    for (int iter = 0; iter < max_iter; iter++) {
        for (int i = 0; i < rr->num_nets; i++) {
            free(rr->nets[i].seg_x);
            free(rr->nets[i].seg_y);
            free(rr->nets[i].seg_layer);
            rr->nets[i].seg_x = NULL;
            rr->nets[i].seg_y = NULL;
            rr->nets[i].seg_layer = NULL;
        }
        pr_route_maze(inst, rr);
    }
    return true;
}

int pr_count_drc(const route_result_t *rr) {
    if (!rr) return 0;
    int drc = 0;
    for (int i = 0; i < rr->num_nets; i++) {
        if (rr->nets[i].num_segments < 1) drc++;
    }
    return drc;
}

double pr_total_wirelength(const route_result_t *rr) {
    return rr ? rr->total_wirelength : 0;
}

int pr_total_vias(const route_result_t *rr) {
    return rr ? rr->num_vias : 0;
}

bool pr_export_def(const place_instance_t *inst, const char *file) {
    if (!inst || !file) return false;
    FILE *f = fopen(file, "w");
    if (!f) return false;
    fprintf(f, "VERSION 5.7 ;\n");
    fprintf(f, "DIEAREA ( 0 0 ) ( %.0f %.0f ) ;\n",
            inst->die.width * 1000, inst->die.height * 1000);
    for (int i = 0; i < inst->num_cells; i++) {
        fprintf(f, "COMPONENTS %d ;\n", i + 1);
    }
    fprintf(f, "END COMPONENTS\n");
    fprintf(f, "NETS %d ;\n", inst->num_nets);
    fprintf(f, "END NETS\n");
    fprintf(f, "END DESIGN\n");
    fclose(f);
    return true;
}

bool pr_export_lef(const die_area_t *die, const char *file) {
    if (!die || !file) return false;
    FILE *f = fopen(file, "w");
    if (!f) return false;
    fprintf(f, "VERSION 5.7 ;\n");
    fprintf(f, "SITE site WIDTH %.3f HEIGHT %.3f ;\n",
            die->site_width, die->site_height);
    fprintf(f, "END LIBRARY\n");
    fclose(f);
    return true;
}

void pr_print_place_result(const place_result_t *r) {
    if (!r) return;
    printf("Placement Result:\n");
    printf("  HPWL:          %.2f um\n", r->total_hpwl);
    printf("  Overflow:      %.2f\n", r->total_overflow);
    printf("  Illegal cells: %d\n", r->illegal_cells);
    printf("  Iterations:    %d\n", r->num_iterations);
}

void pr_print_route_result(const route_result_t *r) {
    if (!r) return;
    printf("Route Result:\n");
    printf("  Nets:         %d\n", r->num_nets);
    printf("  Wirelength:   %.2f um\n", r->total_wirelength);
    printf("  Vias:         %d\n", r->num_vias);
    printf("  DRC:          %d\n", r->num_drc);
}

bool pr_route_single_net(place_instance_t *inst, int net_id,
                         route_result_t *rr) {
    if (!inst || !rr || net_id < 0 || net_id >= inst->num_nets) return false;
    route_net_t *rn = &rr->nets[net_id];
    rn->id = net_id;
    rn->src_cell = inst->nets[net_id].src_cell;
    rn->dst_cell = inst->nets[net_id].dst_cell;
    rn->is_routed = true;
    rn->num_segments = 2;
    rn->seg_x = (int*)calloc(2, sizeof(int));
    rn->seg_y = (int*)calloc(2, sizeof(int));
    rn->seg_layer = (int*)calloc(2, sizeof(int));
    rn->seg_x[0] = 0; rn->seg_y[0] = 0; rn->seg_layer[0] = 0;
    rn->seg_x[1] = 1; rn->seg_y[1] = 1; rn->seg_layer[1] = 0;
    rn->wirelength_um = 10.0;
    rr->total_wirelength += rn->wirelength_um;
    return true;
}

bool pr_check_antenna(const route_result_t *rr, int net_id) {
    if (!rr || net_id < 0 || net_id >= rr->num_nets) return false;
    return rr->nets[net_id].wirelength_um < 100.0;
}

bool pr_add_redundant_via(route_result_t *rr, int net_id) {
    if (!rr || net_id < 0 || net_id >= rr->num_nets) return false;
    rr->nets[net_id].num_vias++;
    rr->num_vias++;
    return true;
}
