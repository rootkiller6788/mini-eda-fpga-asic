#include "noc_flowctrl.h"
#include <stdio.h>
#include <string.h>

void flowctrl_init(FlowCtrlState *fc, FlowCtrlType type, int max_credits) {
    fc->type = type; fc->max_credits = max_credits; fc->credits = max_credits;
    fc->tail_sent = false; fc->vc_id = 0; fc->buffer_full = false;
}

bool flowctrl_can_send(FlowCtrlState *fc) {
    switch (fc->type) {
        case FLOW_CREDIT: return fc->credits > 0;
        case FLOW_WORMHOLE: return !fc->buffer_full;
        case FLOW_VCT: return !fc->buffer_full;
        default: return false;
    }
}

bool flowctrl_send(FlowCtrlState *fc, uint32_t flit, bool is_tail) {
    (void)flit;
    if (!flowctrl_can_send(fc)) return false;
    switch (fc->type) {
        case FLOW_CREDIT: fc->credits--; break;
        case FLOW_WORMHOLE: if (is_tail) { fc->tail_sent = true; fc->buffer_full = false; } break;
        case FLOW_VCT: if (is_tail) { fc->tail_sent = true; fc->buffer_full = false; } break;
    }
    return true;
}

void flowctrl_credit_return(FlowCtrlState *fc, int n) {
    if (fc->type == FLOW_CREDIT && fc->credits + n <= fc->max_credits) fc->credits += n;
}

void flowctrl_buffer_occ(FlowCtrlState *fc, int *used, int *free) {
    *used = fc->max_credits - fc->credits;
    *free = (fc->credits > 0) ? fc->credits : 0;
}

void flowctrl_print(FlowCtrlState *fc) {
    const char *names[] = {"Credit","Wormhole","VCT"};
    printf("  FlowCtrl: %s, credits=%d/%d, tail=%s\n",
           names[fc->type], fc->credits, fc->max_credits, fc->tail_sent ? "yes" : "no");
}
