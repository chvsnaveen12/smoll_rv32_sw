/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Naveen Chavali
 *
 * Authors:
 *   Naveen Chavali <chvsnaveen12@gmail.com>
 */

#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_domain.h>
#include <sbi_utils/serial/smolluart.h>

/* SmollUART Register Offsets */
#define SMOLLUART_RX_DATA	0x00	/* RX Data Register */
#define SMOLLUART_RX_IRQ	0x04	/* RX IRQ Status */
#define SMOLLUART_TX_DATA	0x08	/* TX Data Register */
#define SMOLLUART_TX_BUSY	0x0C	/* TX Busy Status */

static volatile void *smolluart_base;

static u32 smolluart_read(u32 offset)
{
	return readl(smolluart_base + offset);
}

static void smolluart_write(u32 offset, u32 val)
{
	writel(val, smolluart_base + offset);
}

static void smolluart_putc(char ch)
{
	/* Wait until TX is not busy */
	while (smolluart_read(SMOLLUART_TX_BUSY) & 0x1)
		;

	/* Write character to TX data register */
	smolluart_write(SMOLLUART_TX_DATA, ch);

	/* Trigger send by writing 1 to TX busy register */
	// smolluart_write(SMOLLUART_TX_BUSY, 1);
}

static int smolluart_getc(void)
{
	/* Check if RX data available (IRQ flag set) */
	if (smolluart_read(SMOLLUART_RX_IRQ) & 0x1) {
		return smolluart_read(SMOLLUART_RX_DATA) & 0xFF;
	}
	return -1;
}

static struct sbi_console_device smolluart_console = {
	.name = "smolluart",
	.console_putc = smolluart_putc,
	.console_getc = smolluart_getc
};

int smolluart_init(unsigned long base)
{
	smolluart_base = (volatile void *)base;

	sbi_console_set_device(&smolluart_console);

	return sbi_domain_root_add_memrange(base, PAGE_SIZE, PAGE_SIZE,
					    (SBI_DOMAIN_MEMREGION_MMIO |
					    SBI_DOMAIN_MEMREGION_SHARED_SURW_MRW));
}
