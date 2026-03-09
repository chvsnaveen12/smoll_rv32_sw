// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Naveen Chavali

#include "csr.h"
#include "defs.h"
#include "memory.h"
#include "types.h"
#include <stdbool.h>
#include <stdio.h>

static bool core_bus_access_func(core_td *core, uint32_t addr, bus_access access, uint32_t *val,
                                 uint8_t len);
bool soc_bus_access_func(soc_td *soc, uint32_t addr, bus_access access, uint32_t *val, uint8_t len);

// next_pc defaults to pc+4, branches and jumps override it
static inline void lui(core_td *core) {
    core->regs[core->rd] = core->imm_u;
    core->next_pc = core->pc + 4;
}

static inline void auipc(core_td *core) {
    core->regs[core->rd] = core->pc + core->imm_u;
    core->next_pc = core->pc + 4;
}

static inline void addi(core_td *core) {
    core->regs[core->rd] = core->regs[core->rs1] + core->imm_i;
    core->next_pc = core->pc + 4;
}

static inline void slti(core_td *core) {
    if ((int32_t)core->regs[core->rs1] < (int32_t)core->imm_i)
        core->regs[core->rd] = 1;
    else
        core->regs[core->rd] = 0;
    core->next_pc = core->pc + 4;
}

static inline void sltiu(core_td *core) {
    if ((uint32_t)core->regs[core->rs1] < (uint32_t)core->imm_i)
        core->regs[core->rd] = 1;
    else
        core->regs[core->rd] = 0;
    core->next_pc = core->pc + 4;
}

static inline void xori(core_td *core) {
    core->regs[core->rd] = core->regs[core->rs1] ^ core->imm_i;
    core->next_pc = core->pc + 4;
}

static inline void ori(core_td *core) {
    core->regs[core->rd] = core->regs[core->rs1] | core->imm_i;
    core->next_pc = core->pc + 4;
}

static inline void andi(core_td *core) {
    core->regs[core->rd] = core->regs[core->rs1] & core->imm_i;
    core->next_pc = core->pc + 4;
}

static inline void slli(core_td *core) {
    core->regs[core->rd] = core->regs[core->rs1] << core->shamt;
    core->next_pc = core->pc + 4;
}

static inline void srli_srai(core_td *core) {
    bool bit31 = (bool)extract_bits(core->regs[core->rs1], 31, 31);
    core->regs[core->rd] = core->regs[core->rs1] >> core->shamt;
    core->next_pc = core->pc + 4;

    // bit30 of the instruction determines whether it's srli or srai
    // If it's srli OR bit31 is 0, we can exit without sexting
    if (!core->bit30 || !bit31 || core->shamt == 0)
        return;

    core->regs[core->rd] |= sext_bits(32 - core->shamt);
}

static inline void add_sub(core_td *core) {
    // bit30 determines if it's an add or a subtract
    if (core->bit30)
        core->regs[core->rd] = core->regs[core->rs1] - core->regs[core->rs2];
    else
        core->regs[core->rd] = core->regs[core->rs1] + core->regs[core->rs2];

    core->next_pc = core->pc + 4;
}

static inline void sll(core_td *core) {
    core->regs[core->rd] = core->regs[core->rs1] << (core->regs[core->rs2] & 0x1f);
    core->next_pc = core->pc + 4;
}

static inline void slt(core_td *core) {
    if ((int32_t)core->regs[core->rs1] < (int32_t)core->regs[core->rs2])
        core->regs[core->rd] = 1;
    else
        core->regs[core->rd] = 0;
    core->next_pc = core->pc + 4;
}

static inline void sltu(core_td *core) {
    if ((uint32_t)core->regs[core->rs1] < (uint32_t)core->regs[core->rs2])
        core->regs[core->rd] = 1;
    else
        core->regs[core->rd] = 0;
    core->next_pc = core->pc + 4;
}

static inline void xor_op(core_td *core) {
    core->regs[core->rd] = core->regs[core->rs1] ^ core->regs[core->rs2];
    core->next_pc = core->pc + 4;
}

// Figure a better way to do this
static inline void srl_sra(core_td *core) {
    bool bit31 = (bool)extract_bits(core->regs[core->rs1], 31, 31);
    uint8_t shamt = core->regs[core->rs2] & 0x1f;
    core->regs[core->rd] = core->regs[core->rs1] >> shamt;

    core->next_pc = core->pc + 4;

    // bit30 distinguishes sra from srl, skip sext if srli or value is positive or shift is 0
    if (!core->bit30 || !bit31 || shamt == 0)
        return;

    core->regs[core->rd] |= sext_bits(32 - shamt);
}

static inline void or_op(core_td *core) {
    core->regs[core->rd] = core->regs[core->rs1] | core->regs[core->rs2];
    core->next_pc = core->pc + 4;
}

static inline void and_op(core_td *core) {
    core->regs[core->rd] = core->regs[core->rs1] & core->regs[core->rs2];
    core->next_pc = core->pc + 4;
}

static inline void jal(core_td *core) {
    core->regs[core->rd] = core->pc + 4;
    // bit1 is cleared to force 4-byte alignment, spec requires misaligned trap but we just mask
    core->next_pc = (core->pc + core->imm_j) & ~0x02;
}

static inline void jalr(core_td *core) {
    uint32_t temp = core->pc + 4;
    core->next_pc = (core->regs[core->rs1] + core->imm_i) & ~0x02;
    core->regs[core->rd] = temp;
}

static inline void beq(core_td *core) {
    if (core->regs[core->rs1] == core->regs[core->rs2])
        core->next_pc = (core->pc + core->imm_b) & ~0x02;
    else
        core->next_pc = core->pc + 4;
}

static inline void bne(core_td *core) {
    if (core->regs[core->rs1] != core->regs[core->rs2])
        core->next_pc = (core->pc + core->imm_b) & ~0x02;
    else
        core->next_pc = core->pc + 4;
}

static inline void blt(core_td *core) {
    if ((int32_t)core->regs[core->rs1] < (int32_t)core->regs[core->rs2])
        core->next_pc = (core->pc + core->imm_b) & ~0x02;
    else
        core->next_pc = core->pc + 4;
}

static inline void bge(core_td *core) {
    if ((int32_t)core->regs[core->rs1] >= (int32_t)core->regs[core->rs2])
        core->next_pc = (core->pc + core->imm_b) & ~0x02;
    else
        core->next_pc = core->pc + 4;
}

static inline void bltu(core_td *core) {
    if ((uint32_t)core->regs[core->rs1] < (uint32_t)core->regs[core->rs2])
        core->next_pc = (core->pc + core->imm_b) & ~0x02;
    else
        core->next_pc = core->pc + 4;
}

static inline void bgeu(core_td *core) {
    if ((uint32_t)core->regs[core->rs1] >= (uint32_t)core->regs[core->rs2])
        core->next_pc = (core->pc + core->imm_b) & ~0x02;
    else
        core->next_pc = core->pc + 4;
}

static inline void lb(core_td *core) {
    uint32_t val;
    uint32_t addr = core->regs[core->rs1] + core->imm_i;

    if (!core_bus_access_func(core, addr, bus_read, &val, 1))
        return;

    // Sext
    if (val & 0x80)
        val |= sext_bits(8);
    else
        val &= ~sext_bits(8);

    core->regs[core->rd] = val;
    core->next_pc = core->pc + 4;
}

static inline void lh(core_td *core) {
    uint32_t val;
    uint32_t addr = core->regs[core->rs1] + core->imm_i;

    if (addr & 0x1) {
        core_prepare_sync_trap(core, load_addr_misalig, addr);
        return;
    }

    if (!core_bus_access_func(core, addr, bus_read, &val, 2))
        return;

    // Sext
    if (val & 0x8000)
        val |= sext_bits(16);
    else
        val &= ~sext_bits(16);

    core->regs[core->rd] = val;
    core->next_pc = core->pc + 4;
}

static inline void lw(core_td *core) {
    uint32_t val;
    uint32_t addr = core->regs[core->rs1] + core->imm_i;

    if (addr & 0x3) {
        core_prepare_sync_trap(core, load_addr_misalig, addr);
        return;
    }

    if (!core_bus_access_func(core, addr, bus_read, &val, 4))
        return;

    // Sext is not required

    core->regs[core->rd] = val;
    core->next_pc = core->pc + 4;
}

static inline void lbu(core_td *core) {
    uint32_t val;
    uint32_t addr = core->regs[core->rs1] + core->imm_i;

    if (!core_bus_access_func(core, addr, bus_read, &val, 1))
        return;

    core->regs[core->rd] = val & 0xff;
    core->next_pc = core->pc + 4;
}

static inline void lhu(core_td *core) {
    uint32_t val;
    uint32_t addr = core->regs[core->rs1] + core->imm_i;

    if (addr & 0x1) {
        core_prepare_sync_trap(core, load_addr_misalig, addr);
        return;
    }

    if (!core_bus_access_func(core, addr, bus_read, &val, 2))
        return;
    core->regs[core->rd] = val & 0xffff;
    core->next_pc = core->pc + 4;
}

static inline void sb(core_td *core) {
    uint32_t val = core->regs[core->rs2];
    uint32_t addr = core->regs[core->rs1] + core->imm_s;
    if (!core_bus_access_func(core, addr, bus_write, &val, 1))
        return;
    core->next_pc = core->pc + 4;
}

static inline void sh(core_td *core) {
    uint32_t val = core->regs[core->rs2];
    uint32_t addr = core->regs[core->rs1] + core->imm_s;

    if (addr & 0x1) {
        core_prepare_sync_trap(core, load_addr_misalig, addr);
        return;
    }

    if (!core_bus_access_func(core, addr, bus_write, &val, 2))
        return;
    core->next_pc = core->pc + 4;
}

static inline void sw(core_td *core) {
    uint32_t val = core->regs[core->rs2];
    uint32_t addr = core->regs[core->rs1] + core->imm_s;

    if (addr & 0x3) {
        core_prepare_sync_trap(core, load_addr_misalig, addr);
        return;
    }

    if (!core_bus_access_func(core, addr, bus_write, &val, 4))
        return;
    core->next_pc = core->pc + 4;
}

//=======Env====================

static inline void fence_fencei(core_td *core) {
    core->next_pc = core->pc + 4;
}

static inline void ecall_ebreak(core_td *core) {
    // RS2 determines ebreak vs ecall
    if (core->rs2 == 1) {
        core_prepare_sync_trap(core, breakpoint, core->inst);
        return;
    }
    // Valid values for rs2 are either 1 or 0
    if (core->rs2 != 0) {
        core->invalid = true;
        return;
    }

    switch (core->privilege) {
    case priv_machine:
        core_prepare_sync_trap(core, machine_ecall, core->inst);
        return;
    case priv_supervisor:
        core_prepare_sync_trap(core, supervisor_ecall, core->inst);
        return;
    case priv_user:
        core_prepare_sync_trap(core, user_ecall, core->inst);
        return;
    default:
        core->invalid = true;
    }
}

static inline void sret_wfi(core_td *core) {
    uint32_t pie, xstatus, addr;

    // rs2=5 is wfi, rs2=2 is sret
    if (core->rs2 == 5) {
        core->next_pc = core->pc + 4;
        return;
    }

    if (core->privilege != priv_supervisor) {
        core->invalid = true;
        return;
    }

    csr_read(&core->csr, priv_machine, MSTATUS, &xstatus);
    core->privilege = (priv_level_td)extract_bits(xstatus, MSTATUS_SPP_BIT, MSTATUS_SPP_BIT);

    pie = extract_bits(xstatus, MSTATUS_SPIE_BIT, MSTATUS_SPIE_BIT);
    xstatus &= ~(1 << MSTATUS_SPP_BIT);
    xstatus |= (1 << MSTATUS_SPIE_BIT);
    xstatus &= ~(1 << MSTATUS_SIE_BIT);
    xstatus |= (pie << MSTATUS_SIE_BIT);
    xstatus &= ~(1 << MSTATUS_MPRV_BIT);
    csr_write(&core->csr, priv_machine, MSTATUS, xstatus);

    csr_read(&core->csr, priv_machine, SEPC, &addr);
    core->next_pc = addr;
    return;
}

static inline void mret(core_td *core) {
    uint32_t pie, xstatus, addr;

    if (core->privilege != priv_machine) {
        core->invalid = true;
        return;
    }

    csr_read(&core->csr, priv_machine, MSTATUS, &xstatus);

    core->privilege = (priv_level_td)extract_bits(xstatus, MSTATUS_MPP_BIT + 1, MSTATUS_MPP_BIT);
    pie = extract_bits(xstatus, MSTATUS_MPIE_BIT, MSTATUS_MPIE_BIT);
    xstatus &= ~(3 << MSTATUS_MPP_BIT);
    xstatus |= (1 << MSTATUS_MPIE_BIT);
    xstatus &= ~(1 << MSTATUS_MIE_BIT);
    xstatus |= (pie << MSTATUS_MIE_BIT);
    csr_write(&core->csr, priv_machine, MSTATUS, xstatus);

    csr_read(&core->csr, priv_machine, MEPC, &addr);
    core->next_pc = addr;
    return;
}

static inline void sfence_vma(core_td *core) {
    core->next_pc = core->pc + 4;
}

static inline void sinval_vma(core_td *core) {
    core->invalid = true;
}

static inline void sfence_w_inval_inval_ir(core_td *core) {
    core->invalid = true;
}

static inline void csrrw(core_td *core) {
    uint32_t read_val;
    uint32_t write_val = core->regs[core->rs1];
    uint16_t csr_addr = (core->funct7 << 5) | core->rs2;

    // rd=0 means caller doesn't need the old value, skip the read
    if (core->rd == 0) {
        if (!csr_write(&core->csr, core->privilege, csr_addr, write_val)) {
            core->invalid = true;
            return;
        }
    } else {
        if (!csr_read(&core->csr, core->privilege, csr_addr, &read_val)) {
            core->invalid = true;
            return;
        }
        if (!csr_write(&core->csr, core->privilege, csr_addr, write_val)) {
            core->invalid = true;
            return;
        }
        core->regs[core->rd] = read_val;
    }
    core->next_pc = core->pc + 4;
}

static inline void csrrs(core_td *core) {
    uint32_t read_val;
    uint32_t set_val = core->regs[core->rs1];
    uint16_t csr_addr = (core->funct7 << 5) | core->rs2;

    if (core->rs1 == 0) {
        if (!csr_read(&core->csr, core->privilege, csr_addr, &read_val)) {
            core->invalid = true;
            return;
        }
        core->regs[core->rd] = read_val;
    } else {
        if (!csr_read(&core->csr, core->privilege, csr_addr, &read_val)) {
            core->invalid = true;
            return;
        }
        if (!csr_write(&core->csr, core->privilege, csr_addr, set_val | read_val)) {
            core->invalid = true;
            return;
        }
        core->regs[core->rd] = read_val;
    }
    core->next_pc = core->pc + 4;
}

static inline void csrrc(core_td *core) {
    uint32_t read_val;
    uint32_t clear_val = core->regs[core->rs1];
    uint16_t csr_addr = (core->funct7 << 5) | core->rs2;

    if (core->rs1 == 0) {
        if (!csr_read(&core->csr, core->privilege, csr_addr, &read_val)) {
            core->invalid = true;
            return;
        }
        core->regs[core->rd] = read_val;
    } else {
        if (!csr_read(&core->csr, core->privilege, csr_addr, &read_val)) {
            core->invalid = true;
            return;
        }
        if (!csr_write(&core->csr, core->privilege, csr_addr, ~clear_val & read_val)) {
            core->invalid = true;
            return;
        }
        core->regs[core->rd] = read_val;
    }
    core->next_pc = core->pc + 4;
}

static inline void csrrwi(core_td *core) {
    uint32_t read_val;
    uint32_t write_imm = core->rs1;
    uint16_t csr_addr = (core->funct7 << 5) | core->rs2;

    if (core->rd == 0) {
        if (!csr_write(&core->csr, core->privilege, csr_addr, write_imm)) {
            core->invalid = true;
            return;
        }
    } else {
        if (!csr_read(&core->csr, core->privilege, csr_addr, &read_val)) {
            core->invalid = true;
            return;
        }
        if (!csr_write(&core->csr, core->privilege, csr_addr, write_imm)) {
            core->invalid = true;
            return;
        }
        core->regs[core->rd] = read_val;
    }
    core->next_pc = core->pc + 4;
}

static inline void csrrsi(core_td *core) {
    uint32_t read_val;
    uint32_t set_imm = core->rs1;
    uint16_t csr_addr = (core->funct7 << 5) | core->rs2;

    if (core->rs1 == 0) {
        if (!csr_read(&core->csr, core->privilege, csr_addr, &read_val)) {
            core->invalid = true;
            return;
        }
        core->regs[core->rd] = read_val;
    } else {
        if (!csr_read(&core->csr, core->privilege, csr_addr, &read_val)) {
            core->invalid = true;
            return;
        }
        if (!csr_write(&core->csr, core->privilege, csr_addr, set_imm | read_val)) {
            core->invalid = true;
            return;
        }
        core->regs[core->rd] = read_val;
    }
    core->next_pc = core->pc + 4;
}

static inline void csrrci(core_td *core) {
    uint32_t read_val;
    uint32_t clear_imm = core->rs1;
    uint16_t csr_addr = (core->funct7 << 5) | core->rs2;

    if (core->rs1 == 0) {
        if (!csr_read(&core->csr, core->privilege, csr_addr, &read_val)) {
            core->invalid = true;
            return;
        }
        core->regs[core->rd] = read_val;
    } else {
        if (!csr_read(&core->csr, core->privilege, csr_addr, &read_val)) {
            core->invalid = true;
            return;
        }
        if (!csr_write(&core->csr, core->privilege, csr_addr, ~clear_imm & read_val)) {
            core->invalid = true;
            return;
        }
        core->regs[core->rd] = read_val;
    }
    core->next_pc = core->pc + 4;
}

// ---------------------------------- M
// -------------------------------------------------------------------------

static inline void mul(core_td *core) {
    core->regs[core->rd] = (int32_t)core->regs[core->rs1] * (int32_t)core->regs[core->rs2];
    core->next_pc = core->pc + 4;
}

static inline void mulh(core_td *core) {
    int64_t temp1 = (int64_t)(int32_t)core->regs[core->rs1];
    int64_t temp2 = (int64_t)(int32_t)core->regs[core->rs2];
    core->regs[core->rd] = (temp1 * temp2) >> 32;
    core->next_pc = core->pc + 4;
}

static inline void mulhsu(core_td *core) {
    uint64_t temp1 = (int64_t)(int32_t)core->regs[core->rs1];
    uint64_t temp2 = (uint64_t)core->regs[core->rs2];
    core->regs[core->rd] = (temp1 * temp2) >> 32;
    core->next_pc = core->pc + 4;
}

static inline void mulhu(core_td *core) {
    uint64_t temp = (uint64_t)core->regs[core->rs1] * (uint64_t)core->regs[core->rs2];
    core->regs[core->rd] = temp >> 32;
    core->next_pc = core->pc + 4;
}

static inline void divv(core_td *core) {
    if (core->regs[core->rs2] == 0)
        core->regs[core->rd] = 0xffffffff; // div by zero => -1

    else if (core->regs[core->rs2] == 0xffffffff && core->regs[core->rs1] == 0x80000000)
        core->regs[core->rd] = 0x80000000; // INT_MIN / -1 overflow

    else
        core->regs[core->rd] = (int32_t)core->regs[core->rs1] / (int32_t)core->regs[core->rs2];

    core->next_pc = core->pc + 4;
}

static inline void divu(core_td *core) {
    if (core->regs[core->rs2] == 0)
        core->regs[core->rd] = 0xffffffff;

    else
        core->regs[core->rd] = (uint32_t)core->regs[core->rs1] / (uint32_t)core->regs[core->rs2];

    core->next_pc = core->pc + 4;
}

static inline void rem(core_td *core) {
    if (core->regs[core->rs2] == 0)
        core->regs[core->rd] = core->regs[core->rs1]; // rem by zero => dividend

    else if (core->regs[core->rs2] == 0xffffffff && core->regs[core->rs1] == 0x80000000)
        core->regs[core->rd] = 0; // INT_MIN % -1 overflow => 0

    else
        core->regs[core->rd] = (int32_t)core->regs[core->rs1] % (int32_t)core->regs[core->rs2];

    core->next_pc = core->pc + 4;
}

static inline void remu(core_td *core) {
    if (core->regs[core->rs2] == 0)
        core->regs[core->rd] = core->regs[core->rs1];

    else
        core->regs[core->rd] = (uint32_t)core->regs[core->rs1] % (uint32_t)core->regs[core->rs2];

    core->next_pc = core->pc + 4;
}

// ---------------------------------- A
// -------------------------------------------------------------------------

static inline void lr(core_td *core) {
    uint32_t val;
    uint32_t addr = core->regs[core->rs1];

    if (!core_bus_access_func(core, addr, bus_read, &val, 4))
        return;

    core->regs[core->rd] = val;
    core->next_pc = core->pc + 4;
}

static inline void sc(core_td *core) {
    uint32_t val = core->regs[core->rs2];
    uint32_t addr = core->regs[core->rs1];
    if (!core_bus_access_func(core, addr, bus_write, &val, 4))
        return;
    core->regs[core->rd] = 0; // always succeeds, no reservation tracking
    core->next_pc = core->pc + 4;
}

static inline void amoswap(core_td *core) {
    uint32_t read_val;
    uint32_t write_val = core->regs[core->rs2];
    uint32_t addr = core->regs[core->rs1];
    if (!core_bus_access_func(core, addr, bus_read, &read_val, 4))
        return;

    if (!core_bus_access_func(core, addr, bus_write, &write_val, 4))
        return;
    core->regs[core->rd] = read_val;
    core->next_pc = core->pc + 4;
}

static inline void amoadd(core_td *core) {
    uint32_t read_val;
    uint32_t write_val = core->regs[core->rs2];
    uint32_t addr = core->regs[core->rs1];
    if (!core_bus_access_func(core, addr, bus_read, &read_val, 4))
        return;

    write_val = write_val + read_val;

    if (!core_bus_access_func(core, addr, bus_write, &write_val, 4))
        return;
    core->regs[core->rd] = read_val;
    core->next_pc = core->pc + 4;
}

static inline void amoxor(core_td *core) {
    uint32_t read_val;
    uint32_t write_val = core->regs[core->rs2];
    uint32_t addr = core->regs[core->rs1];
    if (!core_bus_access_func(core, addr, bus_read, &read_val, 4))
        return;

    write_val = write_val ^ read_val;

    if (!core_bus_access_func(core, addr, bus_write, &write_val, 4))
        return;
    core->regs[core->rd] = read_val;
    core->next_pc = core->pc + 4;
}

static inline void amoand(core_td *core) {
    uint32_t read_val;
    uint32_t write_val = core->regs[core->rs2];
    uint32_t addr = core->regs[core->rs1];
    if (!core_bus_access_func(core, addr, bus_read, &read_val, 4))
        return;

    write_val = write_val & read_val;

    if (!core_bus_access_func(core, addr, bus_write, &write_val, 4))
        return;
    core->regs[core->rd] = read_val;
    core->next_pc = core->pc + 4;
}

static inline void amoor(core_td *core) {
    uint32_t read_val;
    uint32_t write_val = core->regs[core->rs2];
    uint32_t addr = core->regs[core->rs1];
    if (!core_bus_access_func(core, addr, bus_read, &read_val, 4))
        return;

    write_val = write_val | read_val;

    if (!core_bus_access_func(core, addr, bus_write, &write_val, 4))
        return;
    core->regs[core->rd] = read_val;
    core->next_pc = core->pc + 4;
}

static inline void amomin(core_td *core) {
    uint32_t read_val;
    uint32_t write_val = core->regs[core->rs2];
    uint32_t addr = core->regs[core->rs1];
    if (!core_bus_access_func(core, addr, bus_read, &read_val, 4))
        return;

    write_val = (int32_t)read_val > (int32_t)write_val ? write_val : read_val;

    if (!core_bus_access_func(core, addr, bus_write, &write_val, 4))
        return;
    core->regs[core->rd] = read_val;
    core->next_pc = core->pc + 4;
}

static inline void amomax(core_td *core) {
    uint32_t read_val;
    uint32_t write_val = core->regs[core->rs2];
    uint32_t addr = core->regs[core->rs1];
    if (!core_bus_access_func(core, addr, bus_read, &read_val, 4))
        return;

    write_val = (int32_t)read_val > (int32_t)write_val ? read_val : write_val;

    if (!core_bus_access_func(core, addr, bus_write, &write_val, 4))
        return;
    core->regs[core->rd] = read_val;
    core->next_pc = core->pc + 4;
}

static inline void amominu(core_td *core) {
    uint32_t read_val;
    uint32_t write_val = core->regs[core->rs2];
    uint32_t addr = core->regs[core->rs1];
    if (!core_bus_access_func(core, addr, bus_read, &read_val, 4))
        return;

    write_val = (uint32_t)read_val > (uint32_t)write_val ? write_val : read_val;

    if (!core_bus_access_func(core, addr, bus_write, &write_val, 4))
        return;
    core->regs[core->rd] = read_val;
    core->next_pc = core->pc + 4;
}

static inline void amomaxu(core_td *core) {
    uint32_t read_val;
    uint32_t write_val = core->regs[core->rs2];
    uint32_t addr = core->regs[core->rs1];
    if (!core_bus_access_func(core, addr, bus_read, &read_val, 4))
        return;

    write_val = (uint32_t)read_val > (uint32_t)write_val ? read_val : write_val;

    if (!core_bus_access_func(core, addr, bus_write, &write_val, 4))
        return;
    core->regs[core->rd] = read_val;
    core->next_pc = core->pc + 4;
}

static bool core_bus_access_func(core_td *core, uint32_t addr, bus_access access, uint32_t *val,
                                 uint8_t len) {
    priv_level_td effective_priv;
    trap_cause_exception acccess_fault, page_fault;
    uint32_t satp, xstatus;
    bool sum, mxr;

    uint32_t phy_addr = 0;
    uint32_t i, vpn[SV32_LEVELS], ppn, pte, offset; // [LEVELS]

    // Check for MPRV override and machine mode.
    if (core->privilege == priv_machine) {
        csr_read(&core->csr, priv_machine, MSTATUS, &xstatus);

        // If MPRV is not set or its a machine level fetch, No translation takes place
        if (!extract_bits(xstatus, MSTATUS_MPRV_BIT, MSTATUS_MPRV_BIT) || access == bus_fetch)
            return soc_bus_access_func(core->soc_ptr, addr, access, val, len);

        // If MPRV is set, set the effective priv to MPP
        effective_priv = (priv_level_td)extract_bits(xstatus, MSTATUS_MPP_BIT + 1, MSTATUS_MPP_BIT);
    } else
        effective_priv = core->privilege;

    csr_read(&core->csr, priv_machine, MSTATUS, &xstatus);
    csr_read(&core->csr, priv_machine, SATP, &satp);

    // If mode is bare, exit
    if ((satp >> SATP_MODE_BIT) != SV32_MODE)
        return soc_bus_access_func(core->soc_ptr, addr, access, val, len);

    // Set the correct faults
    switch (access) {
    case bus_write:
        acccess_fault = str_amo_access_fault;
        page_fault = str_amo_page_fault;
        break;
    case bus_fetch:
        acccess_fault = instr_access_fault;
        page_fault = instr_page_fault;
        break;
    case bus_read:
        acccess_fault = load_access_fault;
        page_fault = load_page_fault;
        break;
    default:
        break;
    }

    // Extracting sum and mxr
    sum = (bool)extract_bits(xstatus, MSTATUS_SUM_BIT, MSTATUS_SUM_BIT);
    mxr = (bool)extract_bits(xstatus, MSTATUS_MXR_BIT, MSTATUS_MXR_BIT);

    ppn = (satp & PPN_MASK) << PAGE_SIZE_SHIFT;
    vpn[1] = (addr >> 22) & VPN_MASK;
    vpn[0] = (addr >> 12) & VPN_MASK;
    offset = addr & OFFSET_MASK;

    // i = LEVELS - 1, *PAGESIZE => << 12
    for (i = SV32_LEVELS - 1; i >= 0; i--) {
        // Only if we are unable to access a PTE is it an access fault
        // Otherwise it's always a page fault!!
        if (!soc_bus_access_func(core->soc_ptr, ppn | (vpn[i] << 2), bus_read, &pte, 4)) {
            core_prepare_sync_trap(core, acccess_fault, addr);
            return false;
        }
        if (!extract_bits(pte, PTE_V, PTE_V)) {
            core_prepare_sync_trap(core, page_fault, addr);
            return false;
        }

        if (!(pte & PTE_RWX_MASK)) {
            ppn = ((pte >> 10) & PPN_MASK) << PAGE_SIZE_SHIFT;
            continue;
        }

        // i=1 is a 4MB superpage, i=0 is a normal 4KB page
        if (i == 1)
            phy_addr = (((pte >> 20) & 0x00000fff) << 22) | (vpn[0] << 12) | offset;
        else if (i == 0) {
            phy_addr = (((pte >> 10) & 0x003fffff) << 12) | offset;
        }

        // All the mandatory checks
        if (access == bus_fetch && !extract_bits(pte, PTE_X, PTE_X)) {
            core_prepare_sync_trap(core, page_fault, addr);
            return false;
        } else if (access == bus_read &&
                   !(extract_bits(pte, PTE_R, PTE_R) || (mxr && extract_bits(pte, PTE_X, PTE_X)))) {
            core_prepare_sync_trap(core, page_fault, addr);
            return false;
        } else if (access == bus_write && !extract_bits(pte, PTE_W, PTE_W)) {
            core_prepare_sync_trap(core, page_fault, addr);
            return false;
        }

        if (effective_priv == priv_supervisor && extract_bits(pte, PTE_U, PTE_U) && !sum) {
            core_prepare_sync_trap(core, page_fault, addr);
            return false;
        }

        if (effective_priv == priv_user && !extract_bits(pte, PTE_U, PTE_U)) {
            core_prepare_sync_trap(core, page_fault, addr);
            return false;
        }

        // Done with the translation and checks
        bool ret = soc_bus_access_func(core->soc_ptr, phy_addr, access, val, len);
        return ret;
    }
    // We faulted
    core_prepare_sync_trap(core, page_fault, addr);
    return false;
}

bool core_fetch(core_td *core) {
    core->pc = core->next_pc;
    return core_bus_access_func(core, core->pc, bus_fetch, &core->inst, 4);
}

// extracts all fields and sign extends immediates
void core_decode(core_td *core) {
    uint32_t inst = core->inst;

    core->opcode = extract_bits(inst, 6, 0);
    core->rs1 = extract_bits(inst, 19, 15);
    core->rs2 = extract_bits(inst, 24, 20);
    core->rd = extract_bits(inst, 11, 7);
    core->funct3 = extract_bits(inst, 14, 12);
    core->funct7 = extract_bits(inst, 31, 25);
    core->shamt = extract_bits(inst, 24, 20);

    core->bit25 = (bool)extract_bits(inst, 25, 25);
    core->bit30 = (bool)extract_bits(inst, 30, 30);

    core->imm_u = (extract_bits(inst, 31, 12) << 12);
    core->imm_j = (extract_bits(inst, 31, 31) << 20) | (extract_bits(inst, 30, 21) << 1) |
                  (extract_bits(inst, 20, 20) << 11) | (extract_bits(inst, 19, 12) << 12);
    core->imm_i = (extract_bits(inst, 31, 20) << 0);
    core->imm_s = (extract_bits(inst, 31, 25) << 5) | (extract_bits(inst, 11, 7) << 0);
    core->imm_b = (extract_bits(inst, 31, 31) << 12) | (extract_bits(inst, 30, 25) << 5) |
                  (extract_bits(inst, 11, 8) << 1) | (extract_bits(inst, 7, 7) << 11);
    // SExt
    if (extract_bits(core->imm_j, 20, 20))
        core->imm_j |= sext_bits(21);

    if (extract_bits(core->imm_i, 11, 11))
        core->imm_i |= sext_bits(12);

    if (extract_bits(core->imm_s, 11, 11))
        core->imm_s |= sext_bits(12);

    if (extract_bits(core->imm_b, 12, 12))
        core->imm_b |= sext_bits(13);

    core->invalid = false;
    core->sync_trap_pending = false;
}

// dispatches based on opcode and funct3
void core_execute(core_td *core) {
    core->regs[0] = 0;
    switch (core->opcode) {
    case 0b0110111: // LUI
        lui(core);
        break;
    case 0b0010111: // AUIPC
        auipc(core);
        break;
    case 0b1100011: // Branches
        switch (core->funct3) {
        case 0b000:
            beq(core);
            break;
        case 0b001:
            bne(core);
            break;
        case 0b100:
            blt(core);
            break;
        case 0b101:
            bge(core);
            break;
        case 0b110:
            bltu(core);
            break;
        case 0b111:
            bgeu(core);
            break;
        default:
            core->invalid = true;
            break;
        }
        break;
    case 0b0000011: // Loads
        switch (core->funct3) {
        case 0b000:
            lb(core);
            break;
        case 0b001:
            lh(core);
            break;
        case 0b010:
            lw(core);
            break;
        case 0b100:
            lbu(core);
            break;
        case 0b101:
            lhu(core);
            break;
        default:
            core->invalid = true;
        }
        break;
    case 0b0100011: // Stores
        switch (core->funct3) {
        case 0b000:
            sb(core);
            break;
        case 0b001:
            sh(core);
            break;
        case 0b010:
            sw(core);
            break;
        default:
            core->invalid = true;
        }
        break;
    case 0b0010011: // Immediate instructions
        switch (core->funct3) {
        case 0b000:
            addi(core);
            break;
        case 0b001:
            slli(core);
            break;
        case 0b010:
            slti(core);
            break;
        case 0b011:
            sltiu(core);
            break;
        case 0b100:
            xori(core);
            break;
        case 0b101:
            srli_srai(core);
            break;
        case 0b110:
            ori(core);
            break;
        case 0b111:
            andi(core);
            break;
        default:
            core->invalid = true;
            break;
        }
        break;
    case 0b0110011:         // Register instructions
        if (!core->bit25) { // Integer instructions
            switch (core->funct3) {
            case 0b000:
                add_sub(core);
                break;
            case 0b001:
                sll(core);
                break;
            case 0b010:
                slt(core);
                break;
            case 0b011:
                sltu(core);
                break;
            case 0b100:
                xor_op(core);
                break;
            case 0b101:
                srl_sra(core);
                break;
            case 0b110:
                or_op(core);
                break;
            case 0b111:
                and_op(core);
                break;
            default:
                core->invalid = true;
                break;
            }
        } else { // Multiplication and Division instructions
            switch (core->funct3) {
            default:
                core->invalid = true;
                break;
            }
        }
        break;
    case 0b0001111: // FENCE FENCE.I
        fence_fencei(core);
        break;
    case 0b1110011: // Environment instructions
        // CSR instructions: read first, compute new value, write back.
        switch (core->funct3) {
        case 0b000: // Privileged instructions
            switch (core->funct7) {
            case 0b0000000:
                ecall_ebreak(core);
                break;
            case 0b0001000:
                sret_wfi(core);
                break;
            case 0b0011000:
                mret(core);
                break;
            case 0b0001001:
                sfence_vma(core);
                break;
            case 0b0001011:
                sinval_vma(core);
                break;
            case 0b0001100:
                sfence_w_inval_inval_ir(core);
                break;
            default:
                core->invalid = true;
            }
            break;
        case 0b001: // CSR read writes
            csrrw(core);
            break;
        case 0b010:
            csrrs(core);
            break;
        case 0b011:
            csrrc(core);
            break;
        case 0b101:
            csrrwi(core);
            break;
        case 0b110:
            csrrsi(core);
            break;
        case 0b111:
            csrrci(core);
            break;
        default:
            core->invalid = true;
            break;
        }
        break;
    case 0b1101111: // JAL
        jal(core);
        break;
    case 0b1100111: // JALR
        jalr(core);
        break;
    case 0b0101111: // ATOMICS
        switch (core->funct7 & ~0x00000003) {
        default:
            core->invalid = true;
        }
        break;
    default:
        core->invalid = true;
        break;
    }
    core->regs[0] = 0;
    if (core->invalid)
        core_prepare_sync_trap(core, illegal_instr, core->inst);

    core->cycle++;
}

// fires on the next call to core_process_interrupts
void core_prepare_sync_trap(core_td *core, trap_cause_exception cause, uint32_t tval) {
    if (!core->sync_trap_pending) {
        core->sync_trap_pending = true;
        core->sync_trap_cause = cause;
        core->sync_tval = tval;
    }
}

void core_dump(core_td *core) {
    printf("Cycle: %lu\n", core->cycle);
    printf("Inst: 0x%08x\tInvalid: %s\n", core->inst, core->invalid ? "true" : "false");
    printf("PC: 0x%08x\t\tNPC: 0x%08x\n\n", core->pc, core->next_pc);
    for (int i = 0; i < 16; i++)
        printf("x%02d:0x%08x\t\tx%02d:0x%08x\n", i, core->regs[i], i + 16, core->regs[i + 16]);
    uint32_t val1, val2;
    csr_read(&(core->csr), priv_machine, MIE, &val1);
    csr_read(&(core->csr), priv_machine, MIP, &val2);
    printf("MIE:\t0x%08x\t\tMIP:\t0x%08x\n", val1, val2);

    csr_read(&(core->csr), priv_machine, MIDELEG, &val1);
    csr_read(&(core->csr), priv_machine, MEDELEG, &val2);
    printf("MIDELEG:0x%08x\t\tMEDELEG:0x%08x\n", val1, val2);

    csr_read(&(core->csr), priv_machine, MTVEC, &val1);
    csr_read(&(core->csr), priv_machine, MEPC, &val2);
    printf("MTVEC:\t0x%08x\t\tMEPC:\t0x%08x\n", val1, val2);

    csr_read(&(core->csr), priv_machine, MCAUSE, &val1);
    csr_read(&(core->csr), priv_machine, MTVAL, &val2);
    printf("MCAUSE:\t0x%08x\t\tMTVAL:\t0x%08x\n", val1, val2);

    csr_read(&(core->csr), priv_machine, SEPC, &val1);
    csr_read(&(core->csr), priv_machine, SCAUSE, &val2);
    printf("SEPC:\t0x%08x\t\tSCAUSE:\t0x%08x\n", val1, val2);

    csr_read(&(core->csr), priv_machine, STVEC, &val1);
    csr_read(&(core->csr), priv_machine, STVAL, &val2);
    printf("STVEC:\t0x%08x\t\tSTVAL:\t0x%08x\n", val1, val2);

    csr_read(&(core->csr), priv_machine, MSTATUS, &val1);
    csr_read(&(core->csr), priv_machine, SSTATUS, &val2);
    printf("MSTATUS:0x%08x\t\tSSTATUS:0x%08x\n", val1, val2);

    printf("Sync Pending:%d, Cause:%d, TVAL:%d\n", core->sync_trap_pending, core->sync_trap_cause,
           core->sync_tval);
}

// soc_ptr is used for bus access during fetch, load, and store
void core_init(core_td *core, soc_td *soc_ptr) {
    core->pc = core->next_pc = ROM_BASE;
    core->cycle = 0;
    core->regs[0] = 0;
    core->privilege = priv_machine;
    core->sync_trap_pending = false;

    core->soc_ptr = soc_ptr;
}

// commits pc and csrs after handling any pending traps or interrupts
void core_process_interrupts(core_td *core, bool mei, bool sei, bool mti, bool msi) {
    uint32_t ie, xstatus, addr, medeleg;
    bool delegated = false;

    uint32_t mstatus, mideleg, mip, mie;
    bool mstatus_mie, mstatus_sie, mie_val, mip_val, int_val, mideleg_val;
    trap_cause_interrupt interrupt_cause;

    csr_write_mip(&core->csr, msi, MSIP_BIT);
    csr_write_mip(&core->csr, mti, MTIP_BIT);
    csr_write_mip(&core->csr, mei, MEIP_BIT);
    csr_write_mip(&core->csr, sei, SEIP_BIT);

    if (core->sync_trap_pending == true) {
        if (core->privilege != priv_machine) {
            csr_read(&core->csr, priv_machine, MEDELEG, &medeleg);
            delegated = (bool)extract_bits(medeleg, core->sync_trap_cause, core->sync_trap_cause);
        }

        if (delegated) {
            // xEPC xTVAL and xCAUSE
            csr_write(&core->csr, priv_supervisor, SEPC, core->pc);
            csr_write(&core->csr, priv_supervisor, SCAUSE, core->sync_trap_cause);
            csr_write(&core->csr, priv_supervisor, STVAL, core->sync_tval);

            // xPP & xIE Stack
            csr_read(&core->csr, priv_supervisor, SSTATUS, &xstatus);
            xstatus &= ~(1 << MSTATUS_SPP_BIT);
            xstatus |= (uint32_t)(core->privilege & 0x1) << MSTATUS_SPP_BIT;
            ie = extract_bits(xstatus, MSTATUS_SIE_BIT, MSTATUS_SIE_BIT);
            xstatus &= ~(1 << MSTATUS_SIE_BIT);
            xstatus &= ~(1 << MSTATUS_SPIE_BIT);
            xstatus |= ie << MSTATUS_SPIE_BIT;
            csr_write(&core->csr, priv_supervisor, SSTATUS, xstatus);

            // Boilerplate
            csr_read(&core->csr, priv_supervisor, STVEC, &addr);
            core->next_pc = addr;
            core->privilege = priv_supervisor;

            core->sync_trap_pending = false;
            core->sync_trap_cause = 0;
            core->sync_tval = 0;
            return;
        } else {
            // xEPC xTVAL and xCAUSE
            csr_write(&core->csr, priv_machine, MEPC, core->pc);
            csr_write(&core->csr, priv_machine, MCAUSE, core->sync_trap_cause);
            csr_write(&core->csr, priv_machine, MTVAL, core->sync_tval);

            // xPP & xIE Stack
            csr_read(&core->csr, priv_machine, MSTATUS, &xstatus);
            xstatus &= ~(3 << MSTATUS_MPP_BIT);
            xstatus |= (uint32_t)(core->privilege & 0x3) << MSTATUS_MPP_BIT;
            ie = extract_bits(xstatus, MSTATUS_MIE_BIT, MSTATUS_MIE_BIT);
            xstatus &= ~(1 << MSTATUS_MIE_BIT);
            xstatus &= ~(1 << MSTATUS_MPIE_BIT);
            xstatus |= ie << MSTATUS_MPIE_BIT;
            // Clear MPRV when trap is taken to M-mode
            xstatus &= ~(1 << MSTATUS_MPRV_BIT);
            csr_write(&core->csr, priv_machine, MSTATUS, xstatus);

            // Boilerplate
            csr_read(&core->csr, priv_machine, MTVEC, &addr);
            core->next_pc = addr;
            core->privilege = priv_machine;

            core->sync_trap_pending = false;
            core->sync_trap_cause = 0;
            core->sync_tval = 0;
            return;
        }
    }
    csr_read(&core->csr, priv_machine, MSTATUS, &mstatus);
    csr_read(&core->csr, priv_machine, MIDELEG, &mideleg);
    csr_read(&core->csr, priv_machine, MIP, &mip);
    csr_read(&core->csr, priv_machine, MIE, &mie);

    mstatus_mie = (bool)extract_bits(mstatus, MSTATUS_MIE_BIT, MSTATUS_MIE_BIT);
    mstatus_sie = (bool)extract_bits(mstatus, MSTATUS_SIE_BIT, MSTATUS_SIE_BIT);

    // highest priority first, first match wins
    for (interrupt_cause = machine_exti; interrupt_cause >= supervisor_swi; interrupt_cause--) {
        mip_val = (bool)extract_bits(mip, interrupt_cause, interrupt_cause);
        mie_val = (bool)extract_bits(mie, interrupt_cause, interrupt_cause);
        int_val = mip_val && mie_val;
        mideleg_val = (bool)extract_bits(mideleg, interrupt_cause, interrupt_cause);

        if (!int_val)
            continue;

        if (!mideleg_val) {
            if ((core->privilege == priv_machine && !mstatus_mie) || core->privilege > priv_machine)
                continue;

            csr_write(&core->csr, priv_machine, MCAUSE, (uint32_t)interrupt_cause | (1 << 31));
            csr_write(&core->csr, priv_machine, MEPC, core->next_pc);
            csr_write(&core->csr, priv_machine, MTVAL, 0);

            // xPP & xIE Stack
            csr_read(&core->csr, priv_machine, MSTATUS, &xstatus);
            xstatus &= ~(3 << MSTATUS_MPP_BIT);
            xstatus |= (uint32_t)(core->privilege & 0x3) << MSTATUS_MPP_BIT;
            ie = extract_bits(xstatus, MSTATUS_MIE_BIT, MSTATUS_MIE_BIT);
            xstatus &= ~(1 << MSTATUS_MIE_BIT);
            xstatus &= ~(1 << MSTATUS_MPIE_BIT);
            xstatus |= ie << MSTATUS_MPIE_BIT;
            // Clear MPRV when trap is taken to M-mode
            xstatus &= ~(1 << MSTATUS_MPRV_BIT);
            csr_write(&core->csr, priv_machine, MSTATUS, xstatus);
            // Boilerplate
            csr_read(&core->csr, priv_machine, MTVEC, &addr);
            core->next_pc = addr;
            core->privilege = priv_machine;
            return;
        } else {
            if ((core->privilege == priv_supervisor && !mstatus_sie) ||
                core->privilege > priv_supervisor)
                continue;

            csr_write(&core->csr, priv_machine, SCAUSE, (uint32_t)interrupt_cause | (1 << 31));
            csr_write(&core->csr, priv_machine, SEPC, core->next_pc);
            csr_write(&core->csr, priv_machine, STVAL, 0);

            // xPP & xIE Stack
            csr_read(&core->csr, priv_machine, SSTATUS, &xstatus);
            xstatus &= ~(1 << MSTATUS_SPP_BIT);
            xstatus |= (uint32_t)(core->privilege & 0x1) << MSTATUS_SPP_BIT;
            ie = extract_bits(xstatus, MSTATUS_SIE_BIT, MSTATUS_SIE_BIT);
            xstatus &= ~(1 << MSTATUS_SIE_BIT);
            xstatus &= ~(1 << MSTATUS_SPIE_BIT);
            xstatus |= ie << MSTATUS_SPIE_BIT;
            csr_write(&core->csr, priv_machine, SSTATUS, xstatus);
            // Boilerplate
            csr_read(&core->csr, priv_machine, STVEC, &addr);
            core->next_pc = addr;
            core->privilege = priv_supervisor;

            return;
        }
    }
}
