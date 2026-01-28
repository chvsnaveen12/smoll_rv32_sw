/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Common helper functions used across OpenSBI project.
 *
 * Authors:
 *   Atish Patra <atish.patra@wdc.com>
 */
#include <sbi/sbi_math.h>
#include <sbi/sbi_types.h>
#include <sbi/sbi_bitops.h>

unsigned long log2roundup(unsigned long x)
{
	unsigned long ret = 0;

	while (ret < __riscv_xlen) {
		if (x <= (1UL << ret))
			break;
		ret++;
	}

	return ret;
}

#ifdef SMOLL_RV32
double_result_t multiply_unsigned_with_loops(ulong a, ulong b)
{
    double_result_t result = {0, 0};
	double_result_t multiplicand = {a, 0};
	ulong carry = 0;
    
    while (b) {
        if (b & 1) {
			result.low = multiplicand.low + result.low;
            carry = result.low < multiplicand.low ? 1 : 0;
            result.high += multiplicand.high + carry;
		}
        
		//  Shift multiplicand left by 1 bit
        multiplicand.high = (multiplicand.high << 1) | (multiplicand.low >> (BITS_PER_LONG - 1));
        multiplicand.low <<= 1;
        
        b >>= 1;
    }
    return result;
}

div_result_t divide_unsigned_with_loops(ulong dividend, ulong divisor)
{
    div_result_t result = {0, 0};
    
    if (divisor == 0) {
        result.quotient = -1;
        result.remainder = dividend;
        return result;
    }

    if (dividend < divisor) {
        result.quotient = 0;
        result.remainder = dividend;
        return result;
    }

    for (int i = BITS_PER_LONG - 1; i >= 0; i--) {
        result.remainder = (result.remainder << 1) | ((dividend >> i) & 1);
        result.quotient <<= 1;
        
        if (result.remainder >= divisor) {
            result.remainder -= divisor;
            result.quotient |= 1;
        }
    }
	return result;
}

#endif
