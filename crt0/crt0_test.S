// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Naveen Chavali

# Minimal UART test - no stack, no C, no GP
# Just bang the UART directly from assembly

.section .text.init
.global _start
.type _start, @function

.equ UART_BASE, 0x10000000
.equ UART_TX,   0x08
.equ UART_BUSY, 0x0C

_start:
    li  t0, UART_BASE

    li  t1, 'H'
    jal ra, uart_putc
    li  t1, 'i'
    jal ra, uart_putc
    li  t1, '\r'
    jal ra, uart_putc
    li  t1, '\n'
    jal ra, uart_putc

.hang:
    j .hang

# t0 = UART_BASE, t1 = char, t2 = scratch
uart_putc:
1:  lw  t2, UART_BUSY(t0)
    bnez t2, 1b
    sw  t1, UART_TX(t0)
    ret
