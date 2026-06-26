#ifndef CLB_PACK_H
#define CLB_PACK_H

#include "fpga_arch.h"
#include <stdbool.h>

/* ================================================================
 * L2/L5: CLB Packing — grouping LUTs and FFs into CLB/Slice structures
 * References: VPR packer, T-VPack, RPack algorithms
 * L4: Rent's Rule — T = A * N^p (pins vs gates relationship)
 * L5: Seed-based greedy packing with attraction functions
 * ================================================================ */

/* --- Atom (pre-pack netlist primitive) ---
 * Before packing, the netlist consists of atoms.
 * Each atom is a LUT, FF, carry chain element, or I/O
 */
typedef enum {
    ATOM_LUT,
    ATOM_FF,
    ATOM_CARRY,
    ATOM_IO,
    ATOM_DSP,
    ATOM_BRAM
} FpgaAtomType;

typedef struct {
    int         atom_id;
    FpgaAtomType type;
    int         num_inputs;
    int         inputs[FPGA_MAX_LUT_SIZE];
    int         output;
    int         clk;           /* clock net, -1 if combinational */
    uint64_t    truth_table;   /* for LUT atoms */
    double      area;          /* estimated area */
    int         path_affinity; /* timing path group */
    char        name[32];
} FpgaAtom;

#define FPGA_MAX_ATOMS  2048
#define FPGA_MAX_CLBS   1024

/* --- Atom Netlist --- */
typedef struct {
    FpgaAtom    atoms[FPGA_MAX_ATOMS];
    int         num_atoms;
    FpgaNet*    nets;
    int         num_nets;
} FpgaAtomNetlist;

/* --- Packing Candidate ---
 * Used during packing to evaluate seed attractiveness
 */
typedef struct {
    int     atom_id;
    double  attraction;       /* how much this atom wants to be with seed */
    int     shared_nets;      /* number of nets shared with seed */
    bool    fits_in_clb;     /* whether this atom can be added */
} FpgaPackCandidate;

/* --- CLB Packing Stats --- */
typedef struct {
    int     total_clbs_used;
    int     total_luts_packed;
    int     total_ffs_packed;
    double  lut_utilization;    /* luts used / total luts in used clbs */
    double  ff_utilization;
    double  clb_fill_rate;      /* packed atoms / capacity */
    int     nets_absorbed;      /* nets fully absorbed inside CLB */
    double  average_atoms_per_clb;
} FpgaPackStats;

/* L1 API */
void            atom_init(FpgaAtom *atom, FpgaAtomType type);
void            atom_netlist_init(FpgaAtomNetlist *nl);
int             atom_netlist_add_atom(FpgaAtomNetlist *nl, FpgaAtomType type);
void            atom_set_input(FpgaAtom *atom, int idx, int net);
void            atom_set_truth_table(FpgaAtom *atom, uint64_t tt, int k);
void            atom_set_output(FpgaAtom *atom, int net);
void            atom_set_clock(FpgaAtom *atom, int clk_net);

/* L5: Greedy Seed-Based Packing
 * Algorithm: T-VPack style with timing-driven seed selection.
 * 1. Select seed atom (most critical un-packed)
 * 2. Attract compatible atoms (shared nets, compatible types)
 * 3. Pack until CLB full or no candidates fit
 * Complexity: O(N * M) where N=atoms, M=CLBs */
int             clb_pack_greedy(FpgaAtomNetlist *nl, FpgaFabric *fabric,
                                FpgaPackStats *stats);

/* L5: Timing-Driven Packing
 * Similar to T-VPack: seeds selected by critical path depth
 * Attraction function: A(a,s) = |nets(a) ∩ nets(seed)| + λ * crit(a)
 * where λ balances net sharing vs criticality
 * Reference: Betz & Rose, "Effect of LUT Architecture on FPGA Area", FPGA 1998 */
int             clb_pack_timing_driven(FpgaAtomNetlist *nl, FpgaFabric *fabric,
                                        FpgaPackStats *stats, double lambda);

/* Compute attraction between two atoms */
double          compute_attraction(const FpgaAtom *a, const FpgaAtom *seed,
                                   const FpgaNet* nets, int num_nets, double lambda);

/* Check if atom fits in a partially-filled CLB */
bool            atom_fits_in_clb(const FpgaAtom *atom, const FpgaClb *clb);

/* Check if atom fits in a slice */
bool            atom_fits_in_slice(const FpgaAtom *atom, const FpgaSlice *slice);

/* Absorb net: net fully contained within a CLB (local routing) */
bool            net_is_clb_local(const FpgaNet *net, const FpgaClb *clb);

/* Statistics */
void            pack_stats_init(FpgaPackStats *s);
void            pack_stats_compute(const FpgaFabric *fabric, FpgaPackStats *s);
void            pack_stats_print(const FpgaPackStats *s);

/* L5: Depopulated CLB packing (Intel ALM-style) */
int             clb_pack_fracturable(FpgaAtomNetlist *nl, FpgaFabric *fabric,
                                      FpgaPackStats *stats);

#endif
