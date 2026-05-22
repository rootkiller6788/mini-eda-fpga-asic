#include <string.h>
#include <stdio.h>
#include "pipeline_hazards.h"

void hazard_unit_init(HazardInfo *hz) {
    memset(hz, 0, sizeof(HazardInfo));
}

void bpred_init(BranchPredictor *bp) {
    memset(bp, 0, sizeof(BranchPredictor));
    bp->strategy = BPRED_NOT_TAKEN;
}

bool bpred_is_taken(const BranchPredictor *bp) {
    if (!bp) return false;
    return bp->predict_taken;
}

void bpred_predict(BranchPredictor *bp, uint32_t pc, uint32_t *target) {
    if (!bp || !target) return;

    switch (bp->strategy) {
    case BPRED_NOT_TAKEN:
        bp->predict_taken = false;
        *target = pc + 4;
        break;
    case BPRED_TAKEN:
        bp->predict_taken = true;
        *target = pc + 4;
        break;
    case BPRED_BTB:
        for (uint32_t i = 0; i < bp->btb_entries && i < 64; i++) {
            if (bp->btb_pc[i] == pc) {
                bp->predict_taken = true;
                bp->btb_hits++;
                *target = bp->btb_target[i];
                return;
            }
        }
        bp->predict_taken = false;
        bp->btb_misses++;
        *target = pc + 4;
        break;
    }
    bp->pred_count++;
}

void bpred_update(BranchPredictor *bp, uint32_t pc, uint32_t target,
                  bool taken) {
    if (!bp) return;

    if (bp->predict_taken != taken)
        bp->mispred_count++;

    if (bp->strategy == BPRED_BTB) {
        uint32_t idx = bp->btb_entries % 64;
        bp->btb_pc[idx]     = pc;
        bp->btb_target[idx] = target;
        if (bp->btb_entries < 64) bp->btb_entries++;
    }
}

FwdSel find_forward_src(const RiscvCore *core, uint8_t rs, uint32_t *fwd_val) {
    if (rs == 0) return FWD_SRC_NONE;

    if (core->ex_mem.valid && core->ex_mem.wb_en &&
        core->ex_mem.rd != 0 && core->ex_mem.rd == rs) {
        if (fwd_val) *fwd_val = core->ex_mem.alu_result;
        return FWD_SRC_EX_MEM;
    }

    if (core->mem_wb.valid && core->mem_wb.wb_en &&
        core->mem_wb.rd != 0 && core->mem_wb.rd == rs) {
        if (fwd_val) *fwd_val = core->mem_wb.alu_result;
        return FWD_SRC_MEM_WB;
    }

    return FWD_SRC_NONE;
}

uint32_t hazard_get_forward_value(const RiscvCore *core, FwdSel fwd) {
    switch (fwd) {
    case FWD_SRC_EX_MEM: return core->ex_mem.alu_result;
    case FWD_SRC_MEM_WB: return core->mem_wb.alu_result;
    default:             return 0;
    }
}

void forwarding_mux(const RiscvCore *core, uint8_t rs, FwdSel fwd_sel,
                    uint32_t *out, const uint32_t *rf_val) {
    switch (fwd_sel) {
    case FWD_SRC_EX_MEM:
        *out = core->ex_mem.alu_result;
        break;
    case FWD_SRC_MEM_WB:
        *out = core->mem_wb.alu_result;
        break;
    default:
        *out = regfile_read(&core->rf, rs);
        break;
    }
    (void)rf_val;
}

bool detect_raw_hazard(const RiscvCore *core, uint8_t rs, HazardType *haz) {
    if (rs == 0) return false;

    if (core->ex_mem.valid && core->ex_mem.wb_en &&
        core->ex_mem.rd != 0 && core->ex_mem.rd == rs) {
        if (core->ex_mem.is_load)
            *haz = HAZ_LOAD_USE;
        else
            *haz = HAZ_RAW_RS1;
        return true;
    }

    if (core->mem_wb.valid && core->mem_wb.wb_en &&
        core->mem_wb.rd != 0 && core->mem_wb.rd == rs) {
        *haz = HAZ_RAW_RS1;
        return true;
    }

    return false;
}

bool detect_load_use_hazard(const RiscvCore *core) {
    if (!core->id_ex.valid || !core->id_ex.is_load) return false;

    uint8_t load_rd = core->id_ex.rd;
    if (load_rd == 0) return false;

    if (!core->id_ex.valid) return false;

    if (core->id_ex.rs1 == load_rd || core->id_ex.rs2 == load_rd)
        return true;

    return false;
}

bool detect_control_hazard(const RiscvCore *core, HazardInfo *hz) {
    if (!core->id_ex.valid || !core->id_ex.is_branch) return false;

    uint32_t op_a = core->id_ex.rs1_data;
    uint32_t op_b = core->id_ex.rs2_data;

    FwdSel fwd_a = find_forward_src(core, core->id_ex.rs1, NULL);
    FwdSel fwd_b = find_forward_src(core, core->id_ex.rs2, NULL);

    if (fwd_a != FWD_SRC_NONE)
        op_a = hazard_get_forward_value(core, fwd_a);
    if (fwd_b != FWD_SRC_NONE)
        op_b = hazard_get_forward_value(core, fwd_b);

    bool taken = branch_taken(core->id_ex.funct3, op_a, op_b);
    uint32_t target = core->id_ex.pc + (uint32_t)core->id_ex.imm;

    hz->branch_resolved     = true;
    hz->branch_actual_taken = taken;
    hz->branch_target_pc    = taken ? target : core->id_ex.pc + 4;
    hz->type                = HAZ_CONTROL;

    return taken;
}

void hazard_detect(const RiscvCore *core, HazardInfo *hz) {
    hz->type    = HAZ_NONE;
    hz->stall_if = false;
    hz->stall_id = false;
    hz->flush_if = false;
    hz->flush_id = false;
    hz->flush_ex = false;
    hz->rs1_fwd  = FWD_SRC_NONE;
    hz->rs2_fwd  = FWD_SRC_NONE;

    HazardType haz_rs1 = HAZ_NONE;
    HazardType haz_rs2 = HAZ_NONE;

    if (core->id_ex.valid) {
        if (detect_raw_hazard(core, core->id_ex.rs1, &haz_rs1)) {
            hz->rs1_fwd = find_forward_src(core, core->id_ex.rs1,
                                           &hz->fwd_ex_val);
        }
        if (detect_raw_hazard(core, core->id_ex.rs2, &haz_rs2)) {
            hz->rs2_fwd = find_forward_src(core, core->id_ex.rs2,
                                           &hz->fwd_wb_val);
        }

        if (haz_rs1 == HAZ_LOAD_USE || haz_rs2 == HAZ_LOAD_USE) {
            hz->type = HAZ_LOAD_USE;
            hz->stall_if = true;
            hz->stall_id = true;
            hz->flush_ex = false;
            return;
        }
    }

    if (core->id_ex.valid && core->id_ex.is_branch) {
        if (detect_control_hazard(core, hz)) {
            hz->type = HAZ_CONTROL;
            hz->flush_if = true;
            hz->flush_id = true;
            hz->flush_ex = true;
        }
    }
}

bool hazard_requires_stall(const HazardInfo *hz) {
    return hz->stall_if || hz->stall_id;
}

bool hazard_requires_flush(const HazardInfo *hz) {
    return hz->flush_if || hz->flush_id || hz->flush_ex;
}

void hazard_apply_stall(RiscvCore *core, const HazardInfo *hz) {
    core->stall_if = hz->stall_if;
    core->stall_id = hz->stall_id;
}

void hazard_apply_flush(RiscvCore *core, const HazardInfo *hz) {
    if (hz->flush_if) pipeline_flush_if(core);
    if (hz->flush_id) pipeline_flush_id(core);
    if (hz->flush_ex) pipeline_flush_ex(core);
}

void hazard_resolve(RiscvCore *core, const HazardInfo *hz) {
    hazard_apply_stall(core, hz);
    hazard_apply_flush(core, hz);
}

void pipeline_flush_if(RiscvCore *core) {
    core->if_id.valid = false;
    core->if_id.pc    = 0;
    core->if_id.inst  = 0;
}

void pipeline_flush_id(RiscvCore *core) {
    core->id_ex.valid = false;
}

void pipeline_flush_ex(RiscvCore *core) {
    core->ex_mem.valid = false;
    core->flush_ex = true;
}

void pipeline_insert_bubble(RiscvCore *core) {
    core->id_ex.valid = false;
}

void hazard_unit_dump(const HazardInfo *hz) {
    printf("=== Hazard Detection ===\n");
    printf("Type:            ");
    switch (hz->type) {
    case HAZ_NONE:     printf("NONE\n"); break;
    case HAZ_RAW_RS1:  printf("RAW_RS1\n"); break;
    case HAZ_RAW_RS2:  printf("RAW_RS2\n"); break;
    case HAZ_LOAD_USE: printf("LOAD_USE\n"); break;
    case HAZ_CONTROL:  printf("CONTROL\n"); break;
    case HAZ_TRAP:     printf("TRAP\n"); break;
    default:           printf("UNKNOWN\n"); break;
    }
    printf("Stall IF/ID:     %d / %d\n", hz->stall_if, hz->stall_id);
    printf("Flush IF/ID/EX:  %d / %d / %d\n",
           hz->flush_if, hz->flush_id, hz->flush_ex);
    printf("Forward RS1/RS2: %d / %d\n", hz->rs1_fwd, hz->rs2_fwd);
}
