#include "hls_dfg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

void dfg_init(DataFlowGraph *dfg) {
    dfg->node_count = 0; dfg->edge_count = 0;
}

int dfg_add_node(DataFlowGraph *dfg, HlsOp op, const char *name, double area) {
    if (dfg->node_count >= MAX_DFG_NODES) return -1;
    DfgNode *n = &dfg->nodes[dfg->node_count];
    n->id = dfg->node_count; n->op = op; n->latency = (op == HLS_MUL || op == HLS_DIV) ? 2 : 1;
    n->area = area; n->asap = -1; n->alap = -1; n->schedule_cycle = -1;
    n->input_count = 0; n->scheduled = false;
    strncpy(n->name, name[0] ? name : "anon", 31); n->name[31] = '\0';
    return dfg->node_count++;
}

int dfg_add_edge(DataFlowGraph *dfg, int from, int to, int delay) {
    if (dfg->edge_count >= MAX_DFG_EDGES || from < 0 || to < 0 || from >= dfg->node_count || to >= dfg->node_count) return -1;
    DfgEdge *e = &dfg->edges[dfg->edge_count]; e->from = from; e->to = to; e->delay = delay; e->is_data = true;
    DfgNode *t = &dfg->nodes[to];
    if (t->input_count < 4) t->inputs[t->input_count++] = from;
    return dfg->edge_count++;
}
static double dfg_node_cost(DfgNode *n) { switch (n->op) { case HLS_MUL: return 3.0; case HLS_DIV: return 5.0; default: return 1.0; } }
int dfg_critical_path(DataFlowGraph *dfg) {
    int dist[MAX_DFG_NODES]; for (int i = 0; i < dfg->node_count; i++) dist[i] = 0;
    for (int i = 0; i < dfg->node_count; i++) {
        for (int j = 0; j < dfg->edge_count; j++) {
            DfgEdge *e = &dfg->edges[j]; int w = (int)dfg_node_cost(&dfg->nodes[e->from]);
            if (dist[e->from] + w > dist[e->to]) dist[e->to] = dist[e->from] + w;
        }
    }
    int max_d = 0; for (int i = 0; i < dfg->node_count; i++) if (dist[i] > max_d) max_d = dist[i];
    return max_d;
}
static const char *op_name(HlsOp op) { switch (op) { case HLS_ADD: return "+"; case HLS_SUB: return "-"; case HLS_MUL: return "*"; case HLS_DIV: return "/"; case HLS_LOAD: return "ld"; case HLS_STORE: return "st"; case HLS_PHI: return "phi"; case HLS_CMP: return "cmp"; default: return "?"; } }
void dfg_print(DataFlowGraph *dfg) {
    printf("DataFlowGraph: %d nodes, %d edges\n", dfg->node_count, dfg->edge_count);
    for (int i = 0; i < dfg->node_count; i++) { DfgNode *n = &dfg->nodes[i]; printf("  n%d [%s] %s cycle=%d\n", n->id, n->name, op_name(n->op), n->schedule_cycle); }
    for (int i = 0; i < dfg->edge_count; i++) printf("  n%d -> n%d\n", dfg->edges[i].from, dfg->edges[i].to);
}

int dfg_build_from_c(DataFlowGraph *dfg, const char *c_source) {
    dfg_init(dfg);
    /* simple parser: detect a+b, a*b, a-b patterns and build DFG */
    char buf[4096]; strncpy(buf, c_source, 4095); buf[4095] = '\0';
    char *vars[16]; int vc = 0; int max_nodes = 8; char *ops[] = {"*","/","+","-"};
    /* extract variable names */
    char *s = buf; while (*s) {
        if ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z')) { char *start = s;
            while ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_') s++;
            int len = (int)(s - start); if (len > 0 && len < 32) {
                int found = 0;
                for (int k = 0; k < vc; k++) if (strncmp(vars[k], start, len) == 0 && strlen(vars[k]) == (size_t)len) { found = 1; break; }
                if (!found && vc < 16) { vars[vc] = start; vars[vc][len] = '\0'; vc++; }
            }
        } else s++;
    }
    /* create load nodes for each variable */ int load_ids[16];
    for (int i = 0; i < vc && dfg->node_count < max_nodes; i++) load_ids[i] = dfg_add_node(dfg, HLS_LOAD, vars[i], 0.5);
    /* detect operations */ s = buf;
    char *eq = strchr(buf, '='); if (!eq) return dfg->node_count;
    /* parse rhs: simple pattern var op var */
    char rhs[256]; strncpy(rhs, eq+1, 255); rhs[255] = '\0';
    /* strip semicolon and whitespace */ char *p = rhs; while (*p == ' ') p++; char *end = p + strlen(p) - 1; while (end > p && (*end == ';' || *end == ' ' || *end == '\n')) end--; *(end+1) = '\0';
    for (int oi = 0; oi < 4; oi++) { char *op_pos = strstr(p, ops[oi]); if (op_pos) {
        int llen = (int)(op_pos - p); char left[32]; strncpy(left, p, llen); left[llen] = '\0';
        char *right = op_pos + strlen(ops[oi]); char rclean[32]; int ri = 0; while (*right && ri < 31) { if (*right != ' ') rclean[ri++] = *right; right++; } rclean[ri] = '\0';
        int li = -1, ri_idx = -1;
        for (int k = 0; k < vc; k++) { if (strcmp(vars[k], left) == 0) li = k; if (strcmp(vars[k], rclean) == 0) ri_idx = k; }
        if (li >= 0 && ri_idx >= 0) {
            HlsOp op; if (oi == 0) op = HLS_MUL; else if (oi == 1) op = HLS_DIV; else if (oi == 2) op = HLS_ADD; else op = HLS_SUB;
            int result = dfg_add_node(dfg, op, "result", op == HLS_MUL ? 5.0 : op == HLS_DIV ? 8.0 : 1.5);
            dfg_add_edge(dfg, load_ids[li], result, 0); dfg_add_edge(dfg, load_ids[ri_idx], result, 0);
            /* store result */ int st = dfg_add_node(dfg, HLS_STORE, "out", 0.5);
            dfg_add_edge(dfg, result, st, 0);
        }
        break;
    }}
    return 0;
}

