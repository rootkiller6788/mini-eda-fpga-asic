#ifndef RISCV_MMU_H
#define RISCV_MMU_H
#include <stdint.h>
#include <stdbool.h>

#define SATP_MODE_BARE  0
#define SATP_MODE_SV32  1
#define SATP_MODE_SV39  8

#define PTE_V 0x01
#define PTE_R 0x02
#define PTE_W 0x04
#define PTE_X 0x08
#define PTE_U 0x10
#define PTE_G 0x20
#define PTE_A 0x40
#define PTE_D 0x80

#define TLB_ENTRIES 64

typedef struct { uint32_t vpn; uint32_t ppn; bool valid; uint8_t flags; uint8_t asid; } TlbEntry;
typedef struct { TlbEntry entries[TLB_ENTRIES]; int count; int next_victim; } TLB;

typedef struct { uint32_t entries[1024]; } PageTable;

typedef struct {
    uint32_t satp;
    uint8_t mode;
    PageTable *root_pt;
    TLB itlb, dtlb;
    uint32_t tlb_hits, tlb_misses;
} MmuState;

void mmu_init(MmuState *mmu);
void mmu_set_mode(MmuState *mmu, uint8_t mode, uint32_t root_ppn);
uint32_t mmu_translate(MmuState *mmu, uint32_t vaddr, bool is_write, bool is_exec);
uint32_t mmu_walk_pt(MmuState *mmu, uint32_t vaddr, int levels);
bool mmu_tlb_lookup(TLB *tlb, uint32_t vpn, uint32_t *ppn, uint8_t *flags);
void mmu_tlb_insert(TLB *tlb, uint32_t vpn, uint32_t ppn, uint8_t flags);
void mmu_tlb_flush(TLB *tlb);
bool mmu_check_perms(uint8_t pte_flags, bool is_write, bool is_exec, bool is_user);
bool pmp_check(uint32_t addr, bool is_write, uint8_t mode);
void mmu_print(MmuState *mmu);
#endif
