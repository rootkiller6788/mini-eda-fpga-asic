#ifndef CHIPLET_UCIE_H
#define CHIPLET_UCIE_H
#include <stdint.h>
#include <stdbool.h>

#define UCIE_FLIT_WIDTH 64
#define UCIE_MAX_LANES 16

typedef enum { UCIE_LINK_DOWN, UCIE_LINK_TRAINING, UCIE_LINK_ACTIVE, UCIE_LINK_ERROR } UcieLinkState;

typedef struct {
    UcieLinkState state;
    int num_lanes; int active_lanes;
    uint64_t flit_tx, flit_rx;
    bool lane_good[UCIE_MAX_LANES];
    uint32_t ber_count;
} UcieLink;

void ucie_link_init(UcieLink *link, int num_lanes);
bool ucie_link_train(UcieLink *link);
bool ucie_send_flit(UcieLink *link, uint64_t flit);
bool ucie_recv_flit(UcieLink *link, uint64_t *flit);
void ucie_error_inject(UcieLink *link, int lane);
const char *ucie_link_state_name(UcieLinkState s);
void ucie_print(UcieLink *link);
#endif
