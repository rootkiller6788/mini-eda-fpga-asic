#ifndef TECHMAP_FPGA_H
#define TECHMAP_FPGA_H

#include "fpga_arch.h"
#include "clb_pack.h"
#include <stdbool.h>

/* ================================================================
 * L2/L5: FPGA Technology Mapping
 * Reference: FlowMap (Cong & Ding, TCAD 1994), DAGMap, CutMap
 * L4: Shannon Expansion — LUT implements any k-variable function
 * L5: FlowMap Algorithm — polynomial-time optimal depth mapping for K-LUT
 * L5: Cut Enumeration — enumerate K-feasible cuts for each node
 * ================================================================ */

/* --- Boolean Network Node ---
 * Represents a gate in the technology-independent network
 */
typedef enum {
    GATE_AND, GATE_OR, GATE_NOT, GATE_XOR,
    GATE_NAND, GATE_NOR, GATE_XNOR,
    GATE_BUF, GATE_MUX
} FpgaGateType;

typedef struct {
    int           node_id;
    FpgaGateType  type;
    int           inputs[4];     /* up to 4 inputs */
    int           num_inputs;
    int           output;
    bool          is_primary_input;
    bool          is_primary_output;
    int           level;         /* topological level from PIs */
    int           ref_count;     /* fanout count */
} FpgaBoolNode;

#define FPGA_MAX_BOOL_NODES  1024

typedef struct {
    FpgaBoolNode  nodes[FPGA_MAX_BOOL_NODES];
    int           num_nodes;
    int           num_pi;
    int           num_po;
} FpgaBoolNetwork;

/* --- K-feasible Cut ---
 * A cut represents a LUT implementing the subgraph rooted at root.
 * input_set contains the cut leaves (fanins to the LUT).
 */
#define FPGA_MAX_CUT_SIZE  6

typedef struct {
    int   root;                          /* root node of this cut */
    int   input_set[FPGA_MAX_CUT_SIZE];  /* cut inputs */
    int   num_inputs;                     /* ≤ K */
    int   subgraph_nodes[FPGA_MAX_CUT_SIZE * 4];
    int   num_sub_nodes;
    int   depth;                          /* depth of best mapping using this cut */
    double area_flow;                     /* area estimation */
    double delay;                         /* delay estimation */
} FpgaCut;

#define FPGA_MAX_CUTS_PER_NODE  32

/* --- Mapping Solution ---
 * Maps each node to a specific LUT
 */
typedef struct {
    int      node_id;
    int      lut_id;           /* assigned LUT index */
    int      cut_id;           /* which cut was selected */
    int      inputs[FPGA_MAX_CUT_SIZE];
    int      num_inputs;
    uint64_t truth_table;      /* computed truth table */
    int      depth;
    int      clb_id;           /* assigned CLB after packing */
    int      slice_id;         /* assigned slice after packing */
} FpgaMappingEntry;

/* --- LUT Mapping Result ---
 */
typedef struct {
    FpgaMappingEntry* entries;
    int               num_entries;
    int               max_depth;        /* maximum logic depth */
    int               total_luts;
    int               total_nets;
    double            total_area;
    double            total_delay;
} FpgaLutMapping;

/* L1 API: Boolean network */
void      bool_network_init(FpgaBoolNetwork *bn);
int       bool_network_add_node(FpgaBoolNetwork *bn, FpgaGateType type);
void      bool_network_add_edge(FpgaBoolNetwork *bn, int from, int to, int input_idx);
void      bool_network_set_pi(FpgaBoolNetwork *bn, int node_id);
void      bool_network_set_po(FpgaBoolNetwork *bn, int node_id);
void      bool_network_levelize(FpgaBoolNetwork *bn);
void      bool_network_print(const FpgaBoolNetwork *bn);
void      bool_network_destroy(FpgaBoolNetwork *bn);

/* L4: Shannon Expansion for LUT decomposition
 * f(x1..xk) = xk * f(x1..xk-1, 1) + xk' * f(x1..xk-1, 0)
 * This is used when k > LUT size. */
void      shannon_decompose(uint64_t func, int k, int split_var,
                            uint64_t *cofactor0, uint64_t *cofactor1);

/* L5: Cut Enumeration
 * Enumerate all K-feasible cuts for a given node.
 * Dynamic programming: combine child cuts via Cartesian product.
 * Complexity: O(N * max_cuts^2) where N = nodes */
int       cut_enumerate(FpgaBoolNetwork *bn, int node_id, int K,
                        FpgaCut *cuts, int max_cuts);

/* L5: FlowMap — optimal depth K-LUT mapping
 * Two phases:
 *   Phase 1: Label each node with min depth (topological order)
 *   Phase 2: Generate mapping (reverse topological)
 * Reference: Cong & Ding, "FlowMap: An Optimal Technology Mapping
 *            Algorithm for Delay Optimization", TCAD 1994 */
int       flowmap_mapping(FpgaBoolNetwork *bn, int K, FpgaLutMapping *result);

/* L5: FlowMap Phase 1 — compute labels */
int       flowmap_label(FpgaBoolNetwork *bn, int K, int *labels);

/* L5: FlowMap Phase 2 — generate mapping */
int       flowmap_generate(FpgaBoolNetwork *bn, int K, const int *labels,
                           FpgaLutMapping *result);

/* Combine two cuts */
bool      cuts_merge(const FpgaCut *c1, const FpgaCut *c2, int K, FpgaCut *result);

/* Check if cut is K-feasible */
bool      cut_is_k_feasible(const FpgaCut *cut, int K);

/* Compute truth table for a cut (simulate all input combinations) */
uint64_t  cut_truth_table(const FpgaBoolNetwork *bn, const FpgaCut *cut);

/* L8: Area-oriented mapping (DAGMap) */
int       dagmap_area_mapping(FpgaBoolNetwork *bn, int K, FpgaLutMapping *result);

/* L8: Technology mapping with carry chain extraction */
int       techmap_extract_carry(FpgaBoolNetwork *bn, FpgaLutMapping *result);

/* Mapping utilities */
void      lut_mapping_init(FpgaLutMapping *m, int max_entries);
void      lut_mapping_destroy(FpgaLutMapping *m);
void      lut_mapping_print(const FpgaLutMapping *m);
int       lut_mapping_export_to_atoms(const FpgaLutMapping *m,
                                      FpgaAtomNetlist *nl);

#endif
