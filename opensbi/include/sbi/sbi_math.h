/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Atish Patra <atish.patra@wdc.com>
 */

#ifndef __SBI_MATH_H__
#define __SBI_MATH_H__

#include <sbi/sbi_types.h>

#ifdef SMOLL_RV32
typedef struct {
    ulong low;
    ulong high;
} double_result_t;

typedef struct {
    ulong quotient;
    ulong remainder;
} div_result_t;


double_result_t multiply_unsigned_with_loops(ulong a, ulong b);
div_result_t divide_unsigned_with_loops(ulong dividend, ulong divisor);
#endif

unsigned long log2roundup(unsigned long x);

#endif
