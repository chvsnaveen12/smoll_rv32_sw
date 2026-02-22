#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef enum priv_level{
    priv_user = 0,
    priv_supervisor = 1,
    priv_hypervisor = 2,
    priv_machine = 3,
} priv_level_td;

typedef enum bus_access{
    bus_read = 0,
    bus_write = 1,
    bus_fetch = 2,
    MAX = 3
} bus_access;

typedef enum trap_cause_interrupt{
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

// typedef struct simple_uart{
//     // RX Fifo
//     uint8_t fifo_buf[16];
//     int64_t fifo_read_ptr;
//     int64_t fifo_write_ptr;
// } simple_uart_td;

typedef enum trap_cause_exception{
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

typedef struct csr{
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

// Forward declaration for mmu_td
// #include "mmu.h"

typedef struct core{
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

    // MMU
    // mmu_td mmu;

    // Debug
    uint64_t cycle;
} core_td;

typedef struct uart{
    // 16550
    uint8_t dll;
    uint8_t dlm;
    uint8_t lcr;
    uint8_t mcr;
    uint8_t scr;
    uint8_t iir;
    uint8_t ier;
    uint8_t fcr;

    // RX Fifo
    uint8_t fifo_buf[16];
    int64_t fifo_read_ptr;
    int64_t fifo_write_ptr;

    // Sys
    // struct termios term_config; // termios is internal to uart.c implementation usually, but let's see if we need it here. 
    // In the original it was in the struct. But types.h might not have termios.h included.
    // Let's keep it opaque or use void* if needed, or just include it in uart.c if it's not needed in public interface.
    // Actually, looking at original uart.c, it uses pthread and termios. 
    // types.h is included in defs.h which is included everywhere. 
    // To avoid polluting types.h with POSIX headers, I'll keep system specific stuff in uart.c or use void* here.
    // But for now, let's just copy the registers and fifo. System stuff can be static in uart.c or hidden.
    // Wait, the original code had them in the struct.
    // Let's add the registers first.
    
    // Internal state for interrupts
    uint8_t thre_int;
} simple_uart_td;

#define PLIC_PENDING_REGS 1
#define PLIC_PRIO_REGS 32
#define PLIC_ENABLE_REGS 1
#define PLIC_CLAIMED_REGS 1

typedef struct plic{
    uint32_t pending[PLIC_PENDING_REGS];
    uint32_t priority[PLIC_PRIO_REGS];

    uint32_t enable0[PLIC_ENABLE_REGS];
    uint32_t enable1[PLIC_ENABLE_REGS];

    uint32_t claim_complete0;
    uint32_t threshold0;

    uint32_t claim_complete1;
    uint32_t threshold1;

    // Internal
    uint32_t claimed[PLIC_CLAIMED_REGS];
} plic_td;

typedef struct clint{
    uint32_t msip;
    uint64_t mtimecmp;
    uint64_t mtime;
    uint64_t cycle;
} clint_td;

struct virtq_desc{
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[1024];
}__attribute__((packed));

struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
}__attribute__((packed));

struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[1024];
}__attribute__((packed));

typedef struct blk{
    uint32_t status;
    uint32_t device_features_sel;
    uint32_t queue_sel;
    uint32_t queue_num;
    uint32_t queue_ready;
    uint32_t queue_notify;

    uint32_t queue_desc_low;
    uint32_t queue_avail_low;
    uint32_t queue_used_low;

    // Internal
    struct virtq_desc* desc_ptr;
    struct virtq_avail* avail_ptr;
    struct virtq_used* used_ptr;

    uint8_t *disk;
    uint8_t *ram;
    
    void *soc_ptr;
} blk_td;

typedef struct soc{
    core_td core;
    uint8_t *rom;
    uint8_t *ram;

    // uart_td uart;
    simple_uart_td uart;
    plic_td plic;
    clint_td clint;
    blk_td blk;
} soc_td;

#endif