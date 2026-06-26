/**
 * noc_qos.h ? L5/L7: NoC Quality of Service
 *
 * Implements QoS mechanisms for NoC: priority classes,
 * weighted arbitration, rate limiting, and service-level
 * guarantees.
 *
 * Core Concepts (L2):
 *   - Priority-based VC allocation
 *   - Weighted round-robin arbitration
 *   - Rate limiting for traffic classes
 *
 * Standards (L4):
 *   - Goossens et al., "?thereal Network on Chip" (IEEE Design & Test 2005)
 *     ? Guaranteed throughput (GT) + best-effort (BE) service model
 *   - Bolotin et al., "QNoC: QoS Architecture for NoC" (IEEE Micro 2004)
 *
 * Course Mapping:
 *   - CMU 18-742: QoS in interconnection networks
 *   - Stanford CS 315A: prioritized communication
 *   - ETH 263-3501: parallel programming ? non-uniform communication
 */

#ifndef NOC_QOS_H
#define NOC_QOS_H

#include <stdint.h>
#include <stddef.h>
#include "noc_router.h"

/* ??? L1: QoS service classes ???????????????????????????????????????? */

typedef enum {
    NOC_QOS_BEST_EFFORT     = 0,
    NOC_QOS_LOW_LATENCY     = 1,
    NOC_QOS_GUARANTEED_THR  = 2,
    NOC_QOS_REAL_TIME       = 3,
    NOC_QOS_CLASS_COUNT     = 4
} noc_qos_class_t;

#define NOC_QOS_MAX_CLASSES 4

typedef struct noc_qos_config {
    noc_qos_class_t class_id;
    const char     *name;
    int32_t         priority;         /* 0=highest */
    int32_t         min_bandwidth_pct; /* Minimum guaranteed bandwidth % */
    int32_t         max_latency_cycles;/* Max end-to-end latency bound */
    double          weight;           /* For weighted arbitration */
} noc_qos_config_t;

/* ??? L1: Weighted round-robin arbiter ??????????????????????????????? */

typedef struct noc_wrr_arbiter {
    int32_t  num_classes;
    double   weights[NOC_QOS_MAX_CLASSES];
    double   deficit[NOC_QOS_MAX_CLASSES];  /* Deficit counter per class */
    int32_t  current_class;
    int32_t  quantum;                        /* Minimum service quantum */
} noc_wrr_arbiter_t;

/* ??? L1: QoS-aware VC allocator extension ??????????????????????????? */

typedef struct noc_qos_vc_allocator {
    noc_qos_class_t vc_class[NOC_MAX_VC];     /* QoS class per VC */
    int32_t         class_vc_count[NOC_QOS_MAX_CLASSES]; /* VCs per class */
    noc_qos_config_t class_config[NOC_QOS_MAX_CLASSES];
    noc_wrr_arbiter_t wrr;
    /* Statistics per class */
    uint64_t  flits_sent[NOC_QOS_MAX_CLASSES];
    uint64_t  flits_dropped[NOC_QOS_MAX_CLASSES];
    uint64_t  stall_cycles[NOC_QOS_MAX_CLASSES];
    double    avg_latency[NOC_QOS_MAX_CLASSES];
} noc_qos_vc_allocator_t;

/* ??? L5: Rate limiter (token bucket) ???????????????????????????????? */

typedef struct noc_rate_limiter {
    double    rate;             /* Tokens per cycle */
    double    burst_size;       /* Max burst in flits */
    double    tokens;           /* Current token count */
    uint64_t  packets_allowed;
    uint64_t  packets_blocked;
} noc_rate_limiter_t;

/* ??? L7: Service-level agreement (SLA) monitor ?????????????????????? */

typedef struct noc_sla_monitor {
    int32_t   target_latency;       /* Guaranteed max latency */
    int32_t   target_throughput;    /* Guaranteed min throughput (flits/1k cycles) */
    int32_t   window_size;          /* Measurement window in cycles */
    uint64_t  violations;           /* SLA violations */
    int32_t   current_latency;
    int32_t   current_throughput;
    double    compliance_ratio;     /* Window cycles within SLA / total */
} noc_sla_monitor_t;

/* ??? API: QoS configuration ????????????????????????????????????????? */

void noc_qos_config_init(noc_qos_config_t *cfg, noc_qos_class_t cls,
                         int32_t priority, int32_t min_bw_pct,
                         int32_t max_lat, double weight);
void noc_qos_vc_allocator_init(noc_qos_vc_allocator_t *qos_alloc,
                                int32_t num_vcs,
                                const noc_qos_config_t *configs,
                                int32_t num_configs);

/* ??? API: QoS-aware arbitration ????????????????????????????????????? */

/**
 * Weighted Round-Robin: select next flow to serve.
 * Uses deficit weighted round-robin (DWRR) for fair bandwidth allocation.
 *
 * Complexity: O(C) where C = number of classes.
 *
 * Reference: Shreedhar & Varghese, "Efficient Fair Queuing
 * Using Deficit Round-Robin" (IEEE/ACM ToN 1996).
 */
int32_t noc_wrr_select(noc_wrr_arbiter_t *wrr, const int32_t *requests);

/**
 * Get the QoS class of a specific VC.
 * Complexity: O(1)
 */
noc_qos_class_t noc_qos_vc_class(const noc_qos_vc_allocator_t *alloc, int32_t vc_id);

/**
 * Check if a packet of given QoS class can be admitted based on
 * current resource allocation.
 * Returns 1 if admitted, 0 if must wait.
 * Complexity: O(1)
 */
int noc_qos_admit(const noc_qos_vc_allocator_t *alloc, noc_qos_class_t cls);

/* ??? L5: Rate limiter ??????????????????????????????????????????????? */

void noc_rate_limiter_init(noc_rate_limiter_t *rl, double rate, double burst);
int noc_rate_limiter_allow(noc_rate_limiter_t *rl);
void noc_rate_limiter_refill(noc_rate_limiter_t *rl);

/* ??? SLA monitor ???????????????????????????????????????????????????? */

void noc_sla_monitor_init(noc_sla_monitor_t *sla,
                          int32_t target_latency, int32_t target_throughput);
void noc_sla_monitor_update(noc_sla_monitor_t *sla,
                            int32_t measured_latency, int32_t measured_throughput);
int noc_sla_monitor_check(const noc_sla_monitor_t *sla);
void noc_qos_print_stats(const noc_qos_vc_allocator_t *alloc);

#endif /* NOC_QOS_H */
