/**
 * MMU - Memory Management Unit
 * 
 * Implements SV32 page translation for RISC-V 32-bit systems.
 * Handles virtual to physical address translation with two-level page tables.
 * Includes multi-entry TLB with LRU replacement.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "defs.h"
#include "csr.h"
#include "memory.h"
#include "types.h"
#include "mmu.h"

// Forward declaration for SOC bus access
bool soc_bus_access_func(soc_td *soc, uint32_t addr, bus_access access, uint32_t *val, uint8_t len);

void mmu_init(mmu_td *mmu) {
    memset(mmu, 0, sizeof(mmu_td));
    mmu->mode = 0;  // Bare mode by default
    mmu->tlb_lru_counter = 0;
    
    // Invalidate all TLB entries
    for (int i = 0; i < TLB_ENTRIES; i++) {
        mmu->tlb[i].valid = false;
    }
}

void mmu_update_satp(mmu_td *mmu, uint32_t satp) {
    mmu->mode = (satp >> SATP_MODE_BIT) & 0x1;
    mmu->ppn = (satp & PPN_MASK) << PAGE_SIZE_SHIFT;
    mmu->asid = (satp >> 22) & 0x1ff;
    
    // Invalidate all TLB entries on SATP change
    for (int i = 0; i < TLB_ENTRIES; i++) {
        mmu->tlb[i].valid = false;
    }
}

void mmu_flush_tlb(mmu_td *mmu) {
    for (int i = 0; i < TLB_ENTRIES; i++) {
        mmu->tlb[i].valid = false;
    }
}

/**
 * Looks up address in TLB.
 * 
 * @param mmu       Pointer to MMU structure
 * @param vpn       Virtual page number to look up
 * @param entry_out Output: pointer to matching TLB entry if found
 * @return          Index of matching entry, or -1 if miss
 */
static int tlb_lookup(mmu_td *mmu, uint32_t vpn) {
    for (int i = 0; i < TLB_ENTRIES; i++) {
        if (mmu->tlb[i].valid && mmu->tlb[i].asid == mmu->asid) {
            // Check for exact match (4K page) or superpage match
            if (mmu->tlb[i].is_superpage) {
                // Superpage: only compare VPN[1] (top 10 bits of 20-bit VPN)
                if ((mmu->tlb[i].vpn >> 10) == (vpn >> 10)) {
                    // Update LRU
                    mmu->tlb[i].lru = mmu->tlb_lru_counter++;
                    return i;
                }
            } else {
                // Regular page: compare full VPN
                if (mmu->tlb[i].vpn == vpn) {
                    // Update LRU
                    mmu->tlb[i].lru = mmu->tlb_lru_counter++;
                    return i;
                }
            }
        }
    }
    return -1;
}

/**
 * Finds a TLB entry to evict using LRU policy.
 * 
 * @param mmu       Pointer to MMU structure
 * @return          Index of entry to evict
 */
static int tlb_find_victim(mmu_td *mmu) {
    int victim = 0;
    uint8_t oldest_lru = mmu->tlb[0].lru;
    
    // First, look for invalid entries
    for (int i = 0; i < TLB_ENTRIES; i++) {
        if (!mmu->tlb[i].valid) {
            return i;
        }
        // Track LRU for valid entries
        if ((uint8_t)(mmu->tlb_lru_counter - mmu->tlb[i].lru) > 
            (uint8_t)(mmu->tlb_lru_counter - oldest_lru)) {
            oldest_lru = mmu->tlb[i].lru;
            victim = i;
        }
    }
    return victim;
}

/**
 * Inserts an entry into the TLB.
 * 
 * @param mmu           Pointer to MMU structure
 * @param vpn           Virtual page number
 * @param ppn           Physical page number
 * @param pte           Page table entry (for permission caching)
 * @param is_superpage  True if 4 MiB superpage
 */
static void tlb_insert(mmu_td *mmu, uint32_t vpn, uint32_t ppn, uint32_t pte, bool is_superpage) {
    int idx = tlb_find_victim(mmu);
    
    mmu->tlb[idx].vpn = vpn;
    mmu->tlb[idx].ppn = ppn;
    mmu->tlb[idx].pte = pte;
    mmu->tlb[idx].asid = mmu->asid;
    mmu->tlb[idx].valid = true;
    mmu->tlb[idx].is_superpage = is_superpage;
    mmu->tlb[idx].lru = mmu->tlb_lru_counter++;
}

/**
 * Gets the appropriate fault type for the access type.
 */
static trap_cause_exception mmu_get_fault_type(bus_access access, bool is_page_fault) {
    switch (access) {
        case bus_write:
            return is_page_fault ? str_amo_page_fault : str_amo_access_fault;
        case bus_fetch:
            return is_page_fault ? instr_page_fault : instr_access_fault;
        case bus_read:
        default:
            return is_page_fault ? load_page_fault : load_access_fault;
    }
}

/**
 * Checks page table entry permissions against the access type.
 */
static bool mmu_check_permissions(uint32_t pte, bus_access access, bool mxr) {
    switch (access) {
        case bus_fetch:
            return extract_bits(pte, PTE_X, PTE_X);
        case bus_read:
            return extract_bits(pte, PTE_R, PTE_R) || 
                   (mxr && extract_bits(pte, PTE_X, PTE_X));
        case bus_write:
            return extract_bits(pte, PTE_W, PTE_W);
        default:
            return false;
    }
}

/**
 * Checks privilege access to the page.
 */
static bool mmu_check_privilege(uint32_t pte, priv_level_td priv, bool sum) {
    bool pte_u = extract_bits(pte, PTE_U, PTE_U);

    if (priv == priv_supervisor) {
        if (pte_u && !sum) {
            return false;
        }
    } else if (priv == priv_user) {
        if (!pte_u) {
            return false;
        }
    }

    return true;
}

/**
 * Updates Access (A) and Dirty (D) bits in the page table entry.
 */
static void mmu_update_ad_bits(core_td *core, uint32_t pte_addr, uint32_t pte, bus_access access) {
    if (access == bus_write) {
        pte |= (1 << PTE_A) | (1 << PTE_D);
    } else {
        pte |= (1 << PTE_A);
    }
    soc_bus_access_func(core->soc_ptr, pte_addr, bus_write, &pte, 4);
}

/**
 * Performs SV32 page table walk to translate virtual address to physical.
 * Uses TLB for caching translations.
 */
static bool mmu_page_walk(core_td *core, uint32_t vaddr, bus_access access,
                          uint32_t *paddr, uint32_t *pte_addr_out, uint32_t *pte_out) {
    uint32_t xstatus;
    uint32_t ppn, pte;
    uint32_t vpn[SV32_LEVELS];
    uint32_t offset;
    bool sum, mxr;
    trap_cause_exception access_fault, page_fault;
    int tlb_idx;

    // Read MSTATUS
    csr_read(&core->csr, priv_machine, MSTATUS, &xstatus);
    sum = (bool)extract_bits(xstatus, MSTATUS_SUM_BIT, MSTATUS_SUM_BIT);
    mxr = (bool)extract_bits(xstatus, MSTATUS_MXR_BIT, MSTATUS_MXR_BIT);

    // Get fault types
    access_fault = mmu_get_fault_type(access, false);
    page_fault = mmu_get_fault_type(access, true);

    // Extract virtual page numbers
    uint32_t full_vpn = vaddr >> 12;  // Full 20-bit VPN
    vpn[1] = (vaddr >> 22) & VPN_MASK;
    vpn[0] = (vaddr >> 12) & VPN_MASK;
    offset = vaddr & OFFSET_MASK;

    core->mmu.translations++;

    // TLB lookup
    tlb_idx = tlb_lookup(&core->mmu, full_vpn);
    if (tlb_idx >= 0) {
        // TLB hit!
        tlb_entry_td *entry = &core->mmu.tlb[tlb_idx];
        core->mmu.tlb_hits++;

        // Check permissions using cached PTE
        if (!mmu_check_permissions(entry->pte, access, mxr)) {
            core_prepare_sync_trap(core, page_fault, vaddr);
            core->mmu.page_faults++;
            return false;
        }

        if (!mmu_check_privilege(entry->pte, core->privilege, sum)) {
            core_prepare_sync_trap(core, page_fault, vaddr);
            core->mmu.page_faults++;
            return false;
        }

        // Calculate physical address
        if (entry->is_superpage) {
            *paddr = entry->ppn | (vpn[0] << 12) | offset;
        } else {
            *paddr = entry->ppn | offset;
        }

        // For A/D bit updates, we still need PTE address
        // Store PTE info for potential update
        *pte_out = entry->pte;
        // Note: pte_addr is not cached in TLB, so A/D updates need special handling
        // For now, set to 0 to indicate TLB hit (caller should check)
        *pte_addr_out = 0;

        return true;
    }

    // TLB miss - perform page table walk
    core->mmu.tlb_misses++;
    ppn = core->mmu.ppn;

    for (int i = SV32_LEVELS - 1; i >= 0; i--) {
        uint32_t pte_addr = ppn | (vpn[i] << 2);

        if (!soc_bus_access_func(core->soc_ptr, pte_addr, bus_read, &pte, 4)) {
            core_prepare_sync_trap(core, access_fault, vaddr);
            core->mmu.page_faults++;
            return false;
        }

        if (!extract_bits(pte, PTE_V, PTE_V)) {
            core_prepare_sync_trap(core, page_fault, vaddr);
            core->mmu.page_faults++;
            return false;
        }

        if (!(pte & PTE_RWX_MASK)) {
            ppn = ((pte >> 10) & PPN_MASK) << PAGE_SIZE_SHIFT;
            continue;
        }

        // Found leaf page
        if (!mmu_check_permissions(pte, access, mxr)) {
            core_prepare_sync_trap(core, page_fault, vaddr);
            core->mmu.page_faults++;
            return false;
        }

        if (!mmu_check_privilege(pte, core->privilege, sum)) {
            core_prepare_sync_trap(core, page_fault, vaddr);
            core->mmu.page_faults++;
            return false;
        }

        // Calculate physical address and insert into TLB
        bool is_superpage = (i == 1);
        uint32_t page_ppn;
        
        if (is_superpage) {
            page_ppn = ((pte >> 20) & 0x00000fff) << 22;
            *paddr = page_ppn | (vpn[0] << 12) | offset;
        } else {
            page_ppn = ((pte >> 10) & 0x003fffff) << 12;
            *paddr = page_ppn | offset;
        }

        // Insert into TLB
        tlb_insert(&core->mmu, full_vpn, page_ppn, pte, is_superpage);

        *pte_addr_out = pte_addr;
        *pte_out = pte;

        return true;
    }

    core_prepare_sync_trap(core, page_fault, vaddr);
    core->mmu.page_faults++;
    return false;
}

bool mmu_translate(struct core *core, uint32_t vaddr, int access, uint32_t *paddr) {
    uint32_t pte_addr, pte;
    uint32_t xstatus;
    priv_level_td effective_priv;

    if (core->privilege == priv_machine) {
        csr_read(&core->csr, priv_machine, MSTATUS, &xstatus);

        if (!extract_bits(xstatus, MSTATUS_MPRV_BIT, MSTATUS_MPRV_BIT) || access == bus_fetch) {
            *paddr = vaddr;
            return true;
        }

        effective_priv = (priv_level_td)extract_bits(xstatus, MSTATUS_MPP_BIT + 1, MSTATUS_MPP_BIT);
        if (effective_priv == priv_machine) {
            *paddr = vaddr;
            return true;
        }
    }

    if (core->mmu.mode != SV32_MODE) {
        *paddr = vaddr;
        return true;
    }

    if (!mmu_page_walk(core, vaddr, (bus_access)access, paddr, &pte_addr, &pte)) {
        return false;
    }

    // Update A/D bits only if we did a page walk (pte_addr != 0)
    if (pte_addr != 0) {
        mmu_update_ad_bits(core, pte_addr, pte, (bus_access)access);
    }

    return true;
}

bool mmu_access(struct core *core, uint32_t addr, int access, uint32_t *val, uint8_t len) {
    uint32_t xstatus;
    uint32_t pte_addr, pte;
    uint32_t paddr;
    priv_level_td effective_priv;

    if (core->privilege == priv_machine) {
        csr_read(&core->csr, priv_machine, MSTATUS, &xstatus);

        if (!extract_bits(xstatus, MSTATUS_MPRV_BIT, MSTATUS_MPRV_BIT) || access == bus_fetch) {
            return soc_bus_access_func(core->soc_ptr, addr, (bus_access)access, val, len);
        }

        effective_priv = (priv_level_td)extract_bits(xstatus, MSTATUS_MPP_BIT + 1, MSTATUS_MPP_BIT);

        if (effective_priv == priv_machine) {
            return soc_bus_access_func(core->soc_ptr, addr, (bus_access)access, val, len);
        }
    }

    if (core->mmu.mode != SV32_MODE) {
        return soc_bus_access_func(core->soc_ptr, addr, (bus_access)access, val, len);
    }

    if (!mmu_page_walk(core, addr, (bus_access)access, &paddr, &pte_addr, &pte)) {
        return false;
    }

    // Update A/D bits only if we did a page walk (pte_addr != 0)
    if (pte_addr != 0) {
        mmu_update_ad_bits(core, pte_addr, pte, (bus_access)access);
    }

    return soc_bus_access_func(core->soc_ptr, paddr, (bus_access)access, val, len);
}
