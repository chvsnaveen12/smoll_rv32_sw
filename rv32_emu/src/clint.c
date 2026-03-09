// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Naveen Chavali

#include "defs.h"
#include <string.h>

#define MSIP_OFFSET     0x0
#define MSIP_SIZE       0x04

#define MTIME_CMP_OFFSET 0x4000
#define MTIME_CMP_SIZE  0x08

#define MTIME_OFFSET    0xBFF8
#define MTIME_SIZE      0x08

// returns null if out of range
static uint8_t *clint_get_ptr(clint_td *clint, uint32_t addr, uint32_t len, uint32_t *offset) {
    if (MSIP_OFFSET <= addr && addr + len <= MSIP_OFFSET + MSIP_SIZE) {
        *offset = addr - MSIP_OFFSET;
        return (uint8_t *)&clint->msip;
    } else if (MTIME_CMP_OFFSET <= addr && addr + len <= MTIME_CMP_OFFSET + MTIME_CMP_SIZE) {
        *offset = addr - MTIME_CMP_OFFSET;
        return (uint8_t *)&clint->mtimecmp;
    } else if (MTIME_OFFSET <= addr && addr + len <= MTIME_OFFSET + MTIME_SIZE) {
        *offset = addr - MTIME_OFFSET;
        return (uint8_t *)&clint->mtime;
    }
    return NULL;
}

bool clint_bus_access_func(clint_td *clint, uint32_t addr, bus_access access, uint32_t *val,
                           uint8_t len) {
    uint32_t offset;
    uint8_t *ptr = clint_get_ptr(clint, addr, len, &offset);
    if (!ptr)
        return false;

    if (access == bus_write)
        memcpy(ptr + offset, val, len);
    else
        memcpy(val, ptr + offset, len);

    return true;
}

// mtime ticks every 10 cycles
void clint_update(clint_td *clint, bool *msi, bool *mti) {
    if (clint->cycle++ % 10 == 0)
        clint->mtime++;

    *mti = clint->mtime >= clint->mtimecmp;
    *msi = clint->msip & 0x01;
}
