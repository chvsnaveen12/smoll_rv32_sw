/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * 
 *
 */

#ifdef SMOLL_RV32
#ifndef __SBI_ILLEGAL_ALU_H__
#define __SBI_ILLEGAL_ALU_H__

#include <sbi/sbi_types.h>

struct sbi_trap_regs;

int sbi_illegal_alu(ulong insn, struct sbi_trap_regs *regs);

#endif
#endif