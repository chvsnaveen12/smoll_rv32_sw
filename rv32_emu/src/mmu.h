#ifndef MMU_H
#define MMU_H

#include <stdint.h>
#include <stdbool.h>

// Forward declaration - full definition in types.h
struct core;

/**
 * MMU state structure - contains all MMU-related data.
 * This struct is embedded in core_td.
 */

#define TLB_ENTRIES 16  // Number of TLB entries

typedef struct tlb_entry {
    uint32_t vpn;           // Virtual page number (top 20 bits of vaddr)
    uint32_t ppn;           // Physical page number
    uint32_t pte;           // Cached PTE for permission checks
    uint16_t asid;          // Address space identifier
    bool valid;             // Entry validity
    bool is_superpage;      // 4 MiB superpage flag
    uint8_t lru;            // LRU counter for replacement
} tlb_entry_td;

typedef struct mmu {
    // Current translation mode (0 = Bare, 1 = SV32)
    uint8_t mode;

    // Cached root page table physical page number
    uint32_t ppn;

    // Address space identifier (for TLB tagging)
    uint16_t asid;

    // Multi-entry TLB
    tlb_entry_td tlb[TLB_ENTRIES];
    uint8_t tlb_lru_counter;    // Global LRU counter

    // Statistics (for debugging/profiling)
    uint64_t translations;
    uint64_t page_faults;
    uint64_t tlb_hits;
    uint64_t tlb_misses;
} mmu_td;

/**
 * Initializes the MMU structure.
 * 
 * @param mmu       Pointer to MMU structure to initialize
 */
void mmu_init(mmu_td *mmu);

/**
 * Updates MMU state from SATP CSR.
 * Should be called when SATP is written.
 * 
 * @param mmu       Pointer to MMU structure
 * @param satp      New SATP value
 */
void mmu_update_satp(mmu_td *mmu, uint32_t satp);

/**
 * Flushes the TLB (called on SFENCE.VMA).
 * 
 * @param mmu       Pointer to MMU structure
 */
void mmu_flush_tlb(mmu_td *mmu);

/**
 * Translates a virtual address to a physical address using SV32 page tables.
 * 
 * @param core      Pointer to the CPU core structure
 * @param vaddr     Virtual address to translate
 * @param access    Type of access (bus_read, bus_write, bus_fetch)
 * @param paddr     Output: translated physical address
 * @return          true if translation succeeded, false if a page fault occurred
 */
bool mmu_translate(struct core *core, uint32_t vaddr, int access, uint32_t *paddr);

/**
 * Performs a memory access through the MMU.
 * Handles privilege checks, address translation, and page fault generation.
 * 
 * @param core      Pointer to the CPU core structure
 * @param addr      Virtual address to access
 * @param access    Type of access (bus_read, bus_write, bus_fetch)
 * @param val       Pointer to value (read into or written from)
 * @param len       Length of access in bytes (1, 2, or 4)
 * @return          true if access succeeded, false if fault occurred
 */
bool mmu_access(struct core *core, uint32_t addr, int access, uint32_t *val, uint8_t len);

#endif
