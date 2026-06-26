/**
 * noc_qos.c ? NoC Quality of Service Implementation
 *
 * QoS mechanisms: priority classes, weighted arbitration,
 * token-bucket rate limiting, SLA monitoring.
 */

#include "noc_qos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ??? QoS configuration ??????????????????????????????????????????????? */

void noc_qos_config_init(noc_qos_config_t *cfg, noc_qos_class_t cls,
                         int32_t priority, int32_t min_bw_pct,
                         int32_t max_lat, double weight) {
    if (!cfg) return;

    memset(cfg, 0, sizeof(noc_qos_config_t));
    cfg->class_id = cls;
    cfg->priority = priority;
    cfg->min_bandwidth_pct = min_bw_pct;
    cfg->max_latency_cycles = max_lat;
    cfg->weight = weight;

    switch (cls) {
    case NOC_QOS_BEST_EFFORT:     cfg->name = "Best-Effort"; break;
    case NOC_QOS_LOW_LATENCY:     cfg->name = "Low-Latency"; break;
    case NOC_QOS_GUARANTEED_THR:  cfg->name = "Guaranteed-THR"; break;
    case NOC_QOS_REAL_TIME:       cfg->name = "Real-Time"; break;
    default:                      cfg->name = "Unknown"; break;
    }
}

/* ??? QoS-aware VC allocator ??????????????????????????????????????????? */

void noc_qos_vc_allocator_init(noc_qos_vc_allocator_t *qos_alloc,
                                int32_t num_vcs,
                                const noc_qos_config_t *configs,
                                int32_t num_configs) {
    if (!qos_alloc) return;

    memset(qos_alloc, 0, sizeof(noc_qos_vc_allocator_t));

    int32_t i;
    /* Copy configs */
    for (i = 0; i < num_configs && i < NOC_QOS_MAX_CLASSES; i++) {
        qos_alloc->class_config[i] = configs[i];
    }

    /* Distribute VCs among classes proportionally to weight */
    double total_weight = 0.0;
    for (i = 0; i < num_configs; i++) {
        if (configs[i].weight <= 0.0) continue;
        total_weight += configs[i].weight;
    }

    if (total_weight <= 0.0) total_weight = 1.0;

    int32_t vcs_assigned = 0;
    for (i = 0; i < num_configs; i++) {
        if (configs[i].weight <= 0.0) continue;
        int32_t share = (int32_t)((double)num_vcs * configs[i].weight / total_weight);
        if (share < 1) share = 1; /* At least 1 VC per class */
        qos_alloc->class_vc_count[i] = share;
        vcs_assigned += share;
    }

    /* Assign remaining VCs to first class */
    while (vcs_assigned < num_vcs && vcs_assigned < NOC_MAX_VC) {
        qos_alloc->class_vc_count[0]++;
        vcs_assigned++;
    }

    /* Map VCs to classes */
    int32_t vc = 0;
    for (i = 0; i < NOC_QOS_MAX_CLASSES; i++) {
        int32_t j;
        for (j = 0; j < qos_alloc->class_vc_count[i] && vc < NOC_MAX_VC; j++) {
            qos_alloc->vc_class[vc] = (noc_qos_class_t)i;
            vc++;
        }
    }

    /* Initialize WRR arbiter */
    qos_alloc->wrr.num_classes = num_configs;
    for (i = 0; i < num_configs && i < NOC_QOS_MAX_CLASSES; i++) {
        qos_alloc->wrr.weights[i] = configs[i].weight;
        qos_alloc->wrr.deficit[i] = 0.0;
    }
    qos_alloc->wrr.current_class = 0;
    qos_alloc->wrr.quantum = 1;
}

/* ??? Weighted Round-Robin (DWRR) ??????????????????????????????????????
 *
 * Deficit Weighted Round-Robin (Shreedhar & Varghese, IEEE/ACM ToN 1996).
 *
 * Each class has a deficit counter. In each round, if the class has
 * a pending request and its deficit >= quantum, it is serviced.
 * Otherwise, deficit accumulates.
 */

int32_t noc_wrr_select(noc_wrr_arbiter_t *wrr, const int32_t *requests) {
    if (!wrr || !requests) return -1;

    int32_t start = wrr->current_class;
    int32_t i;

    for (i = 0; i < wrr->num_classes; i++) {
        int32_t cls = (start + i) % wrr->num_classes;

        /* Add weight to deficit */
        wrr->deficit[cls] += wrr->weights[cls];

        if (requests[cls] && wrr->deficit[cls] >= (double)wrr->quantum) {
            wrr->deficit[cls] -= (double)wrr->quantum;
            wrr->current_class = (cls + 1) % wrr->num_classes;
            return cls;
        }
    }

    /* No request ready: accumulate all deficits */
    for (i = 0; i < wrr->num_classes; i++) {
        wrr->deficit[i] += wrr->weights[i];
    }

    return -1;
}

/* ??? VC class lookup ?????????????????????????????????????????????????? */

noc_qos_class_t noc_qos_vc_class(const noc_qos_vc_allocator_t *alloc,
                                  int32_t vc_id) {
    if (!alloc || vc_id < 0 || vc_id >= NOC_MAX_VC) return NOC_QOS_BEST_EFFORT;
    return alloc->vc_class[vc_id];
}

/* ??? Admission control ??????????????????????????????????????????????? */

int noc_qos_admit(const noc_qos_vc_allocator_t *alloc, noc_qos_class_t cls) {
    if (!alloc) return 0;
    if (cls < 0 || cls >= NOC_QOS_MAX_CLASSES) return 0;

    /* Simple admission: accept if class has VCs assigned */
    return (alloc->class_vc_count[cls] > 0) ? 1 : 0;
}

/* ??? Token bucket rate limiter ???????????????????????????????????????
 *
 * Classic token bucket algorithm for traffic shaping.
 * Tokens accumulate at rate 'rate' up to 'burst_size'.
 * Each packet consumes 1 token.
 */

void noc_rate_limiter_init(noc_rate_limiter_t *rl, double rate, double burst) {
    if (!rl) return;

    memset(rl, 0, sizeof(noc_rate_limiter_t));
    rl->rate = rate;
    rl->burst_size = burst;
    rl->tokens = burst; /* Start with full bucket */
}

int noc_rate_limiter_allow(noc_rate_limiter_t *rl) {
    if (!rl) return 0;

    if (rl->tokens >= 1.0) {
        rl->tokens -= 1.0;
        rl->packets_allowed++;
        return 1;
    }

    rl->packets_blocked++;
    return 0;
}

void noc_rate_limiter_refill(noc_rate_limiter_t *rl) {
    if (!rl) return;

    rl->tokens += rl->rate;
    if (rl->tokens > rl->burst_size) {
        rl->tokens = rl->burst_size;
    }
}

/* ??? SLA monitor ????????????????????????????????????????????????????? */

void noc_sla_monitor_init(noc_sla_monitor_t *sla,
                          int32_t target_latency, int32_t target_throughput) {
    if (!sla) return;

    memset(sla, 0, sizeof(noc_sla_monitor_t));
    sla->target_latency = target_latency;
    sla->target_throughput = target_throughput;
    sla->window_size = 1000; /* 1000 cycles */
    sla->compliance_ratio = 1.0;
}

void noc_sla_monitor_update(noc_sla_monitor_t *sla,
                            int32_t measured_latency, int32_t measured_throughput) {
    if (!sla) return;

    sla->current_latency = measured_latency;
    sla->current_throughput = measured_throughput;

    /* Check violation */
    if (measured_latency > sla->target_latency ||
        measured_throughput < sla->target_throughput) {
        sla->violations++;
    }

    /* Compute compliance ratio over window */
    /* Simplified: ratio = non-violation fraction */
    double window = (double)sla->window_size;
    sla->compliance_ratio = 1.0 - (double)sla->violations / window;
    if (sla->compliance_ratio < 0.0) sla->compliance_ratio = 0.0;
}

int noc_sla_monitor_check(const noc_sla_monitor_t *sla) {
    if (!sla) return 0;
    return (sla->compliance_ratio > 0.95) ? 1 : 0;
}

/* ??? Statistics ?????????????????????????????????????????????????????? */

void noc_qos_print_stats(const noc_qos_vc_allocator_t *alloc) {
    if (!alloc) return;

    printf("=== QoS VC Allocator Stats ===\n");
    printf("Classes configured: %d\n", alloc->wrr.num_classes);

    int32_t i;
    for (i = 0; i < NOC_QOS_MAX_CLASSES; i++) {
        if (alloc->class_vc_count[i] <= 0) continue;
        printf("  Class %d (%s): VCs=%d, Priority=%d, Weight=%.2f\n",
               i, alloc->class_config[i].name,
               alloc->class_vc_count[i],
               alloc->class_config[i].priority,
               alloc->class_config[i].weight);
        printf("    Flits sent: %llu, Dropped: %llu, Stalls: %llu\n",
               (unsigned long long)alloc->flits_sent[i],
               (unsigned long long)alloc->flits_dropped[i],
               (unsigned long long)alloc->stall_cycles[i]);
        printf("    Avg latency: %.2f cycles\n", alloc->avg_latency[i]);
    }
}
