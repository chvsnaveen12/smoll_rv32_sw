/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 MIPS
 *
 */

#include <sbi/riscv_asm.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_illegal_alu.h>
#include <sbi/sbi_illegal_insn.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_types.h>
#include <sbi/sbi_math.h>

#ifdef SMOLL_RV32
static double_result_t negate_double(double_result_t val)
{
    double_result_t result = {~val.low, ~val.high};
    
	result.low += 1;
    result.high += result.low == 0 ? 1 : 0;

    return result;
}

static int mul_insn(ulong insn, struct sbi_trap_regs *regs)
{
	ulong rs1 = GET_RS1(insn, regs);
	ulong rs2 = GET_RS2(insn, regs);

	double_result_t full_product = multiply_unsigned_with_loops(rs1, rs2);
	SET_RD(insn, regs, full_product.low);
	regs->mepc += 4;
	return 0;
}

static int mulh_insn(ulong insn, struct sbi_trap_regs *regs)
{
	bool negative_result = false;
    ulong abs_rs1, abs_rs2;

	long rs1 = GET_RS1(insn, regs);
	long rs2 = GET_RS2(insn, regs);
    
    if (rs1 < 0) {
        abs_rs1 = -rs1;
        negative_result = !negative_result;
    } else {
        abs_rs1 = rs1;
    }
    
    if (rs2 < 0) {
        abs_rs2 = -rs2;
        negative_result  = !negative_result;
    } else {
        abs_rs2 = rs2;
    }

	double_result_t full_product = multiply_unsigned_with_loops(abs_rs1, abs_rs2);

	if (negative_result) {
        full_product = negate_double(full_product);
    }

	SET_RD(insn, regs, full_product.high);
	regs->mepc += 4;
	return 0;
}

static int mulhsu_insn(ulong insn, struct sbi_trap_regs *regs)
{
	bool negative_result = false;
    ulong abs_rs1;

	long rs1 = GET_RS1(insn, regs);
	ulong rs2 = GET_RS2(insn, regs);
    
    if (rs1 < 0) {
        abs_rs1 = -rs1;
        negative_result = !negative_result;
    } else {
        abs_rs1 = rs1;
    }

	double_result_t full_product = multiply_unsigned_with_loops(abs_rs1, rs2);

	if (negative_result) {
        full_product = negate_double(full_product);
    }

	SET_RD(insn, regs, full_product.high);
	regs->mepc += 4;
	return 0;
}

static int mulhu_insn(ulong insn, struct sbi_trap_regs *regs)
{
	ulong rs1 = GET_RS1(insn, regs);
	ulong rs2 = GET_RS2(insn, regs);

	double_result_t full_product = multiply_unsigned_with_loops(rs1, rs2);

	SET_RD(insn, regs, full_product.high);
	regs->mepc += 4;
	return 0;
}

static int div_insn(ulong insn, struct sbi_trap_regs *regs)
{
	bool negative_result = false;
    ulong abs_rs1, abs_rs2;

	long rs1 = GET_RS1(insn, regs);
	long rs2 = GET_RS2(insn, regs);

	if(rs1 == 0x80000000 && rs2 == -1) {
		SET_RD(insn, regs, rs1);
		regs->mepc += 4;
		return 0;
	}

	if (rs1 < 0) {
        abs_rs1 = -rs1;
        negative_result = !negative_result;
    } else {
        abs_rs1 = rs1;
    }
    
    if (rs2 < 0) {
        abs_rs2 = -rs2;
        negative_result  = !negative_result;
    } else {
        abs_rs2 = rs2;
    }

	div_result_t result = divide_unsigned_with_loops(abs_rs1, abs_rs2);

	if (negative_result) {
		result.quotient = -result.quotient;
		// result.remainder = -result.remainder;
	}
	SET_RD(insn, regs, result.quotient);
	regs->mepc += 4;
	return 0;
}

static int divu_insn(ulong insn, struct sbi_trap_regs *regs)
{
	ulong rs1 = GET_RS1(insn, regs);
	ulong rs2 = GET_RS2(insn, regs);

	if(rs1 == 0x80000000 && rs2 == -1) {
		SET_RD(insn, regs, rs1);
		regs->mepc += 4;
		return 0;
	}

	div_result_t result = divide_unsigned_with_loops(rs1, rs2);
	SET_RD(insn, regs, result.quotient);
	regs->mepc += 4;
	return 0;
}

static int rem_insn(ulong insn, struct sbi_trap_regs *regs)
{
	bool negative_result = false;
    ulong abs_rs1, abs_rs2;

	long rs1 = GET_RS1(insn, regs);
	long rs2 = GET_RS2(insn, regs);

	if(rs1 == 0x80000000 && rs2 == -1) {
		SET_RD(insn, regs, rs1);
		regs->mepc += 4;
		return 0;
	}

	if (rs1 < 0) {
        abs_rs1 = -rs1;
        negative_result = !negative_result;
    } else {
        abs_rs1 = rs1;
    }
    
    if (rs2 < 0) {
        abs_rs2 = -rs2;
        negative_result  = !negative_result;
    } else {
        abs_rs2 = rs2;
    }

	div_result_t result = divide_unsigned_with_loops(abs_rs1, abs_rs2);

	if (negative_result) {
		// result.quotient = -result.quotient;
		result.remainder = -result.remainder;
	}
	SET_RD(insn, regs, result.remainder);
	regs->mepc += 4;
	return 0;
}

static int remu_insn(ulong insn, struct sbi_trap_regs *regs)
{
	ulong rs1 = GET_RS1(insn, regs);
	ulong rs2 = GET_RS2(insn, regs);

	if(rs1 == 0x80000000 && rs2 == -1) {
		SET_RD(insn, regs, rs1);
		regs->mepc += 4;
		return 0;
	}

	div_result_t result = divide_unsigned_with_loops(rs1, rs2);
	SET_RD(insn, regs, result.remainder);
	regs->mepc += 4;
	return 0;
}

static const illegal_insn_func alu_insn_table[8] = {
	mul_insn, /* 0 */
	mulh_insn, /* 1 */
	mulhsu_insn, /* 2 */
	mulhu_insn, /* 3 */
	div_insn, /* 4 */
	divu_insn, /* 5 */
	rem_insn, /* 6 */
	remu_insn, /* 7 */
};

int sbi_illegal_alu(ulong insn, struct sbi_trap_regs *regs)
{
	if(GET_FUNC7(insn) != 1)
  		return truly_illegal_insn(insn, regs);
	return alu_insn_table[GET_FUNC3(insn)](insn, regs);
}
#endif