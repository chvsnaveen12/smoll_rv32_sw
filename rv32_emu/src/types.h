// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Naveen Chavali

#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum priv_level {
    priv_user = 0,
    priv_supervisor = 1,
    priv_hypervisor = 2,
    priv_machine = 3,
} priv_level_td;

typedef enum bus_access { bus_read = 0, bus_write = 1, bus_fetch = 2 } bus_access;

typedef enum trap_cause_interrupt {
    rsvd_0 = 0,
    supervisor_swi,
    rsvd_1,
    machine_swi,
    rsvd_2,
    supervisor_ti,
    rsvd_3,
    machine_ti,
    rsvd_4,
    supervisor_exti,
    rsvd_5,
    machine_exti
} trap_cause_interrupt;

typedef enum trap_cause_exception {
    instr_addr_misalign = 0,
    instr_access_fault,
    illegal_instr,
    breakpoint,
    load_addr_misalig,
    load_access_fault,
    str_amo_addr_misalign,
    str_amo_access_fault,
    user_ecall,
    supervisor_ecall,
    rsvd_6,
    machine_ecall,
    instr_page_fault,
    load_page_fault,
    rsvd_7,
    str_amo_page_fault
} trap_cause_exception;

// CSRs are a flat struct; reads/writes go through csr_read/csr_write for privilege checks.
typedef struct csr {
    uint32_t mstatus;
    uint32_t medeleg;
    uint32_t mideleg;
    uint32_t mie;
    uint32_t mtvec;

    uint32_t mscratch;
    uint32_t mepc;
    uint32_t mcause;
    uint32_t mtval;
    uint32_t mip;

    uint32_t stvec;

    uint32_t sscratch;
    uint32_t sepc;
    uint32_t scause;
    uint32_t stval;

    uint32_t satp;
} csr_td;

// soc_ptr is a void* to avoid a circular include with soc_td.
typedef struct core {
    // Core registers
    priv_level_td privilege;
    uint32_t pc;
    uint32_t next_pc;
    uint32_t regs[32];

    // Instruction properties
    uint32_t inst;
    uint8_t opcode;
    uint8_t rs1;
    uint8_t rs2;
    uint8_t rd;
    uint8_t funct3;
    uint8_t funct7;
    uint8_t shamt;
    bool bit25;
    bool bit30;

    // Sexted immediates
    uint32_t imm_u;
    uint32_t imm_j;
    uint32_t imm_i;
    uint32_t imm_s;
    uint32_t imm_b;

    // This is set after execution
    bool invalid;

    // Trap properties
    bool sync_trap_pending;
    uint32_t sync_trap_cause;
    uint32_t sync_tval;

    // Pointer to the SOC struct
    void *soc_ptr;

    // CSRs
    csr_td csr;

    // Debug
    uint64_t cycle;
} core_td;

typedef struct uart {
    // Simple circular FIFO for RX. Filled by the reader thread, drained by the emulator.
    uint8_t fifo_buf[16];
    int64_t fifo_read_ptr;
    int64_t fifo_write_ptr;
} simple_uart_td;

#define PLIC_PENDING_REGS 1
#define PLIC_ENABLE_REGS 1
#define PLIC_CLAIMED_REGS 1

// Priorities are fixed at 1 and thresholds fixed at 0, so neither needs storage.
typedef struct plic {
    uint32_t pending[PLIC_PENDING_REGS];
    uint32_t enable0[PLIC_ENABLE_REGS];
    uint32_t enable1[PLIC_ENABLE_REGS];
    uint32_t claim_complete0;
    uint32_t claim_complete1;
    // Internal
    uint32_t claimed[PLIC_CLAIMED_REGS];
} plic_td;

typedef struct clint {
    uint32_t msip;
    uint64_t mtimecmp;
    uint64_t mtime;
    uint64_t cycle;
} clint_td;

typedef struct soc {
    core_td core;
    uint8_t *rom;
    uint8_t *ram;

    simple_uart_td uart;
    plic_td plic;
    clint_td clint;
} soc_td;

#endif
