#include "vc_wormhole.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    noc_vc_port_t in_port, out_port;
    noc_credit_counter_t credits;
    noc_flit_t flits[5], out_flit;
    int i;

    printf("=== Virtual Channel & Wormhole Example ===\n\n");

    noc_vc_port_init(&in_port);
    noc_vc_port_init(&out_port);
    noc_credit_init(&credits, 8);

    printf("Allocating VC on input port...\n");
    int in_vc = noc_vc_allocate(&in_port);
    printf("  in_vc = %d\n", in_vc);

    printf("\nInjecting 5-flit packet (HEAD BODY BODY BODY TAIL):\n");
    for (i = 0; i < 5; i++) {
        noc_flit_type_t t = (i == 0) ? NOC_FLIT_HEAD :
                            (i == 4) ? NOC_FLIT_TAIL : NOC_FLIT_BODY;
        noc_flit_init(&flits[i], t, 0, 15, i);
        flits[i].vc_id = in_vc;
        if (!noc_vc_buffer_push(&in_port.vc[in_vc], &flits[i])) {
            printf("  FAILED push flit %d\n", i);
        } else {
            printf("  Pushed flit[%d] %s\n", i, noc_flit_type_name(flits[i].type));
        }
    }

    printf("\nFlit injection complete.\n");
    noc_buffer_status(&in_port.vc[in_vc]);

    printf("\nForwarding via wormhole switching...\n");
    int out_vc = -1;
    while (noc_vc_buffer_peek(&in_port.vc[in_vc], &out_flit)) {
        printf("  Forwarding: vc=%d ", in_vc);
        noc_flit_dump(&out_flit);
        if (!noc_wormhole_forward_flit(&in_port, &out_port, in_vc, &out_vc, &credits)) {
            printf("  Forward stall (no credits)\n");
            break;
        }
    }

    printf("\nOutput port status:\n");
    for (i = 0; i < VC_MAX; i++) {
        printf("  VC[%d]: ", i);
        noc_buffer_status(&out_port.vc[i]);
    }

    printf("\nReading forwarded flits from output port...\n");
    for (i = 0; i < VC_MAX; i++) {
        while (noc_vc_buffer_pop(&out_port.vc[i], &out_flit)) {
            printf("  Read: vc=%d ", i);
            noc_flit_dump(&out_flit);
        }
    }

    printf("\nCredits: %d/%d\n", credits.credits, credits.max_credits);

    noc_vc_release(&in_port, in_vc);
    noc_vc_release(&out_port, out_vc);
    printf("\nDone.\n");
    return 0;
}
