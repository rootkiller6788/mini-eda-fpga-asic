#include "axi_protocol.h"
#include "noc_topology.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    axi_aw_channel_t aw;
    axi_w_channel_t  w;
    axi_b_channel_t  b;
    axi_ar_channel_t ar;
    axi_r_channel_t  r;
} axi_bridge_t;

static void axi_write_burst(axi_bridge_t *b, uint64_t addr, const uint64_t *data, int beats) {
    int i;
    printf("--- AXI Write Burst: addr=0x%llX len=%d ---\n",
        (unsigned long long)addr, beats);

    b->aw.addr  = addr;
    b->aw.len   = (uint8_t)(beats - 1);
    b->aw.size  = 3;
    b->aw.burst = AXI_BURST_INCR;
    b->aw.id    = 0;
    b->aw.valid = true;
    b->aw.ready = true;

    if (axi_aw_handshake(&b->aw)) {
        axi_aw_dump(&b->aw);
        axi_inc_outstanding();
    }

    for (i = 0; i < beats; i++) {
        b->w.data  = data[i];
        b->w.strb  = 0xFF;
        b->w.last  = (uint8_t)(i == beats - 1);
        b->w.id    = 0;
        b->w.valid = true;
        b->w.ready = true;

        if (axi_w_handshake(&b->w)) {
            printf("  W[%d]: data=0x%llX last=%d\n",
                i, (unsigned long long)data[i], i == beats - 1);
        }
    }

    b->b.id    = 0;
    b->b.resp  = AXI_RESP_OKAY;
    b->b.valid = true;
    b->b.ready = true;

    if (axi_b_handshake(&b->b)) {
        axi_b_dump(&b->b);
        axi_dec_outstanding();
    }
}

static void axi_read_burst(axi_bridge_t *b, uint64_t addr, int beats, uint64_t *out) {
    int i;
    printf("--- AXI Read Burst: addr=0x%llX len=%d ---\n",
        (unsigned long long)addr, beats);

    b->ar.addr  = addr;
    b->ar.len   = (uint8_t)(beats - 1);
    b->ar.size  = 3;
    b->ar.burst = AXI_BURST_INCR;
    b->ar.id    = 1;
    b->ar.valid = true;
    b->ar.ready = true;

    if (axi_ar_handshake(&b->ar)) {
        axi_ar_dump(&b->ar);
        axi_inc_outstanding();
    }

    for (i = 0; i < beats; i++) {
        b->r.data  = out[i];
        b->r.id    = 1;
        b->r.resp  = AXI_RESP_OKAY;
        b->r.last  = (uint8_t)(i == beats - 1);
        b->r.valid = true;
        b->r.ready = true;

        if (axi_r_handshake(&b->r)) {
            printf("  R[%d]: data=0x%llX resp=%s last=%d\n",
                i, (unsigned long long)out[i], axi_resp_name(AXI_RESP_OKAY), i == beats - 1);
        }
    }
    axi_dec_outstanding();
}

int main(void) {
    axi_bridge_t bridge;
    noc_topology_t topo;
    uint64_t write_data[4] = {0xDEADBEEF, 0xCAFEBABE, 0xFEEDFACE, 0xABCDEF01};
    uint64_t read_data[4]  = {0x11111111, 0x22222222, 0x33333333, 0x44444444};

    memset(&bridge, 0, sizeof(bridge));
    axi_aw_init(&bridge.aw);
    axi_w_init(&bridge.w);
    axi_b_init(&bridge.b);
    axi_ar_init(&bridge.ar);
    axi_r_init(&bridge.r);

    noc_topology_init(&topo, NOC_TOPO_MESH, 4, 4);

    printf("=== AXI-NoC Bridge Example ===\n\n");

    axi_write_burst(&bridge, 0x1000, write_data, 4);
    printf("Outstanding: %d\n\n", axi_outstanding_count());

    axi_read_burst(&bridge, 0x2000, 4, read_data);
    printf("Outstanding: %d\n\n", axi_outstanding_count());

    printf("QoS values:\n");
    printf("  LOW=%d MEDIUM=%d HIGH=%d CRITICAL=%d\n",
        AXI_QOS_LOW, AXI_QOS_MEDIUM, AXI_QOS_HIGH, AXI_QOS_CRITICAL);

    printf("\nTopology: %s\n", noc_topology_name(NOC_TOPO_MESH));
    noc_topology_destroy(&topo);

    printf("\nDone.\n");
    return 0;
}
