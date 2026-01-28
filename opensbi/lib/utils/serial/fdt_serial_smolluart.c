/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Naveen Chavali
 *
 * Authors:
 *   Naveen Chavali <chvsnaveen12@gmail.com>
 */

#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/serial/fdt_serial.h>
#include <sbi_utils/serial/smolluart.h>

static int serial_smolluart_init(const void *fdt, int nodeoff,
				 const struct fdt_match *match)
{
	int rc;
	struct platform_uart_data uart = { 0 };

	rc = fdt_parse_uart_node(fdt, nodeoff, &uart);
	if (rc)
		return rc;

	return smolluart_init(uart.addr);
}

static const struct fdt_match serial_smolluart_match[] = {
	{ .compatible = "smolluart" },
	{ },
};

const struct fdt_driver fdt_serial_smolluart = {
	.match_table = serial_smolluart_match,
	.init = serial_smolluart_init,
};
