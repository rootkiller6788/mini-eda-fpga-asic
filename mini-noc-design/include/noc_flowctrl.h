#ifndef NOC_FLOWCTRL_H
#define NOC_FLOWCTRL_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { FLOW_CREDIT, FLOW_WORMHOLE, FLOW_VCT } FlowCtrlType;

typedef struct {
    FlowCtrlType type;
    int credits;       /* for credit-based */
    int max_credits;
    bool tail_sent;    /* for wormhole/VCT */
    int vc_id;
    bool buffer_full;
} FlowCtrlState;

void flowctrl_init(FlowCtrlState *fc, FlowCtrlType type, int max_credits);
bool flowctrl_send(FlowCtrlState *fc, uint32_t flit, bool is_tail);
bool flowctrl_can_send(FlowCtrlState *fc);
void flowctrl_credit_return(FlowCtrlState *fc, int n);
void flowctrl_buffer_occ(FlowCtrlState *fc, int *used, int *free);
void flowctrl_print(FlowCtrlState *fc);
#endif
