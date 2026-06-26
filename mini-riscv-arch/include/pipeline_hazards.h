#ifndef PIPELINE_HAZARDS_H
#define PIPELINE_HAZARDS_H

#include <stdint.h>
#include <stdbool.h>
#include "riscv_core.h"

typedef enum {
    FWD_SRC_NONE    = 0,
    FWD_SRC_EX_MEM  = 1,
    FWD_SRC_MEM_WB  = 2
} FwdSel;

typedef enum {
    HAZ_NONE        = 0,
    HAZ_RAW_RS1     = 1,
    HAZ_RAW_RS2     = 2,
    HAZ_LOAD_USE    = 3,
    HAZ_CONTROL     = 4,
    HAZ_TRAP        = 5
} HazardType;

typedef enum {
    BPRED_NOT_TAKEN = 0,
    BPRED_TAKEN     = 1,
    BPRED_BTB       = 2
} BranchPrediction;

typedef struct {
    HazardType       type;
    bool             stall_if;
    bool             stall_id;
    bool             flush_if;
    bool             flush_id;
    bool             flush_ex;

    FwdSel           rs1_fwd;
    FwdSel           rs2_fwd;
    uint32_t         fwd_ex_val;
    uint32_t         fwd_wb_val;

    bool             branch_resolved;
    bool             branch_actual_taken;
    uint32_t         branch_target_pc;
} HazardInfo;

typedef struct {
    BranchPrediction strategy;
    bool             predict_taken;
    uint32_t         btb_entries;
    uint32_t         btb_pc[64];
    uint32_t         btb_target[64];
    uint32_t         pred_count;
    uint32_t         mispred_count;
    uint32_t         btb_hits;
    uint32_t         btb_misses;
} BranchPredictor;

void hazard_unit_init(HazardInfo *hz);
void hazard_detect(const RiscvCore *core, HazardInfo *hz);
void hazard_resolve(RiscvCore *core, const HazardInfo *hz);
void hazard_apply_stall(RiscvCore *core, const HazardInfo *hz);
void hazard_apply_flush(RiscvCore *core, const HazardInfo *hz);

FwdSel find_forward_src(const RiscvCore *core, uint8_t rs, uint32_t *fwd_val);
bool   detect_raw_hazard(const RiscvCore *core, uint8_t rs, HazardType *haz);
bool   detect_load_use_hazard(const RiscvCore *core);
bool   detect_control_hazard(const RiscvCore *core, HazardInfo *hz);

void   forwarding_mux(const RiscvCore *core, uint8_t rs, FwdSel fwd_sel,
                      uint32_t *out, const uint32_t *rf_val);

void   bpred_init(BranchPredictor *bp);
void   bpred_predict(BranchPredictor *bp, uint32_t pc, uint32_t *target);
void   bpred_update(BranchPredictor *bp, uint32_t pc, uint32_t target,
                    bool taken);
bool   bpred_is_taken(const BranchPredictor *bp);

void   pipeline_flush_if(RiscvCore *core);
void   pipeline_flush_id(RiscvCore *core);
void   pipeline_flush_ex(RiscvCore *core);
void   pipeline_insert_bubble(RiscvCore *core);

uint32_t hazard_get_forward_value(const RiscvCore *core, FwdSel fwd);
bool     hazard_requires_stall(const HazardInfo *hz);
bool     hazard_requires_flush(const HazardInfo *hz);

#endif
