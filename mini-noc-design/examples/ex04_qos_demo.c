/**
 * ex04_qos_demo.c ? L7 Application: QoS-aware NoC Routing
 *
 * Demonstrates QoS mechanisms:
 *   - Priority-based VC allocation
 *   - Weighted round-robin arbitration
 *   - Token bucket rate limiting
 *   - SLA monitoring
 */

#include <stdio.h>
#include <stdlib.h>
#include "noc_qos.h"

int main(void) {
    printf("???????????????????????????????????????????\n");
    printf("  Example 04: QoS-aware NoC Routing\n");
    printf("???????????????????????????????????????????\n\n");

    /* ??? Configure QoS Classes ?????????????????????????????????? */
    printf("??? QoS Class Configuration ???\n\n");

    noc_qos_config_t configs[4];
    noc_qos_config_init(&configs[0], NOC_QOS_REAL_TIME,      0, 50, 20,  4.0);
    noc_qos_config_init(&configs[1], NOC_QOS_LOW_LATENCY,    1, 30, 50,  2.0);
    noc_qos_config_init(&configs[2], NOC_QOS_GUARANTEED_THR, 2, 15, 100, 1.5);
    noc_qos_config_init(&configs[3], NOC_QOS_BEST_EFFORT,    3, 5,  200, 1.0);

    int32_t i;
    for (i = 0; i < 4; i++) {
        printf("  %s: priority=%d, BW?%d%%, Lat?%d cyc, weight=%.1f\n",
               configs[i].name, configs[i].priority,
               configs[i].min_bandwidth_pct, configs[i].max_latency_cycles,
               configs[i].weight);
    }

    /* ??? VC Allocator ?????????????????????????????????????????? */
    printf("\n??? VC Allocation ???\n\n");

    noc_qos_vc_allocator_t alloc;
    noc_qos_vc_allocator_init(&alloc, 12, configs, 4);

    printf("  VC-to-Class mapping (12 VCs):\n");
    int32_t v;
    for (v = 0; v < 12; v++) {
        noc_qos_class_t cls = noc_qos_vc_class(&alloc, v);
        printf("    VC%2d ? %s\n", v, configs[cls].name);
    }

    printf("\n  Class VC distribution:\n");
    for (i = 0; i < 4; i++) {
        printf("    %s: %d VCs\n", configs[i].name, alloc.class_vc_count[i]);
    }

    /* ??? WRR Arbitration ??????????????????????????????????????? */
    printf("\n??? Weighted Round-Robin (DWRR) ???\n\n");

    int32_t reqs[4] = {1, 1, 1, 1}; /* all classes requesting */
    printf("  Scheduler weights: [%.1f, %.1f, %.1f, %.1f]\n",
           alloc.wrr.weights[0], alloc.wrr.weights[1],
           alloc.wrr.weights[2], alloc.wrr.weights[3]);

    printf("  10 arbitration rounds:\n  ");
    int32_t r;
    for (r = 0; r < 10; r++) {
        int32_t winner = noc_wrr_select(&alloc.wrr, reqs);
        if (winner >= 0) {
            printf("%d ", winner);
        }
    }
    printf("\n  Expected: RT(weight 4) most frequent, BE(weight 1) least\n");

    /* ??? Rate Limiter ?????????????????????????????????????????? */
    printf("\n??? Token Bucket Rate Limiter ???\n\n");

    noc_rate_limiter_t rl;
    noc_rate_limiter_init(&rl, 0.3, 3.0);
    printf("  Rate=0.3 tokens/cycle, Burst=3 tokens\n");

    int32_t allowed = 0, blocked = 0;
    for (r = 0; r < 20; r++) {
        noc_rate_limiter_refill(&rl);
        if (noc_rate_limiter_allow(&rl)) allowed++;
        else blocked++;
    }
    printf("  Over 20 cycles: %d allowed, %d blocked (rate=%.2f)\n",
           allowed, blocked, (double)allowed / 20.0);

    /* ??? SLA Monitor ??????????????????????????????????????????? */
    printf("\n??? SLA Monitor ???\n\n");

    noc_sla_monitor_t sla;
    noc_sla_monitor_init(&sla, 100, 50);

    printf("  Target: Lat?100 cyc, Thr?50 flits/1k cyc\n");

    int32_t test_measurements[][2] = {
        {80, 60},  /* Compliant */
        {90, 55},  /* Compliant */
        {120, 45}, /* Latency violation */
        {85, 40},  /* Throughput violation */
        {75, 65},  /* Compliant */
    };
    int32_t n = 5;
    for (i = 0; i < n; i++) {
        noc_sla_monitor_update(&sla, test_measurements[i][0],
                               test_measurements[i][1]);
        printf("  Measured: Lat=%d, Thr=%d ? ", sla.current_latency,
               sla.current_throughput);
        if (sla.current_latency > 100 || sla.current_throughput < 50) {
            printf("? VIOLATION\n");
        } else {
            printf("? OK\n");
        }
    }

    printf("\n  Total violations: %llu\n", (unsigned long long)sla.violations);
    printf("  SLA compliance: %.1f%%\n", sla.compliance_ratio * 100.0);

    return 0;
}
