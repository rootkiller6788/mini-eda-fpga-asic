#include "riscv_mmu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void mmu_init(MmuState *mmu) {
    memset(mmu, 0, sizeof(*mmu));
    mmu->mode = SATP_MODE_BARE;
    mmu->itlb.count = 0; mmu->itlb.next_victim = 0;
    mmu->dtlb.count = 0; mmu->dtlb.next_victim = 0;
}

void mmu_set_mode(MmuState *mmu, uint8_t mode, uint32_t root_ppn) {
    mmu->mode = mode;
    mmu->satp = (root_ppn & 0x003FFFFF) | ((uint32_t)mode << 31);
    mmu_tlb_flush(&mmu->itlb);
    mmu_tlb_flush(&mmu->dtlb);
}

bool mmu_tlb_lookup(TLB *tlb, uint32_t vpn, uint32_t *ppn, uint8_t *flags) {
    for (int i = 0; i < tlb->count; i++) {
        if (tlb->entries[i].valid && tlb->entries[i].vpn == vpn) {
            *ppn = tlb->entries[i].ppn;
            *flags = tlb->entries[i].flags;
            return true;
        }
    }
    return false;
}

void mmu_tlb_insert(TLB *tlb, uint32_t vpn, uint32_t ppn, uint8_t flags) {
    if (tlb->count < TLB_ENTRIES) {
        tlb->entries[tlb->count].vpn = vpn;
        tlb->entries[tlb->count].ppn = ppn;
        tlb->entries[tlb->count].flags = flags;
        tlb->entries[tlb->count].valid = true;
        tlb->entries[tlb->count].asid = 0;
        tlb->count++;
    } else {
        int idx = tlb->next_victim;
        tlb->entries[idx].vpn = vpn;
        tlb->entries[idx].ppn = ppn;
        tlb->entries[idx].flags = flags;
        tlb->entries[idx].valid = true;
        tlb->entries[idx].asid = 0;
        tlb->next_victim = (tlb->next_victim + 1) % TLB_ENTRIES;
    }
}

void mmu_tlb_flush(TLB *tlb) {
    for (int i = 0; i < TLB_ENTRIES; i++) tlb->entries[i].valid = false;
    tlb->count = 0; tlb->next_victim = 0;
}

uint32_t mmu_walk_pt(MmuState *mmu, uint32_t vaddr, int levels) { (void)levels;
    if (!mmu->root_pt) return UINT32_MAX;
    if (mmu->mode == SATP_MODE_SV32) {
        uint32_t vpn1 = (vaddr >> 22) & 0x3FF;
        uint32_t vpn0 = (vaddr >> 12) & 0x3FF;
        uint32_t offset = vaddr & 0xFFF;
        uint32_t l1_pte = mmu->root_pt->entries[vpn1];
        if (!(l1_pte & PTE_V)) return UINT32_MAX;
        uint32_t ppn1 = (l1_pte >> 10) & 0x003FFFFF;
        (void)ppn1;
        uint32_t ppn0 = vpn0;
        return (ppn0 << 12) | offset;
    }
    return vaddr;
}

uint32_t mmu_translate(MmuState *mmu, uint32_t vaddr, bool is_write, bool is_exec) { (void)is_write;
    if (mmu->mode == SATP_MODE_BARE) return vaddr;
    TLB *tlb = is_exec ? &mmu->itlb : &mmu->dtlb;
    uint32_t vpn = vaddr >> 12;
    uint32_t ppn; uint8_t flags;
    if (mmu_tlb_lookup(tlb, vpn, &ppn, &flags)) {
        mmu->tlb_hits++;
        return (ppn << 12) | (vaddr & 0xFFF);
    }
    mmu->tlb_misses++;
    uint32_t paddr = mmu_walk_pt(mmu, vaddr, 2);
    if (paddr == UINT32_MAX) return UINT32_MAX;
    mmu_tlb_insert(tlb, vpn, paddr >> 12, PTE_V | PTE_R | PTE_W | PTE_X);
    return paddr;
}

bool mmu_check_perms(uint8_t pte_flags, bool is_write, bool is_exec, bool is_user) {
    if (!(pte_flags & PTE_V)) return false;
    if (is_write && !(pte_flags & PTE_W)) return false;
    if (is_exec && !(pte_flags & PTE_X)) return false;
    if (is_user && !(pte_flags & PTE_U)) return false;
    return true;
}

bool pmp_check(uint32_t addr, bool is_write, uint8_t mode) {
    (void)addr; (void)is_write;
    return (mode == 3); /* M-mode bypasses PMP */
}

void mmu_print(MmuState *mmu) {
    printf("=== MMU State ===\n");
    printf("  Mode: %s\n", mmu->mode == SATP_MODE_BARE ? "Bare" : mmu->mode == SATP_MODE_SV32 ? "Sv32" : mmu->mode == SATP_MODE_SV39 ? "Sv39" : "?");
    printf("  SATP: 0x%08X\n", mmu->satp);
    printf("  TLB hits: %u, misses: %u\n", mmu->tlb_hits, mmu->tlb_misses);
    printf("  ITLB entries: %d, DTLB entries: %d\n", mmu->itlb.count, mmu->dtlb.count);
}
