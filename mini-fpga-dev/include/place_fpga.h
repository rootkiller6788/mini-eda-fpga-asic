#ifndef PLACE_FPGA_H
#define PLACE_FPGA_H

#include "fpga_arch.h"
#include "clb_pack.h"
#include "timing_fpga.h"
#include <stdbool.h>

/* ================================================================
 * L2/L5: FPGA Placement
 * References: VPR placement, Simulated Annealing, Quadratic Placement
 * L5: Simulated Annealing (Kirkpatrick et al., 1983)
 * L5: Analytical Placement (Quadratic wirelength minimization)
 * L8: Timing-Driven Placement (T-VPlace)
 * ================================================================ */

/* --- Placement State ---
 * Maps each CLB to a physical position on the FPGA grid.
 */
typedef struct {
    int     clb_id;
    int     x, y;           /* assigned grid position */
    int     sub_block;      /* which sub-block within tile */
    bool    is_fixed;       /* fixed position (e.g., I/O) */
    bool    is_io;
} FpgaPlaceBlock;

#define FPGA_MAX_PLACE_BLOCKS  1024

/* --- Placement Solution --- */
typedef struct {
    FpgaPlaceBlock blocks[FPGA_MAX_PLACE_BLOCKS];
    int            num_blocks;
    int            grid_w, grid_h;
    double         wirelength;      /* half-perimeter wirelength */
    double         timing_cost;
    double         cost;            /* combined cost */
} FpgaPlacement;

/* --- Simulated Annealing Parameters ---
 * T_start = initial temperature
 * T_end = final temperature
 * alpha = cooling rate
 * moves_per_temp = number of swaps per temperature step
 */
typedef struct {
    double  T_start;
    double  T_end;
    double  alpha;            /* cooling factor, typically 0.95 */
    int     moves_per_temp;
    int     max_iterations;
    double  timing_tradeoff;  /* 0=wirelength only, 1=timing only */
    int     seed;
} FpgaPlaceParams;

/* --- Move Types for Simulated Annealing ---
 */
typedef enum {
    MOVE_SWAP,           /* swap two CLB positions */
    MOVE_SHIFT,          /* move one CLB to empty slot */
    MOVE_ROTATE_REGION   /* rotate a subregion */
} FpgaPlaceMove;

/* --- Bounding Box ---
 * Used for HPWL (half-perimeter wirelength) computation
 */
typedef struct {
    int xmin, xmax, ymin, ymax;
} FpgaBoundingBox;

/* L1 API */
void          placement_init(FpgaPlacement *p, int grid_w, int grid_h);
int           placement_add_block(FpgaPlacement *p, int clb_id);
void          placement_fix_block(FpgaPlacement *p, int blk_idx, int x, int y);
void          placement_destroy(FpgaPlacement *p);

/* L4: Half-Perimeter Wirelength (HPWL)
 * HPWL(net) = (xmax - xmin) + (ymax - ymin)
 * For nets with > 2 pins: ignore internal pins, bound by extremes.
 * Theorem: HPWL is Σ of bounding box semi-perimeters,
 *   provably proportional to routing wirelength.
 *   HPWL ≤ Actual_WL ≤ (p/2) * HPWL where p = pins.
 * Reference: Cheng et al., "Interconnect Estimation for FPGAs", 2006 */
double        placement_hpwl(const FpgaPlacement *p, const FpgaNet* nets, int num_nets);
FpgaBoundingBox placement_net_bbox(const FpgaPlacement *p, const FpgaNet *net);

/* L5: Simulated Annealing Placement
 * Cost = λ * Wirelength + (1-λ) * Timing_Cost
 * Accept worse moves with P = exp(-ΔC / T)
 * Reference: Betz & Rose, "VPR: A New Packing, Placement and
 *            Routing Tool for FPGA Research", FPL 1997 */
int           place_simulated_annealing(FpgaPlacement *p,
                                        const FpgaNet* nets, int num_nets,
                                        const FpgaPlaceParams *params);

/* Propose and evaluate a random move */
void          place_propose_move(FpgaPlacement *p, int *a, int *b, FpgaPlaceMove *type);
double        place_evaluate_move(const FpgaPlacement *p, const FpgaNet* nets,
                                  int num_nets, int a, int b, FpgaPlaceMove type);

/* Apply move to placement */
void          place_apply_move(FpgaPlacement *p, int a, int b, FpgaPlaceMove type);

/* L5: Quadratic Analytical Placement
 * Minimize Σ ||x_i - x_j||² → solve Ax = b (linear system)
 * Then legalize to discrete grid positions. */
int           place_quadratic(FpgaPlacement *p, const FpgaNet* nets, int num_nets);

/* L8: Timing-Driven Placement
 * Critical path-based net weighting.
 * net_weight = (1 - slack/critical_path) ^ exponent */
int           place_timing_driven(FpgaPlacement *p, const FpgaNet* nets,
                                  int num_nets, const FpgaPlaceParams *params,
                                  const FpgaTimingGraph *tg);

/* L8: Multi-FPGA partitioning placement */
int           place_partition_multi_fpga(FpgaPlacement *p, const FpgaNet* nets,
                                          int num_nets, int num_fpgas);

/* Placement utilities */
double        placement_wirelength_net(const FpgaPlacement *p, const FpgaNet *net);
void          placement_print_summary(const FpgaPlacement *p);
bool          placement_is_legal(const FpgaPlacement *p);

#endif
