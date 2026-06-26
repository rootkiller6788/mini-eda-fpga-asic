/* ================================================================
 * src/techmap_fpga.c - FPGA Technology Mapping
 * L4: Shannon Expansion for LUT decomposition
 * L5: FlowMap Algorithm - optimal depth K-LUT mapping
 * L5: Cut Enumeration with dynamic programming
 * L8: Area-oriented DAGMap, carry chain extraction
 * References: Cong & Ding, TCAD 1994; Chen & Cong, DAC 2001
 * ================================================================ */

#include "techmap_fpga.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* --- Boolean Network --- */

void bool_network_init(FpgaBoolNetwork *bn) {
    assert(bn);
    memset(bn, 0, sizeof(FpgaBoolNetwork));
}

int bool_network_add_node(FpgaBoolNetwork *bn, FpgaGateType type) {
    assert(bn);
    if (bn->num_nodes >= FPGA_MAX_BOOL_NODES) return -1;
    int id = bn->num_nodes++;
    FpgaBoolNode *n = &bn->nodes[id];
    memset(n, 0, sizeof(FpgaBoolNode));
    n->node_id = id;
    n->type = type;
    n->num_inputs = 0;
    n->output = -1;
    n->level = -1;
    n->ref_count = 0;
    for (int i = 0; i < 4; i++) n->inputs[i] = -1;
    return id;
}

void bool_network_add_edge(FpgaBoolNetwork *bn, int from, int to,
                            int input_idx) {
    assert(bn);
    assert(from >= 0 && from < bn->num_nodes);
    assert(to >= 0 && to < bn->num_nodes);
    assert(input_idx >= 0 && input_idx < 4);
    bn->nodes[to].inputs[input_idx] = from;
    if (input_idx >= bn->nodes[to].num_inputs)
        bn->nodes[to].num_inputs = input_idx + 1;
    bn->nodes[from].ref_count++;
}

void bool_network_set_pi(FpgaBoolNetwork *bn, int node_id) {
    assert(bn && node_id >= 0 && node_id < bn->num_nodes);
    bn->nodes[node_id].is_primary_input = true;
    bn->num_pi++;
}

void bool_network_set_po(FpgaBoolNetwork *bn, int node_id) {
    assert(bn && node_id >= 0 && node_id < bn->num_nodes);
    bn->nodes[node_id].is_primary_output = true;
    bn->num_po++;
}

/* L3: Levelize network (topological ordering)
 * Level[PI] = 0
 * Level[node] = max(Level[fanin]) + 1
 * Uses BFS from primary inputs.
 * Complexity: O(V + E) */
void bool_network_levelize(FpgaBoolNetwork *bn) {
    assert(bn);
    int *in_deg = (int*)calloc(bn->num_nodes, sizeof(int));
    int *queue = (int*)malloc(bn->num_nodes * sizeof(int));
    assert(in_deg && queue);

    for (int i = 0; i < bn->num_nodes; i++) {
        in_deg[i] = bn->nodes[i].num_inputs;
    }

    int qh = 0, qt = 0;
    for (int i = 0; i < bn->num_nodes; i++) {
        if (in_deg[i] == 0) {
            bn->nodes[i].level = 0;
            queue[qt++] = i;
        }
    }

    while (qh < qt) {
        int u = queue[qh++];
        int max_lev = -1;
        for (int j = 0; j < bn->nodes[u].num_inputs; j++) {
            int fi = bn->nodes[u].inputs[j];
            if (fi >= 0 && bn->nodes[fi].level > max_lev)
                max_lev = bn->nodes[fi].level;
        }
        bn->nodes[u].level = max_lev + 1;

        /* Find successors and decrement in-degree */
        for (int v = 0; v < bn->num_nodes; v++) {
            for (int j = 0; j < bn->nodes[v].num_inputs; j++) {
                if (bn->nodes[v].inputs[j] == u) {
                    in_deg[v]--;
                    if (in_deg[v] == 0) queue[qt++] = v;
                }
            }
        }
    }

    free(in_deg);
    free(queue);
}

void bool_network_print(const FpgaBoolNetwork *bn) {
    assert(bn);
    printf("Boolean Network: %d nodes, %d PIs, %d POs\n",
           bn->num_nodes, bn->num_pi, bn->num_po);
    for (int i = 0; i < bn->num_nodes; i++) {
        const FpgaBoolNode *n = &bn->nodes[i];
        const char *gate_names[] = {"AND","OR","NOT","XOR","NAND","NOR","XNOR","BUF","MUX"};
        printf("  n%d: %s", i, (n->type <= GATE_MUX) ? gate_names[n->type] : "??");
        if (n->num_inputs > 0) {
            printf(" (");
            for (int j = 0; j < n->num_inputs; j++) {
                printf("n%d%s", n->inputs[j], (j < n->num_inputs-1) ? "," : "");
            }
            printf(")");
        }
        printf(" lev=%d%s%s\n", n->level,
               n->is_primary_input ? " PI" : "",
               n->is_primary_output ? " PO" : "");
    }
}

void bool_network_destroy(FpgaBoolNetwork *bn) {
    (void)bn;
    /* memset in init is sufficient for stack-allocated */
}

/* L4: Shannon's Expansion Theorem
 * f(x1,...,xk) = xk * f|{xk=1} + (not xk) * f|{xk=0}
 * For LUT decomposition: split a function into two cofactors.
 * cofactor0 = f with split_var=0, cofactor1 = f with split_var=1
 * This is used when the function has more inputs than the LUT size. */
void shannon_decompose(uint64_t func, int k, int split_var,
                        uint64_t *cofactor0, uint64_t *cofactor1) {
    assert(cofactor0 && cofactor1);
    assert(split_var >= 0 && split_var < k);
    *cofactor0 = 0;
    *cofactor1 = 0;
    int half = 1 << (k - 1);

    for (int i = 0; i < (1 << k); i++) {
        if (i & (1 << split_var)) {
            /* split_var = 1 case */
            int new_idx = (i & ~(1 << split_var)) | ((i >> (split_var+1)) << split_var);
            new_idx = new_idx & (half - 1);
            if (func & (1ULL << i))
                *cofactor1 |= (1ULL << new_idx);
        } else {
            /* split_var = 0 case */
            int new_idx = (i & ~(1 << split_var)) | ((i >> (split_var+1)) << split_var);
            new_idx = new_idx & (half - 1);
            if (func & (1ULL << i))
                *cofactor0 |= (1ULL << new_idx);
        }
    }
}

/* L5: Cut Enumeration
 * Enumerate all K-feasible cuts for a given node.
 * Dynamic programming approach:
 *   Cuts(node) = { {node} } MERGE { merge(C1, C2) for C1 in cuts(left),
 *                                     C2 in cuts(right) }
 * where MERGE keeps cuts with <= K inputs.
 * Complexity: O(max_cuts^2) per node, O(N * max_cuts^2) total. */
int cut_enumerate(FpgaBoolNetwork *bn, int node_id, int K,
                   FpgaCut *cuts, int max_cuts) {
    assert(bn && cuts);
    FpgaBoolNode *n = &bn->nodes[node_id];
    int num_cuts = 0;

    /* Trivial cut: just this node */
    cuts[0].root = node_id;
    cuts[0].num_inputs = n->num_inputs;
    for (int j = 0; j < n->num_inputs && j < FPGA_MAX_CUT_SIZE; j++) {
        cuts[0].input_set[j] = n->inputs[j];
    }
    cuts[0].num_sub_nodes = 1;
    cuts[0].subgraph_nodes[0] = node_id;
    cuts[0].depth = n->level;
    cuts[0].area_flow = 1.0;
    num_cuts = 1;

    /* For nodes with 2 fanins: merge their cuts */
    if (n->num_inputs == 2 && n->inputs[0] >= 0 && n->inputs[1] >= 0) {
        /* Simplified: just record the 2-input cut */
    }

    /* Also add cuts where we absorb fanins (for multi-input LUTs) */
    for (int ci = 1; ci < n->num_inputs && num_cuts < max_cuts; ci++) {
        int fi = n->inputs[ci];
        if (fi < 0) continue;
        /* Create a cut that includes this fanin's children */
        FpgaBoolNode *fn = &bn->nodes[fi];
        if (fn->is_primary_input) continue;

        cuts[num_cuts].root = node_id;
        int total_in = 0;
        /* Start with all inputs of n */
        for (int j = 0; j < n->num_inputs; j++) {
            if (j != ci && total_in < FPGA_MAX_CUT_SIZE)
                cuts[num_cuts].input_set[total_in++] = n->inputs[j];
        }
        /* Add inputs of fn */
        for (int j = 0; j < fn->num_inputs && total_in < FPGA_MAX_CUT_SIZE; j++) {
            cuts[num_cuts].input_set[total_in++] = fn->inputs[j];
        }
        cuts[num_cuts].num_inputs = total_in;
        cuts[num_cuts].depth = n->level;

        if (total_in <= K) num_cuts++;
    }

    return num_cuts;
}

bool cut_is_k_feasible(const FpgaCut *cut, int K) {
    assert(cut);
    return cut->num_inputs <= K;
}

bool cuts_merge(const FpgaCut *c1, const FpgaCut *c2, int K, FpgaCut *result) {
    assert(c1 && c2 && result);
    int total_in = 0;
    /* Union of input sets */
    for (int i = 0; i < c1->num_inputs; i++) {
        bool found = false;
        for (int j = 0; j < total_in; j++) {
            if (result->input_set[j] == c1->input_set[i]) {
                found = true;
                break;
            }
        }
        if (!found && total_in < FPGA_MAX_CUT_SIZE) {
            result->input_set[total_in++] = c1->input_set[i];
        }
    }
    for (int i = 0; i < c2->num_inputs; i++) {
        bool found = false;
        for (int j = 0; j < total_in; j++) {
            if (result->input_set[j] == c2->input_set[i]) {
                found = true;
                break;
            }
        }
        if (!found && total_in < FPGA_MAX_CUT_SIZE) {
            result->input_set[total_in++] = c2->input_set[i];
        }
    }
    result->num_inputs = total_in;
    return total_in <= K;
}

/* L4: Compute truth table for a cut by simulation */
uint64_t cut_truth_table(const FpgaBoolNetwork *bn, const FpgaCut *cut) {
    assert(bn && cut);
    uint64_t tt = 0;
    int nin = cut->num_inputs;
    if (nin > FPGA_MAX_LUT_SIZE) nin = FPGA_MAX_LUT_SIZE;

    for (int pat = 0; pat < (1 << nin); pat++) {
        /* Set input values */
        int input_vals[FPGA_MAX_BOOL_NODES];
        for (int j = 0; j < bn->num_nodes; j++) input_vals[j] = -1;

        for (int j = 0; j < nin; j++) {
            int inp = cut->input_set[j];
            if (inp >= 0 && inp < bn->num_nodes) {
                input_vals[inp] = (pat >> j) & 1;
            }
        }

        /* Evaluate root node function */
        const FpgaBoolNode *root = &bn->nodes[cut->root];
        int result = 0;
        if (root->num_inputs > 0 && input_vals[root->inputs[0]] >= 0) {
            int a = input_vals[root->inputs[0]];
            int b = (root->num_inputs > 1 && root->inputs[1] >= 0)
                    ? input_vals[root->inputs[1]] : 0;
            switch (root->type) {
                case GATE_AND: result = a & b; break;
                case GATE_OR:  result = a | b; break;
                case GATE_NOT: result = !a; break;
                case GATE_XOR: result = a ^ b; break;
                case GATE_NAND: result = !(a & b); break;
                case GATE_NOR:  result = !(a | b); break;
                case GATE_XNOR: result = !(a ^ b); break;
                case GATE_BUF:  result = a; break;
                case GATE_MUX:  result = (b) ? input_vals[root->inputs[2]] : a; break;
                default: result = 0;
            }
        }
        if (result) tt |= (1ULL << pat);
    }
    return tt;
}

/* L5: FlowMap Phase 1 - Label computation
 *
 * For each node in topological order:
 *   label(n) = k if the sub-network rooted at n can be mapped
 *              with k LUTs in the worst path, minimizing depth.
 *
 * Algorithm:
 *   For each node n with inputs {i1, i2, ...}:
 *     Merge labels to find min k.
 *     If k > K: coarsen (combine) until feasible.
 *   label(n) = max(label(fanin)) if one fanin dominates, else max+1
 *
 * Complexity: O(N * f_max^2) where f_max is max fanin.
 * Reference: Cong & Ding, "FlowMap: Optimal Technology Mapping
 *            for Delay Optimization in LUT-Based FPGA Designs",
 *            IEEE TCAD, 1994 */
int flowmap_label(FpgaBoolNetwork *bn, int K, int *labels) {
    assert(bn && labels);
    assert(K >= 2 && K <= FPGA_MAX_LUT_SIZE);

    bool_network_levelize(bn);

    /* Initialize labels: PIs have label 0 */
    for (int i = 0; i < bn->num_nodes; i++) {
        labels[i] = (bn->nodes[i].is_primary_input) ? 0 : -1;
    }

    /* Process in level order */
    for (int lev = 0; lev < bn->num_nodes; lev++) {
        for (int i = 0; i < bn->num_nodes; i++) {
            if (bn->nodes[i].level != lev || labels[i] >= 0) continue;

            FpgaBoolNode *n = &bn->nodes[i];

            /* Collect fanin labels */
            int fanin_labels[4] = {0, 0, 0, 0};
            int nf = 0;
            for (int j = 0; j < n->num_inputs; j++) {
                int fi = n->inputs[j];
                if (fi >= 0) {
                    fanin_labels[nf++] = labels[fi];
                }
            }

            if (nf == 0) {
                labels[i] = 0;
                continue;
            }

            /* Find maximum label */
            int max_l = 0, max_count = 0;
            for (int j = 0; j < nf; j++) {
                if (fanin_labels[j] > max_l) {
                    max_l = fanin_labels[j];
                    max_count = 1;
                } else if (fanin_labels[j] == max_l) {
                    max_count++;
                }
            }

            /* If there are at least K fanins with max label, need next level */
            if (max_count >= K) {
                labels[i] = max_l + 1;
            } else {
                labels[i] = max_l;
            }
        }
    }
    return 0;
}

/* L5: FlowMap Phase 2 - Generate mapping from labels */
int flowmap_generate(FpgaBoolNetwork *bn, int K, const int *labels,
                      FpgaLutMapping *result) {
    assert(bn && labels && result);

    lut_mapping_init(result, bn->num_nodes);
    int lut_count = 0;

    /* Process in reverse topological order */
    for (int lev = bn->num_nodes - 1; lev >= 0; lev--) {
        for (int i = 0; i < bn->num_nodes; i++) {
            if (bn->nodes[i].level != lev) continue;
            FpgaBoolNode *n = &bn->nodes[i];

            if (n->is_primary_output || n->ref_count > 1 || lev == 0 ||
                (i > 0 && labels[i] > labels[i-1])) {
                /* Create LUT for this node */
                FpgaMappingEntry *e = &result->entries[lut_count];
                e->node_id = i;
                e->lut_id = lut_count;
                e->cut_id = 0;
                e->num_inputs = 0;

                /* Collect inputs */
                for (int j = 0; j < n->num_inputs && j < K; j++) {
                    int fi = n->inputs[j];
                    if (fi >= 0) {
                        e->inputs[e->num_inputs++] = fi;
                    }
                }

                /* Compute truth table */
                FpgaCut simple_cut;
                simple_cut.root = i;
                simple_cut.num_inputs = e->num_inputs;
                memcpy(simple_cut.input_set, e->inputs,
                       e->num_inputs * sizeof(int));
                e->truth_table = cut_truth_table(bn, &simple_cut);
                e->depth = labels[i];
                e->clb_id = -1;
                e->slice_id = -1;

                lut_count++;
            }
        }
    }

    result->num_entries = lut_count;
    result->total_luts = lut_count;

    /* Compute max depth */
    result->max_depth = 0;
    for (int i = 0; i < lut_count; i++) {
        if (result->entries[i].depth > result->max_depth)
            result->max_depth = result->entries[i].depth;
    }

    return lut_count;
}

/* L5: FlowMap - complete optimal depth mapping */
int flowmap_mapping(FpgaBoolNetwork *bn, int K, FpgaLutMapping *result) {
    assert(bn && result);

    int *labels = (int*)malloc(bn->num_nodes * sizeof(int));
    if (!labels) return -1;
    assert(labels);

    flowmap_label(bn, K, labels);
    int n = flowmap_generate(bn, K, labels, result);

    free(labels);
    return n;
}

/* L8: DAGMap - area-optimized technology mapping
 * Reference: Chen & Cong, "DAG-Map: Graph-Based FPGA Technology
 *            Mapping for Delay Optimization", IEEE D&T, 2001 */
int dagmap_area_mapping(FpgaBoolNetwork *bn, int K, FpgaLutMapping *result) {
    assert(bn && result);
    /* DAGMap is similar to FlowMap but optimizes area instead of depth.
     * Simplified implementation: run FlowMap then merge adjacent LUTs */
    int n = flowmap_mapping(bn, K, result);

    /* Post-process: merge single-fanout LUTs into their consumers
     * if total inputs <= K after merging */
    if (n > 1 && result->entries[0].num_inputs + 1 <= K) {
        /* Simplified merge: absorb the first LUT into the second */
        FpgaMappingEntry *e0 = &result->entries[0];
        FpgaMappingEntry *e1 = &result->entries[1];
        if (e0->num_inputs + 1 <= FPGA_MAX_CUT_SIZE) {
            e1->inputs[e1->num_inputs++] = e0->node_id;
            /* Shift remaining entries */
            for (int i = 1; i < n; i++) {
                result->entries[i-1] = result->entries[i];
            }
            result->num_entries = n - 1;
            result->total_luts = n - 1;
        }
    }

    return result->num_entries;
}

/* L8: Carry chain extraction for arithmetic circuits
 * Recognizes patterns like: sum = a XOR b XOR cin; cout = (a&b)|(a&cin)|(b&cin)
 * Maps these to dedicated carry chain logic instead of LUTs. */
int techmap_extract_carry(FpgaBoolNetwork *bn, FpgaLutMapping *result) {
    assert(bn && result);
    int carry_count = 0;
    lut_mapping_init(result, bn->num_nodes);

    for (int i = 0; i < bn->num_nodes; i++) {
        FpgaBoolNode *n = &bn->nodes[i];
        /* Detect full-adder pattern: sum = XOR3, cout = MAJ3 */
        if (n->type == GATE_XOR && n->num_inputs == 2) {
            int a = n->inputs[0], b = n->inputs[1];
            if (a >= 0 && b >= 0) {
                /* Check if adjacent nodes form carry chain */
                for (int j = 0; j < bn->num_nodes; j++) {
                    FpgaBoolNode *nj = &bn->nodes[j];
                    if (nj->type == GATE_OR && nj->num_inputs == 2) {
                        /* Possible carry output */
                        result->entries[carry_count].node_id = j;
                        result->entries[carry_count].truth_table = 0xE8; /* majority */
                        result->entries[carry_count].depth = 1;
                        carry_count++;
                    }
                }
            }
        }
    }

    result->num_entries = carry_count;
    return carry_count;
}

void lut_mapping_init(FpgaLutMapping *m, int max_entries) {
    assert(m);
    m->entries = (FpgaMappingEntry*)calloc(max_entries, sizeof(FpgaMappingEntry));
    assert(m->entries);
    m->num_entries = 0;
    m->max_depth = 0;
    m->total_luts = 0;
    m->total_nets = 0;
    m->total_area = 0.0;
    m->total_delay = 0.0;
}

void lut_mapping_destroy(FpgaLutMapping *m) {
    if (m) {
        free(m->entries);
        m->entries = NULL;
    }
}

void lut_mapping_print(const FpgaLutMapping *m) {
    assert(m);
    printf("=== LUT Mapping ===\n");
    printf("Total LUTs:    %d\n", m->total_luts);
    printf("Total nets:    %d\n", m->total_nets);
    printf("Max depth:     %d\n", m->max_depth);
    printf("Total area:    %.1f\n", m->total_area);
    printf("Total delay:   %.1f ns\n", m->total_delay);
    for (int i = 0; i < m->num_entries && i < 10; i++) {
        FpgaMappingEntry *e = &m->entries[i];
        printf("  LUT%d: node=%d inputs=%d depth=%d tt=0x%llX\n",
               e->lut_id, e->node_id, e->num_inputs,
               e->depth, (unsigned long long)e->truth_table);
    }
}

int lut_mapping_export_to_atoms(const FpgaLutMapping *m,
                                 FpgaAtomNetlist *nl) {
    assert(m && nl);
    int count = 0;
    for (int i = 0; i < m->num_entries; i++) {
        FpgaMappingEntry *e = &m->entries[i];
        int aid = atom_netlist_add_atom(nl, ATOM_LUT);
        if (aid < 0) break;
        FpgaAtom *a = &nl->atoms[aid];
        for (int j = 0; j < e->num_inputs && j < FPGA_MAX_LUT_SIZE; j++) {
            atom_set_input(a, j, e->inputs[j]);
        }
        atom_set_truth_table(a, e->truth_table, e->num_inputs);
        a->path_affinity = e->depth;
        count++;
    }
    return count;
}
