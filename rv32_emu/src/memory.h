// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Naveen Chavali

// Address map
#define ROM_BASE 0x40000000
#define ROM_SIZE 0xf000

#define RAM_BASE 0x80000000
#define RAM_SIZE (1024 * 1024 * 128) // 128MiB

#define UART_BASE 0x10000000
#define UART_SIZE 0x100

#define PLIC_BASE 0x30000000
#define PLIC_SIZE 0x3FFF004

#define CLINT_BASE 0x20000000
#define CLINT_SIZE 0x000c0000

#define LINUX_ADDR 0x80400000  // 4MiB from the base
#define FDT_ADDR 0x86000000    // 32MiB from the end
#define INITRD_ADDR 0x87000000 // 16MiB from the end

// Page table constants (Sv32)
#define SV32_LEVELS 2
#define SATP_MODE_BIT 31
#define SV32_MODE 0x01
#define PPN_MASK 0x00000000003fffffUL
#define PAGE_SIZE_SHIFT 12
#define VPN_MASK 0x3ff
#define OFFSET_MASK 0xfff

// PTE bits
#define PTE_V 0
#define PTE_R 1
#define PTE_W 2
#define PTE_X 3
#define PTE_U 4
#define PTE_G 5
#define PTE_A 6
#define PTE_D 7

#define PTE_RWX_MASK ((1 << PTE_R) | (1 << PTE_W) | (1 << PTE_X))
