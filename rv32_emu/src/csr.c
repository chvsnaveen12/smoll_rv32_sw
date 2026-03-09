// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Naveen Chavali

#include "csr.h"
#include "defs.h"

static inline uint32_t write_with_bitmask(uint32_t initial_val, uint32_t new_val,
                                          uint32_t bitmask) {
    return (initial_val & ~bitmask) | (new_val & bitmask);
}

// Returns false for unrecognized or privilege-inaccessible CSR addresses. Caller treats this as an
// illegal instruction.
bool csr_read(csr_td *csr, priv_level_td privilege, uint32_t addr, uint32_t *val) {
    if (privilege == priv_machine) {
        if (addr == MVENDORID || addr == MARCHID || addr == MIMPID || addr == MHARTID ||
            addr == MCONFIGPTR) {
            *val = 0;
            return true;
        }

        switch (addr) {
        case MSTATUS:
            *val = csr->mstatus & MSTATUS_MASK;
            return true;
        case MSTATUSH:
            *val = 0;
            return true;
        case MISA:
            *val = MISA_DEFAULT;
            return true;
        case MEDELEG:
            *val = csr->medeleg;
            return true;
        case MIDELEG:
            *val = csr->mideleg;
            return true;
        case MIE:
            *val = csr->mie;
            return true;
        case MTVEC:
            *val = csr->mtvec;
            return true;
        case MSCRATCH:
            *val = csr->mscratch;
            return true;
        case MEPC:
            *val = csr->mepc;
            return true;
        case MCAUSE:
            *val = csr->mcause;
            return true;
        case MTVAL:
            *val = csr->mtval;
            return true;
        case MIP:
            *val = csr->mip;
            return true;
        }
    }

    if (privilege == priv_machine || privilege == priv_supervisor) {
        switch (addr) {
        case SSTATUS:
            *val = csr->mstatus & SSTATUS_MASK;
            return true;
        case SIE:
            *val = csr->mie & csr->mideleg;
            return true;
        case STVEC:
            *val = csr->stvec;
            return true;
        case SCOUNTEREN:
            *val = COUNTEREN_DEFAULT;
            return true;
        case SSCRATCH:
            *val = csr->sscratch;
            return true;
        case SEPC:
            *val = csr->sepc;
            return true;
        case SCAUSE:
            *val = csr->scause;
            return true;
        case STVAL:
            *val = csr->stval;
            return true;
        case SIP:
            *val = csr->mip & csr->mideleg;
            return true;
        case SATP:
            *val = csr->satp;
            return true;
        }
    }

    *val = 0;
    return false;
}

// Returns false for unrecognized CSRs. Writes to read-only CSRs (misa, mvendorid, etc.) silently
// succeed -- the RISC-V spec allows this.
bool csr_write(csr_td *csr, priv_level_td privilege, uint32_t addr, uint32_t val) {
    if (privilege == priv_machine) {
        switch (addr) {
        case MSTATUS:
            csr->mstatus = write_with_bitmask(csr->mstatus, val, MSTATUS_MASK);
            return true;
        case MSTATUSH:
            return true;
        case MISA:
            return true;
        case MEDELEG:
            csr->medeleg = write_with_bitmask(csr->medeleg, val, MEDELEG_MASK);
            return true;
        case MIDELEG:
            csr->mideleg = write_with_bitmask(csr->mideleg, val, SIE_SIP_MASK);
            return true;
        case MIE:
            csr->mie = write_with_bitmask(csr->mie, val, MIE_MIP_MASK);
            return true;
        case MTVEC:
            csr->mtvec = write_with_bitmask(csr->mtvec, val, TVEC_MASK);
            return true;
        case MSCRATCH:
            csr->mscratch = val;
            return true;
        case MEPC:
            csr->mepc = val;
            return true;
        case MCAUSE:
            csr->mcause = val;
            return true;
        case MTVAL:
            csr->mtval = val;
            return true;
        case MIP:
            csr->mip = write_with_bitmask(csr->mip, val, SIE_SIP_MASK);
            return true;
        default:
            break;
        }
    }
    if (privilege == priv_machine || privilege == priv_supervisor) {
        switch (addr) {
        case SSTATUS:
            csr->mstatus = write_with_bitmask(csr->mstatus, val, SSTATUS_MASK);
            return true;
        case SIE:
            csr->mie = write_with_bitmask(csr->mie, val, SIE_SIP_MASK);
            return true;
        case STVEC:
            csr->stvec = write_with_bitmask(csr->stvec, val, TVEC_MASK);
            return true;
        case SCOUNTEREN:
            return true;
        case SSCRATCH:
            csr->sscratch = val;
            return true;
        case SEPC:
            csr->sepc = write_with_bitmask(csr->sepc, val, EPC_MASK);
            return true;
        case SCAUSE:
            csr->scause = val;
            return true;
        case STVAL:
            csr->stval = val;
            return true;
        case SIP:
            csr->mip = write_with_bitmask(csr->mip, val, SIP_SSIP_MASK);
            return true;
        case SATP:
            csr->satp = val;
            return true;
        default:
            break;
        }
    }
    return false;
}

void csr_write_mip(csr_td *csr, bool val, uint8_t bit) {
    csr->mip &= ~(1 << bit);
    csr->mip |= (uint32_t)val << bit;
}
