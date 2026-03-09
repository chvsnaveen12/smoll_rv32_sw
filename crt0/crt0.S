// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Naveen Chavali

# Simple RISC-V crt0

.section .text.init
.global _start
.global _trap_handler
.global irq_count
.global enable_interrupts
.global disable_interrupts
.type _start, @function

# Memory Map Constants
.equ SRAM_BASE,   0x80000000
.equ SRAM_SIZE,   0x8000000
.equ SRAM_TOP,    0x87fffffc
#.equ SRAM_TOP,    0x80001000

_start:
    # Set up the global pointer (gp) for relaxed addressing
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    li sp, SRAM_TOP
    # Call main
    call main

    # If main returns, loop forever
.hang:
    j .hang