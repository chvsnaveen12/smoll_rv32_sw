// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Naveen Chavali

#include "defs.h"
#include <stdio.h>
#include <stdlib.h>

#define PRIO_OFFSET (0x0 >> 2)
#define PRIO_SIZE (0x80 >> 2)

#define PENDING_OFFSET (0x1000 >> 2)
#define PENDING_SIZE (0x04 >> 2)

#define ENABLE0_OFFSET (0x2000 >> 2)
#define ENABLE0_SIZE (0x04 >> 2)

#define ENABLE1_OFFSET (0x2080 >> 2)
#define ENABLE1_SIZE ENABLE0_SIZE

#define PRIO0_THRESH_OFFSET (0x200000 >> 2)
#define PRIO0_THRESH_SIZE (0x04 >> 2)

#define PRIO1_THRESH_OFFSET (0x201000 >> 2)
#define PRIO1_THRESH_SIZE (0x04 >> 2)

#define CLAIM0_COMPLETE_OFFSET (0x200004 >> 2)
#define CLAIM0_COMPLETE_SIZE (0x04 >> 2)

#define CLAIM1_COMPLETE_OFFSET (0x201004 >> 2)
#define CLAIM1_COMPLETE_SIZE CLAIM0_COMPLETE_SIZE

void plic_update_pending(plic_td *plic, uint32_t id, bool val) {
    uint32_t reg = id / 32;
    uint32_t mask = 1u << (id % 32);
    // skip if claimed
    if (val && !(plic->claimed[reg] & mask))
        plic->pending[reg] |= mask;
    else
        plic->pending[reg] &= ~mask;
}

void plic_init(plic_td *plic) {
    (void)plic;
}

// all priorities are 1 and threshold is 0 so first enabled and pending wins
void plic_update(plic_td *plic, bool *mei, bool *sei) {
    uint32_t irq0 = 0, irq1 = 0;

    for (uint32_t j = 1; j < 32; j++) {
        uint32_t bit = 1u << j;
        if (!irq0 && (plic->enable0[0] & bit) && (plic->pending[0] & bit))
            irq0 = j;
        if (!irq1 && (plic->enable1[0] & bit) && (plic->pending[0] & bit))
            irq1 = j;
    }

    plic->claim_complete0 = irq0;
    *mei = irq0 > 0;
    plic->claim_complete1 = irq1;
    *sei = irq1 > 0;
}

bool plic_bus_access_func(plic_td *plic, uint32_t addr, bus_access access, uint32_t *val,
                          uint8_t len) {

    if (addr % 4 != 0 || len != 4) {
        printf("PLIC addr misaligned or length != 4, dying uwu\n");
        exit(-1);
    }

    addr = addr >> 2;

    if (access == bus_write) {
        if (PRIO_OFFSET <= addr && addr < PRIO_OFFSET + PRIO_SIZE)
            ; // fixed at 1

        else if (PENDING_OFFSET <= addr && addr < PENDING_OFFSET + PENDING_SIZE)
            plic->pending[addr - PENDING_OFFSET] = *val;

        else if (ENABLE0_OFFSET <= addr && addr < ENABLE0_OFFSET + ENABLE0_SIZE)
            plic->enable0[addr - ENABLE0_OFFSET] = *val;
        else if (ENABLE1_OFFSET <= addr && addr < ENABLE1_OFFSET + ENABLE1_SIZE)
            plic->enable1[addr - ENABLE1_OFFSET] = *val;

        else if (PRIO0_THRESH_OFFSET <= addr && addr < PRIO0_THRESH_OFFSET + PRIO0_THRESH_SIZE)
            ; // fixed at 0
        else if (PRIO1_THRESH_OFFSET <= addr && addr < PRIO1_THRESH_OFFSET + PRIO1_THRESH_SIZE)
            ; // fixed at 0

        else if (CLAIM0_COMPLETE_OFFSET <= addr &&
                 addr < CLAIM0_COMPLETE_OFFSET + CLAIM0_COMPLETE_SIZE) {
            uint32_t reg = *val / 32;
            uint32_t bit = *val % 32;
            plic->claimed[reg] &= ~(1u << bit);
        } else if (CLAIM1_COMPLETE_OFFSET <= addr &&
                   addr < CLAIM1_COMPLETE_OFFSET + CLAIM1_COMPLETE_SIZE) {
            uint32_t reg = *val / 32;
            uint32_t bit = *val % 32;
            plic->claimed[reg] &= ~(1u << bit);
        }
    } else {
        if (PRIO_OFFSET <= addr && addr < PRIO_OFFSET + PRIO_SIZE)
            *val = 1; // fixed at 1

        else if (PENDING_OFFSET <= addr && addr < PENDING_OFFSET + PENDING_SIZE)
            *val = plic->pending[addr - PENDING_OFFSET];

        else if (ENABLE0_OFFSET <= addr && addr < ENABLE0_OFFSET + ENABLE0_SIZE)
            *val = plic->enable0[addr - ENABLE0_OFFSET];
        else if (ENABLE1_OFFSET <= addr && addr < ENABLE1_OFFSET + ENABLE1_SIZE)
            *val = plic->enable1[addr - ENABLE1_OFFSET];

        else if (PRIO0_THRESH_OFFSET <= addr && addr < PRIO0_THRESH_OFFSET + PRIO0_THRESH_SIZE)
            *val = 0; // fixed at 0
        else if (PRIO1_THRESH_OFFSET <= addr && addr < PRIO1_THRESH_OFFSET + PRIO1_THRESH_SIZE)
            *val = 0; // fixed at 0

        else if (CLAIM0_COMPLETE_OFFSET <= addr &&
                 addr < CLAIM0_COMPLETE_OFFSET + CLAIM0_COMPLETE_SIZE) {
            *val = plic->claim_complete0;
            uint32_t reg = *val / 32;
            uint32_t bit = *val % 32;
            plic->claimed[reg] |= 1u << bit;
        } else if (CLAIM1_COMPLETE_OFFSET <= addr &&
                   addr < CLAIM1_COMPLETE_OFFSET + CLAIM1_COMPLETE_SIZE) {
            *val = plic->claim_complete1;
            uint32_t reg = *val / 32;
            uint32_t bit = *val % 32;
            plic->claimed[reg] |= 1u << bit;
        }
    }
    return true;
}
