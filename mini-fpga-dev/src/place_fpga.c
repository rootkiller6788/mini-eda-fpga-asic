/* ================================================================
 * src/place_fpga.c - FPGA Placement Implementation
 * L4: Half-Perimeter Wirelength (HPWL) model
 * L5: Simulated Annealing Placement
 * L5: Quadratic Analytical Placement
 * L8: Timing-Driven Placement, Multi-FPGA Partitioning
 * References: Betz & Rose, "VPR", FPGA 1997; Kirkpatrick et al. 1983
 * ================================================================ */

#include "place_fpga.h"
#include "timing_fpga.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>

void placement_init(FpgaPlacement *p, int grid_w, int grid_h) {
    assert(p);
    memset(p, 0, sizeof(FpgaPlacement));
    p->grid_w = grid_w;
    p->grid_h = grid_h;
    p->num_blocks = 0;
    p->wirelength = 0.0;
    p->timing_cost = 0.0;
    p->cost = 0.0;
}

int placement_add_block(FpgaPlacement *p, int clb_id) {
    assert(p);
    if (p->num_blocks >= FPGA_MAX_PLACE_BLOCKS) return -1;
    int idx = p->num_blocks++;
    p->blocks[idx].clb_id = clb_id;
    p->blocks[idx].x = idx % p->grid_w;
    p->blocks[idx].y = idx / p->grid_w;
    p->blocks[idx].sub_block = 0;
    p->blocks[idx].is_fixed = false;
    p->blocks[idx].is_io = false;
    return idx;
}

void placement_fix_block(FpgaPlacement *p, int blk_idx, int x, int y) {
    assert(p && blk_idx >= 0 && blk_idx < p->num_blocks);
    p->blocks[blk_idx].x = x;
    p->blocks[blk_idx].y = y;
    p->blocks[blk_idx].is_fixed = true;
}

void placement_destroy(FpgaPlacement *p) {
    (void)p;
}

/* L4: Half-Perimeter Wirelength (HPWL) Model
 *
 * For a net connecting pins at positions (x_i, y_i):
 *   HPWL = (max(x_i) - min(x_i)) + (max(y_i) - min(y_i))
 *
 * This is the most widely used wirelength estimation in FPGA CAD
 * because it is simple, differentiable, and correlates well with
 * actual routed wirelength.
 *
 * Theorem (Hanan, 1966): On a grid with only 2-pin nets and no
 * obstacles, HPWL equals the minimum rectilinear Steiner tree length.
 *
 * For multi-pin nets: HPWL is a lower bound on Steiner tree length.
 *   HPWL <= Actual_WL <= (p/2) * HPWL, where p = number of pins. */
FpgaBoundingBox placement_net_bbox(const FpgaPlacement *p,
                                    const FpgaNet *net) {
    FpgaBoundingBox bb;
    bb.xmin = p->grid_w;
    bb.xmax = 0;
    bb.ymin = p->grid_h;
    bb.ymax = 0;

    /* Source position */
    if (net->source_node >= 0 && net->source_node < p->num_blocks) {
        int sx = p->blocks[net->source_node].x;
        int sy = p->blocks[net->source_node].y;
        if (sx < bb.xmin) bb.xmin = sx;
        if (sx > bb.xmax) bb.xmax = sx;
        if (sy < bb.ymin) bb.ymin = sy;
        if (sy > bb.ymax) bb.ymax = sy;
    }

    /* Sink positions */
    for (int i = 0; i < net->num_sinks; i++) {
        int snk = net->sink_nodes[i];
        if (snk >= 0 && snk < p->num_blocks) {
            int sx = p->blocks[snk].x;
            int sy = p->blocks[snk].y;
            if (sx < bb.xmin) bb.xmin = sx;
            if (sx > bb.xmax) bb.xmax = sx;
            if (sy < bb.ymin) bb.ymin = sy;
            if (sy > bb.ymax) bb.ymax = sy;
        }
    }
    return bb;
}

double placement_hpwl(const FpgaPlacement *p, const FpgaNet* nets,
                       int num_nets) {
    assert(p);
    double total = 0.0;
    for (int i = 0; i < num_nets; i++) {
        FpgaBoundingBox bb = placement_net_bbox(p, &nets[i]);
        total += (double)(bb.xmax - bb.xmin + bb.ymax - bb.ymin);
    }
    return total;
}

double placement_wirelength_net(const FpgaPlacement *p, const FpgaNet *net) {
    FpgaBoundingBox bb = placement_net_bbox(p, net);
    return (double)(bb.xmax - bb.xmin + bb.ymax - bb.ymin);
}

/* L5: Simulated Annealing Placement
 *
 * Inspired by metallurgical annealing: material is heated and then
 * slowly cooled to find minimum energy crystal structure.
 *
 * Cost function: C = lambda * WL + (1-lambda) * Timing_Cost
 *
 * Algorithm:
 * 1. Generate random initial placement
 * 2. T = T_start
 * 3. While T > T_end:
 *    a. For moves_per_temp iterations:
 *       - Randomly pick two blocks (a, b)
 *       - Compute delta_C = C(new) - C(old)
 *       - If delta_C < 0: accept move
 *       - Else: accept with probability exp(-delta_C / T)
 *    b. T = T * alpha
 *
 * Acceptance probability: P(accept) = min(1, exp(-delta_C / T))
 * At high T: accepts many bad moves (exploration)
 * At low T: rarely accepts bad moves (exploitation)
 *
 * Complexity: O(max_iter * moves_per_temp * N_nets)
 *
 * Reference: Kirkpatrick, Gelatt, Vecchi, "Optimization by
 *            Simulated Annealing", Science, 1983 */
int place_simulated_annealing(FpgaPlacement *p,
                               const FpgaNet* nets, int num_nets,
                               const FpgaPlaceParams *params) {
    assert(p && params);

    double T = params->T_start;
    double current_cost = placement_hpwl(p, nets, num_nets);
    int total_moves = 0;

    while (T > params->T_end && total_moves < params->max_iterations) {
        for (int m = 0; m < params->moves_per_temp; m++) {
            /* Pick two random movable blocks */
            int a = rand() % p->num_blocks;
            int b = rand() % p->num_blocks;
            if (a == b || p->blocks[a].is_fixed || p->blocks[b].is_fixed)
                continue;

            /* Evaluate swap */
            int ax = p->blocks[a].x, ay = p->blocks[a].y;
            int bx = p->blocks[b].x, by = p->blocks[b].y;

            p->blocks[a].x = bx; p->blocks[a].y = by;
            p->blocks[b].x = ax; p->blocks[b].y = ay;

            double new_cost = placement_hpwl(p, nets, num_nets);
            double delta = new_cost - current_cost;

            if (delta <= 0 || ((double)rand() / RAND_MAX) < exp(-delta / T)) {
                /* Accept */
                current_cost = new_cost;
            } else {
                /* Reject - swap back */
                p->blocks[a].x = ax; p->blocks[a].y = ay;
                p->blocks[b].x = bx; p->blocks[b].y = by;
            }
            total_moves++;
        }
        T *= params->alpha;
    }

    p->wirelength = current_cost;
    p->cost = current_cost;
    return 0;
}

void place_propose_move(FpgaPlacement *p, int *a, int *b,
                         FpgaPlaceMove *type) {
    assert(p && a && b && type);
    *a = rand() % p->num_blocks;
    *b = rand() % p->num_blocks;
    if (*a == *b) *b = (*b + 1) % p->num_blocks;
    *type = (rand() % 2 == 0) ? MOVE_SWAP : MOVE_SHIFT;
}

double place_evaluate_move(const FpgaPlacement *p, const FpgaNet* nets,
                            int num_nets, int a, int b, FpgaPlaceMove type) {
    (void)type;
    /* Quick estimate: only check nets involving a or b */
    double delta = 0.0;
    for (int i = 0; i < num_nets; i++) {
        if (nets[i].source_node == a || nets[i].source_node == b) {
            delta -= placement_wirelength_net(p, &nets[i]);
        }
    }
    /* Swap positions and re-evaluate */
    FpgaPlacement tmp = *p;
    int tx = tmp.blocks[a].x; tmp.blocks[a].x = tmp.blocks[b].x;
    int ty = tmp.blocks[a].y; tmp.blocks[a].y = tmp.blocks[b].y;
    tmp.blocks[b].x = tx; tmp.blocks[b].y = ty;

    for (int i = 0; i < num_nets; i++) {
        if (nets[i].source_node == a || nets[i].source_node == b) {
            delta += placement_wirelength_net(&tmp, &nets[i]);
        }
    }
    return delta;
}

void place_apply_move(FpgaPlacement *p, int a, int b, FpgaPlaceMove type) {
    assert(p);
    if (type == MOVE_SWAP) {
        int tx = p->blocks[a].x, ty = p->blocks[a].y;
        p->blocks[a].x = p->blocks[b].x;
        p->blocks[a].y = p->blocks[b].y;
        p->blocks[b].x = tx;
        p->blocks[b].y = ty;
    }
}

/* L5: Quadratic Analytical Placement
 *
 * Minimizes: Phi = sum_{nets} sum_{(i,j) in net} w_ij * ||pos_i - pos_j||^2
 *
 * For each dimension independently:
 *   X: minimize sum w_ij * (x_i - x_j)^2
 *   Solution: Solve sparse linear system A*x = b_x
 *
 * A[i][i] = sum_{j connected to i} w_ij
 * A[i][j] = -w_ij (for i != j)
 * b_x[i] = sum_{j fixed} w_ij * x_j (fixed pad constraints)
 *
 * For small designs, we use simple Gauss-Seidel iteration.
 * Reference: Hall, "Primer on Placement", 1970;
 *            Kleinhans et al., "GORDIAN: VLSI Placement by
 *            Quadratic Programming", TCAD 1991 */
int place_quadratic(FpgaPlacement *p, const FpgaNet* nets, int num_nets) {
    assert(p && nets);

    int N = p->num_blocks;
    double *x = (double*)malloc(N * sizeof(double));
    double *y = (double*)malloc(N * sizeof(double));
    if (!x || !y) { free(x); free(y); return -1; }

    for (int i = 0; i < N; i++) {
        x[i] = (double)p->blocks[i].x;
        y[i] = (double)p->blocks[i].y;
    }

    /* Gauss-Seidel iterative solver */
    for (int iter = 0; iter < 100; iter++) {
        double max_dx = 0.0, max_dy = 0.0;
        for (int i = 0; i < N; i++) {
            if (p->blocks[i].is_fixed) continue;

            double sum_wx = 0.0, sum_w = 0.0;
            double sum_wy = 0.0;

            for (int net = 0; net < num_nets; net++) {
                int src = nets[net].source_node;
                for (int s = 0; s < nets[net].num_sinks; s++) {
                    int snk = nets[net].sink_nodes[s];
                    if (src == i || snk == i) {
                        int other = (src == i) ? snk : src;
                        if (other < 0 || other >= N) continue;
                        double w = 1.0;
                        sum_wx += w * x[other];
                        sum_wy += w * y[other];
                        sum_w += w;
                    }
                }
            }

            if (sum_w > 0) {
                double new_x = sum_wx / sum_w;
                double new_y = sum_wy / sum_w;

                double dx = fabs(new_x - x[i]);
                double dy = fabs(new_y - y[i]);
                if (dx > max_dx) max_dx = dx;
                if (dy > max_dy) max_dy = dy;

                x[i] = new_x;
                y[i] = new_y;
            }
        }
        if (max_dx < 0.01 && max_dy < 0.01) break;
    }

    /* Legalize: snap to nearest integer grid position */
    for (int i = 0; i < N; i++) {
        int ix = (int)round(x[i]);
        int iy = (int)round(y[i]);
        if (ix < 0) ix = 0;
        if (ix >= p->grid_w) ix = p->grid_w - 1;
        if (iy < 0) iy = 0;
        if (iy >= p->grid_h) iy = p->grid_h - 1;
        p->blocks[i].x = ix;
        p->blocks[i].y = iy;
    }

    p->wirelength = placement_hpwl(p, nets, num_nets);
    p->cost = p->wirelength;

    free(x); free(y);
    return 0;
}

/* L8: Timing-Driven Placement
 * Weights nets by timing criticality:
 *   net_weight = (1 - slack/T_crit) ^ exponent
 * This makes critical nets more influential in wirelength cost,
 * pulling them closer together to reduce delay.
 * Reference: Marquardt, Betz, Rose, FPGA 2000 */
int place_timing_driven(FpgaPlacement *p, const FpgaNet* nets,
                         int num_nets, const FpgaPlaceParams *params,
                         const FpgaTimingGraph *tg) {
    assert(p && nets && params);

    /* Compute net weights based on timing */
    double *net_weights = (double*)malloc(num_nets * sizeof(double));
    assert(net_weights);

    double crit_path_delay = 10.0;  /* default if tg==NULL */
    for (int i = 0; i < num_nets; i++) {
        net_weights[i] = 1.0;
    }

    if (tg) {
        /* Find max slack for normalization */
        double max_slack = 0.0;
        for (int i = 0; i < tg->num_nodes; i++) {
            if (tg->nodes[i].slack > max_slack) max_slack = tg->nodes[i].slack;
        }
        crit_path_delay = fmax(crit_path_delay, max_slack + 1.0);

        /* Assign weights: critical nets get higher weight */
        for (int i = 0; i < num_nets; i++) {
            double crit = nets[i].timing_criticality;
            if (crit > 0.0) {
                net_weights[i] = pow(1.0 + crit, params->timing_tradeoff * 3.0);
            }
        }
    }

    /* Run simulated annealing with weighted wirelength */
    double T = params->T_start;
    double current_cost = 0.0;
    for (int i = 0; i < num_nets; i++) {
        current_cost += net_weights[i] * placement_wirelength_net(p, &nets[i]);
    }

    while (T > params->T_end) {
        for (int m = 0; m < params->moves_per_temp; m++) {
            int a = rand() % p->num_blocks;
            int b = rand() % p->num_blocks;
            if (a == b || p->blocks[a].is_fixed || p->blocks[b].is_fixed)
                continue;

            int ax = p->blocks[a].x, ay = p->blocks[a].y;
            int bx = p->blocks[b].x, by = p->blocks[b].y;

            p->blocks[a].x = bx; p->blocks[a].y = by;
            p->blocks[b].x = ax; p->blocks[b].y = ay;

            double new_cost = 0.0;
            for (int i = 0; i < num_nets; i++) {
                new_cost += net_weights[i] * placement_wirelength_net(p, &nets[i]);
            }

            double delta = new_cost - current_cost;
            if (delta <= 0 || ((double)rand() / RAND_MAX) < exp(-delta / T)) {
                current_cost = new_cost;
            } else {
                p->blocks[a].x = ax; p->blocks[a].y = ay;
                p->blocks[b].x = bx; p->blocks[b].y = by;
            }
        }
        T *= params->alpha;
    }

    p->wirelength = placement_hpwl(p, nets, num_nets);
    p->cost = current_cost;

    free(net_weights);
    return 0;
}

/* L8: Multi-FPGA partitioning placement
 * Partition design across multiple FPGAs.
 * Minimize cut nets (signals crossing FPGA boundaries). */
int place_partition_multi_fpga(FpgaPlacement *p, const FpgaNet* nets,
                                int num_nets, int num_fpgas) {
    assert(p && nets && num_fpgas >= 2);
    /* Simple round-robin partition */
    for (int i = 0; i < p->num_blocks; i++) {
        int fpga_id = i % num_fpgas;
        p->blocks[i].sub_block = fpga_id;
    }

    /* Count cut nets (nets spanning multiple FPGAs) */
    int cut_nets = 0;
    for (int i = 0; i < num_nets; i++) {
        int src_fpga = -1;
        if (nets[i].source_node >= 0 && nets[i].source_node < p->num_blocks)
            src_fpga = p->blocks[nets[i].source_node].sub_block;

        for (int s = 0; s < nets[i].num_sinks; s++) {
            int snk = nets[i].sink_nodes[s];
            if (snk >= 0 && snk < p->num_blocks) {
                if (p->blocks[snk].sub_block != src_fpga) {
                    cut_nets++;
                    break;
                }
            }
        }
    }

    /* Store cut count in cost */
    p->cost = (double)cut_nets;
    return cut_nets;
}

void placement_print_summary(const FpgaPlacement *p) {
    assert(p);
    printf("=== Placement Summary ===\n");
    printf("Grid:          %d x %d\n", p->grid_w, p->grid_h);
    printf("Blocks placed: %d\n", p->num_blocks);
    printf("Wirelength:    %.1f (HPWL)\n", p->wirelength);
    printf("Timing cost:   %.1f\n", p->timing_cost);
    printf("Total cost:    %.1f\n", p->cost);
}

bool placement_is_legal(const FpgaPlacement *p) {
    assert(p);
    /* Check no overlapping blocks at same position (excluding sub-blocks) */
    for (int i = 0; i < p->num_blocks; i++) {
        for (int j = i + 1; j < p->num_blocks; j++) {
            if (p->blocks[i].x == p->blocks[j].x &&
                p->blocks[i].y == p->blocks[j].y &&
                p->blocks[i].sub_block == p->blocks[j].sub_block) {
                return false;
            }
        }
    }
    return true;
}
