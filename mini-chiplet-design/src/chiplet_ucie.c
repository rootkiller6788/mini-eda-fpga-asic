#include "chiplet_ucie.h"
#include <stdio.h>
#include <string.h>

void ucie_link_init(UcieLink *link, int num_lanes) {
    memset(link, 0, sizeof(*link));
    link->state = UCIE_LINK_DOWN;
    link->num_lanes = num_lanes > UCIE_MAX_LANES ? UCIE_MAX_LANES : num_lanes;
    link->active_lanes = 0;
    for (int i = 0; i < link->num_lanes; i++) link->lane_good[i] = true;
}

const char *ucie_link_state_name(UcieLinkState s) {
    switch (s) { case UCIE_LINK_DOWN: return "Down"; case UCIE_LINK_TRAINING: return "Training"; case UCIE_LINK_ACTIVE: return "Active"; case UCIE_LINK_ERROR: return "Error"; default: return "?"; }
}

bool ucie_link_train(UcieLink *link) {
    link->state = UCIE_LINK_TRAINING;
    int good = 0;
    for (int i = 0; i < link->num_lanes; i++) if (link->lane_good[i]) good++;
    link->active_lanes = good;
    link->state = good > 0 ? UCIE_LINK_ACTIVE : UCIE_LINK_ERROR;
    return link->state == UCIE_LINK_ACTIVE;
}

bool ucie_send_flit(UcieLink *link, uint64_t flit) {
    if (link->state != UCIE_LINK_ACTIVE) return false;
    link->flit_tx++;
    (void)flit;
    return true;
}

bool ucie_recv_flit(UcieLink *link, uint64_t *flit) {
    if (link->state != UCIE_LINK_ACTIVE) return false;
    link->flit_rx++;
    if (flit) *flit = 0;
    return true;
}

void ucie_error_inject(UcieLink *link, int lane) {
    if (lane < link->num_lanes) { link->lane_good[lane] = false; link->ber_count++; }
}

void ucie_print(UcieLink *link) {
    printf("=== UCIe Link ===\n");
    printf("  State: %s, Active lanes: %d/%d\n", ucie_link_state_name(link->state), link->active_lanes, link->num_lanes);
    printf("  Flits: TX=%lu RX=%lu BER errors: %u\n", (unsigned long)link->flit_tx, (unsigned long)link->flit_rx, link->ber_count);
}
