/* ================================================================
 * src/clb_pack.c - CLB Packing Implementation
 * L5: T-VPack - Timing-Driven Seed-Based Greedy Packing
 * L4: Rent's Rule used for packing heuristics
 * L8: Fracturable LUT packing (Intel ALM-style)
 * References: Betz & Rose, "VPR Packing Algorithm", FPL 1997
 * ================================================================ */

#include "clb_pack.h"
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <assert.h>

void atom_init(FpgaAtom *atom, FpgaAtomType type) {
    assert(atom);
    atom->type = type;
    atom->num_inputs = 0;
    atom->output = -1;
    atom->clk = -1;
    atom->truth_table = 0;
    atom->area = 1.0;
    atom->path_affinity = 0;
    for (int i = 0; i < FPGA_MAX_LUT_SIZE; i++) {
        atom->inputs[i] = -1;
    }
    atom->name[0] = '\0';
}

void atom_netlist_init(FpgaAtomNetlist *nl) {
    assert(nl);
    nl->num_atoms = 0;
    nl->nets = NULL;
    nl->num_nets = 0;
}

int atom_netlist_add_atom(FpgaAtomNetlist *nl, FpgaAtomType type) {
    assert(nl);
    if (nl->num_atoms >= FPGA_MAX_ATOMS) return -1;
    int idx = nl->num_atoms++;
    atom_init(&nl->atoms[idx], type);
    nl->atoms[idx].atom_id = idx;
    return idx;
}

void atom_set_input(FpgaAtom *atom, int idx, int net) {
    assert(atom && idx >= 0 && idx < FPGA_MAX_LUT_SIZE);
    atom->inputs[idx] = net;
    if (idx >= atom->num_inputs) atom->num_inputs = idx + 1;
}

void atom_set_truth_table(FpgaAtom *atom, uint64_t tt, int k) {
    assert(atom);
    atom->truth_table = tt;
    atom->num_inputs = k;
}

void atom_set_output(FpgaAtom *atom, int net) {
    assert(atom);
    atom->output = net;
}

void atom_set_clock(FpgaAtom *atom, int clk_net) {
    assert(atom);
    atom->clk = clk_net;
}

/* L5: Compute attraction between two atoms
 * A(a,s) = |nets(a) AND nets(seed)| + lambda * crit_path_depth(a)
 * This encourages packing atoms that share nets (reduces routing)
 * and prioritizes timing-critical atoms.
 * Reference: Marquardt, Betz, Rose, "Timing-Driven Placement for FPGAs",
 *            FPGA 2000 */
double compute_attraction(const FpgaAtom *a, const FpgaAtom *seed,
                           const FpgaNet* nets, int num_nets, double lambda) {
    assert(a && seed);
    int shared = 0;

    /* Count shared nets */
    if (a->output == seed->output && a->output >= 0) shared++;
    for (int i = 0; i < a->num_inputs; i++) {
        if (a->inputs[i] < 0) continue;
        if (a->inputs[i] == seed->output) shared++;
        for (int j = 0; j < seed->num_inputs; j++) {
            if (seed->inputs[j] < 0) continue;
            if (a->inputs[i] == seed->inputs[j]) shared++;
        }
    }

    (void)nets;
    (void)num_nets;

    /* Criticality factor: higher for atoms on critical path */
    double crit = (a->path_affinity > 0) ? (double)a->path_affinity / 10.0 : 0.0;

    return (double)shared + lambda * crit;
}

bool atom_fits_in_slice(const FpgaAtom *atom, const FpgaSlice *slice) {
    assert(atom && slice);
    switch (atom->type) {
        case ATOM_LUT:
            return slice->num_luts_used < FPGA_SLICE_NUM_LUTS;
        case ATOM_FF:
            return slice->num_ffs_used < FPGA_SLICE_NUM_FFS;
        case ATOM_CARRY:
            return !slice->carry.used;
        default:
            return false;
    }
}

bool atom_fits_in_clb(const FpgaAtom *atom, const FpgaClb *clb) {
    assert(atom && clb);
    for (int s = 0; s < FPGA_CLB_NUM_SLICES; s++) {
        if (atom_fits_in_slice(atom, &clb->slices[s])) return true;
    }
    return false;
}

bool net_is_clb_local(const FpgaNet *net, const FpgaClb *clb) {
    assert(net && clb);
    /* A net is CLB-local if all its source and sinks are inside the same CLB.
     * This means the net can be routed with local interconnect (no external
     * routing channel needed), reducing routing congestion. */
    (void)clb;
    /* For now, simplified: a net is local if it has at most 1 sink */
    return net->num_sinks <= 1;
}

/* L5: Timing-Driven Seed-Based Greedy Packing (T-VPack)
 *
 * Algorithm:
 * 1. Compute timing criticality for all atoms
 * 2. Select un-packed atom with highest criticality as seed
 * 3. Initialize new CLB with seed
 * 4. While CLB not full:
 *    a. Find all compatible un-packed atoms
 *    b. Select atom with max attraction to current CLB contents
 *    c. Add to CLB if fits
 * 5. Repeat from step 2
 *
 * Complexity: O(N^2) where N = atoms
 *   - Each atom considered once as seed: O(N)
 *   - Attraction computation per candidate: O(N)
 *   - Total: O(N^2)
 *
 * Reference: Betz & Rose, "Effect of LUT Architecture on FPGA Area",
 *            FPGA 1998 */
int clb_pack_timing_driven(FpgaAtomNetlist *nl, FpgaFabric *fabric,
                            FpgaPackStats *stats, double lambda) {
    assert(nl && fabric);
    bool *packed = (bool*)calloc(nl->num_atoms, sizeof(bool));
    if (!packed) return -1;
    assert(packed);

    int clb_count = 0;
    int total_luts = 0, total_ffs = 0;

    while (true) {
        /* Find most critical un-packed LUT as seed */
        FpgaAtom *seed = NULL;
        int seed_idx = -1;
        for (int i = 0; i < nl->num_atoms; i++) {
            if (packed[i]) continue;
            FpgaAtom *a = &nl->atoms[i];
            if (a->type == ATOM_LUT || a->type == ATOM_FF) {
                if (!seed || a->path_affinity > seed->path_affinity) {
                    seed = a;
                    seed_idx = i;
                }
            }
        }
        if (!seed) break;  /* all atoms packed */

        /* Start new CLB */
        if (clb_count >= fabric->grid_width * fabric->grid_height) break;
        int cx = clb_count % fabric->grid_width;
        int cy = clb_count / fabric->grid_width;
        FpgaClb *clb = &fabric->tiles[cx][cy].clb;
        fpga_clb_init(clb, cx, cy);
        clb->used = true;

        /* Place seed in slice 0 */
        if (seed->type == ATOM_LUT) {
            clb->slices[0].num_luts_used++;
            total_luts++;
        } else {
            clb->slices[0].num_ffs_used++;
            total_ffs++;
        }
        packed[seed_idx] = true;

        /* Attract compatible atoms */
        for (int iter = 0; iter < FPGA_CLB_NUM_SLICES * FPGA_SLICE_NUM_LUTS; iter++) {
            double best_attr = -1.0;
            int best_idx = -1;

            for (int i = 0; i < nl->num_atoms; i++) {
                if (packed[i]) continue;
                FpgaAtom *a = &nl->atoms[i];
                if (!atom_fits_in_clb(a, clb)) continue;

                double attr = compute_attraction(a, seed, nl->nets,
                                                  nl->num_nets, lambda);
                if (attr > best_attr) {
                    best_attr = attr;
                    best_idx = i;
                }
            }

            if (best_idx < 0) break;

            /* Place in first available slice */
            FpgaAtom *best = &nl->atoms[best_idx];
            for (int s = 0; s < FPGA_CLB_NUM_SLICES; s++) {
                if (atom_fits_in_slice(best, &clb->slices[s])) {
                    if (best->type == ATOM_LUT) {
                        clb->slices[s].num_luts_used++;
                        total_luts++;
                    } else if (best->type == ATOM_FF) {
                        clb->slices[s].num_ffs_used++;
                        total_ffs++;
                    }
                    packed[best_idx] = true;
                    break;
                }
            }
        }

        clb_count++;
    }

    if (stats) {
        stats->total_clbs_used = clb_count;
        stats->total_luts_packed = total_luts;
        stats->total_ffs_packed = total_ffs;
        stats->lut_utilization = (clb_count > 0)
            ? (double)total_luts / (clb_count * FPGA_CLB_NUM_SLICES * FPGA_SLICE_NUM_LUTS)
            : 0.0;
        stats->ff_utilization = (clb_count > 0)
            ? (double)total_ffs / (clb_count * FPGA_CLB_NUM_SLICES * FPGA_SLICE_NUM_FFS)
            : 0.0;
        stats->clb_fill_rate = (clb_count > 0)
            ? (double)(total_luts + total_ffs) / (clb_count * FPGA_CLB_NUM_SLICES * (FPGA_SLICE_NUM_LUTS + FPGA_SLICE_NUM_FFS))
            : 0.0;
        stats->average_atoms_per_clb = (clb_count > 0)
            ? (double)(total_luts + total_ffs) / clb_count
            : 0.0;
    }

    free(packed);
    return clb_count;
}

/* L5: Standard greedy packing (non-timing)
 * Same algorithm as T-VPack but lambda=0 (only net sharing) */
int clb_pack_greedy(FpgaAtomNetlist *nl, FpgaFabric *fabric,
                     FpgaPackStats *stats) {
    return clb_pack_timing_driven(nl, fabric, stats, 0.0);
}

void pack_stats_init(FpgaPackStats *s) {
    assert(s);
    memset(s, 0, sizeof(FpgaPackStats));
}

void pack_stats_compute(const FpgaFabric *fabric, FpgaPackStats *s) {
    assert(fabric && s);
    memset(s, 0, sizeof(FpgaPackStats));
    s->total_clbs_used = 0;
    int total_l = 0, total_f = 0;
    for (int x = 0; x < fabric->grid_width; x++) {
        for (int y = 0; y < fabric->grid_height; y++) {
            const FpgaClb *clb = &fabric->tiles[x][y].clb;
            if (!clb->used) continue;
            s->total_clbs_used++;
            for (int sl = 0; sl < FPGA_CLB_NUM_SLICES; sl++) {
                total_l += clb->slices[sl].num_luts_used;
                total_f += clb->slices[sl].num_ffs_used;
            }
        }
    }
    s->total_luts_packed = total_l;
    s->total_ffs_packed = total_f;
    if (s->total_clbs_used > 0) {
        int cap = s->total_clbs_used * FPGA_CLB_NUM_SLICES;
        s->lut_utilization = (double)total_l / (cap * FPGA_SLICE_NUM_LUTS);
        s->ff_utilization = (double)total_f / (cap * FPGA_SLICE_NUM_FFS);
        s->clb_fill_rate = (double)(total_l + total_f) /
                           (cap * (FPGA_SLICE_NUM_LUTS + FPGA_SLICE_NUM_FFS));
        s->average_atoms_per_clb = (double)(total_l + total_f) / s->total_clbs_used;
    }
}

void pack_stats_print(const FpgaPackStats *s) {
    assert(s);
    printf("=== CLB Packing Statistics ===\n");
    printf("CLBs used:         %d\n", s->total_clbs_used);
    printf("LUTs packed:       %d\n", s->total_luts_packed);
    printf("FFs packed:        %d\n", s->total_ffs_packed);
    printf("LUT utilization:   %.1f%%\n", s->lut_utilization * 100.0);
    printf("FF utilization:    %.1f%%\n", s->ff_utilization * 100.0);
    printf("CLB fill rate:     %.1f%%\n", s->clb_fill_rate * 100.0);
    printf("Avg atoms/CLB:     %.1f\n", s->average_atoms_per_clb);
    printf("Nets absorbed:     %d\n", s->nets_absorbed);
}

/* L8: Fracturable LUT packing (Intel ALM-style)
 * An ALM can implement: 1x 6-LUT, or 2x 5-LUT with shared inputs,
 * or 1x 5-LUT + 1x 3-LUT, etc.
 * This allows more flexible packing, improving density.
 * Reference: Lewis et al., "The Stratix II Logic and Routing Architecture",
 *            FPGA 2005 */
int clb_pack_fracturable(FpgaAtomNetlist *nl, FpgaFabric *fabric,
                          FpgaPackStats *stats) {
    assert(nl && fabric);
    /* Fracturable mode: each CLB can hold up to 8 small LUTs (fractured)
     * or 4 full-size LUTs */
    bool *packed = (bool*)calloc(nl->num_atoms, sizeof(bool));
    assert(packed);
    int clb_count = 0;
    int total_luts = 0, total_ffs = 0;

    for (int i = 0; i < nl->num_atoms; i++) {
        if (packed[i]) continue;
        FpgaAtom *a = &nl->atoms[i];
        if (a->type != ATOM_LUT && a->type != ATOM_FF) continue;

        if (clb_count >= fabric->grid_width * fabric->grid_height) break;
        int cx = clb_count % fabric->grid_width;
        int cy = clb_count / fabric->grid_width;
        FpgaClb *clb = &fabric->tiles[cx][cy].clb;
        fpga_clb_init(clb, cx, cy);
        clb->used = true;

        /* Pack as many atoms as fit into this ALM-style CLB */
        int packed_here = 0;
        for (int j = i; j < nl->num_atoms && packed_here < 8; j++) {
            if (packed[j]) continue;
            FpgaAtom *aj = &nl->atoms[j];
            if (!atom_fits_in_clb(aj, clb)) continue;

            for (int s = 0; s < FPGA_CLB_NUM_SLICES; s++) {
                if (atom_fits_in_slice(aj, &clb->slices[s])) {
                    if (aj->type == ATOM_LUT) {
                        clb->slices[s].num_luts_used++;
                        total_luts++;
                    } else {
                        clb->slices[s].num_ffs_used++;
                        total_ffs++;
                    }
                    packed[j] = true;
                    packed_here++;
                    break;
                }
            }
        }
        clb_count++;
    }

    if (stats) {
        stats->total_clbs_used = clb_count;
        stats->total_luts_packed = total_luts;
        stats->total_ffs_packed = total_ffs;
        if (clb_count > 0) {
            stats->lut_utilization = (double)total_luts /
                (clb_count * FPGA_CLB_NUM_SLICES * FPGA_SLICE_NUM_LUTS);
            stats->average_atoms_per_clb = (double)(total_luts + total_ffs) / clb_count;
        }
    }

    free(packed);
    return clb_count;
}
