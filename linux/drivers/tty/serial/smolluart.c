// SPDX-License-Identifier: GPL-2.0
/*
 * smolluart.c: Serial driver for SmollUART serial controller
 *
 * Based on 8250 driver patterns, adapted for SmollUART register map:
 * 0x00: RX Data (Read)
 * 0x04: RX Status - bit 0 = data available (Read)
 * 0x08: TX Data (Write)
 * 0x0C: TX Status - bit 0 = busy (Read)
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/delay.h>

#define SM_UART_NAME		"ttyS"
#define SM_UART_MAJOR		4
#define SM_UART_MINOR		64
#define SM_UART_NR_UARTS	1

/* Register offsets - SmollUART specific */
#define SM_UART_RX		0x00
#define SM_UART_RX_STATUS	0x04
#define SM_UART_TX		0x08
#define SM_UART_TX_STATUS	0x0c

/* Status bits */
#define SM_UART_RX_VALID	0x01
#define SM_UART_TX_BUSY		0x01

/* Custom port type */
#define PORT_SMOLLUART		100

static struct uart_port sm_uart_ports[SM_UART_NR_UARTS];
static struct uart_driver sm_uart_driver;

/*
 * Low-level I/O helpers
 */
static inline u32 sm_uart_read(struct uart_port *port, int offset)
{
	return readl(port->membase + offset);
}

static inline void sm_uart_write(struct uart_port *port, int offset, u32 val)
{
	writel(val, port->membase + offset);
}

/*
 * Wait for TX to be ready (not busy), with timeout
 */
static int sm_uart_wait_for_xmitr(struct uart_port *port)
{
	int count = 10000;
	
	while ((sm_uart_read(port, SM_UART_TX_STATUS) & SM_UART_TX_BUSY) && --count)
		cpu_relax();
	
	return count > 0;
}

/*
 * UART operations
 */
static unsigned int sm_uart_tx_empty(struct uart_port *port)
{
	return (sm_uart_read(port, SM_UART_TX_STATUS) & SM_UART_TX_BUSY) ? 0 : TIOCSER_TEMT;
}

static unsigned int sm_uart_get_mctrl(struct uart_port *port)
{
	return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
}

static void sm_uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static void sm_uart_stop_tx(struct uart_port *port)
{
}

static void sm_uart_start_tx(struct uart_port *port)
{
	unsigned char ch;

	while (sm_uart_wait_for_xmitr(port)) {
		if (port->x_char) {
			sm_uart_write(port, SM_UART_TX, port->x_char);
			port->x_char = 0;
			port->icount.tx++;
			continue;
		}
		if (uart_tx_stopped(port) || !uart_fifo_get(port, &ch))
			break;

		sm_uart_write(port, SM_UART_TX, ch);
		port->icount.tx++;
	}
}

static void sm_uart_stop_rx(struct uart_port *port)
{
}

static void sm_uart_break_ctl(struct uart_port *port, int ctl)
{
}

static irqreturn_t sm_uart_isr(int irq, void *dev_id)
{
	struct uart_port *port = dev_id;
	struct tty_port *tport = &port->state->port;
	unsigned long flags;
	unsigned char ch;
	int busy = 0;

	uart_port_lock_irqsave(port, &flags);

	/* Receive all available characters */
	while (sm_uart_read(port, SM_UART_RX_STATUS) & SM_UART_RX_VALID) {
		ch = sm_uart_read(port, SM_UART_RX);
		port->icount.rx++;
		tty_insert_flip_char(tport, ch, TTY_NORMAL);
		busy = 1;
	}

	/* Transmit pending characters */
	sm_uart_start_tx(port);

	uart_port_unlock_irqrestore(port, flags);

	if (busy)
		tty_flip_buffer_push(tport);

	return IRQ_HANDLED;
}

static int sm_uart_startup(struct uart_port *port)
{
	return request_irq(port->irq, sm_uart_isr, IRQF_SHARED, "smolluart", port);
}

static void sm_uart_shutdown(struct uart_port *port)
{
	free_irq(port->irq, port);
}

static void sm_uart_set_termios(struct uart_port *port, struct ktermios *termios,
				const struct ktermios *old)
{
	unsigned long flags;
	unsigned int baud;

	baud = uart_get_baud_rate(port, termios, old, 0, port->uartclk / 16);
	uart_port_lock_irqsave(port, &flags);
	uart_update_timeout(port, termios->c_cflag, baud);
	uart_port_unlock_irqrestore(port, flags);
}

static const char *sm_uart_type(struct uart_port *port)
{
	return port->type == PORT_SMOLLUART ? "SmollUART" : NULL;
}

static int sm_uart_request_port(struct uart_port *port)
{
	/* Already setup in probe */
	return 0;
}

static void sm_uart_release_port(struct uart_port *port)
{
}

static void sm_uart_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE)
		port->type = PORT_SMOLLUART;
}

static int sm_uart_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	return -EINVAL;
}

static const struct uart_ops sm_uart_ops = {
	.tx_empty	= sm_uart_tx_empty,
	.set_mctrl	= sm_uart_set_mctrl,
	.get_mctrl	= sm_uart_get_mctrl,
	.stop_tx	= sm_uart_stop_tx,
	.start_tx	= sm_uart_start_tx,
	.stop_rx	= sm_uart_stop_rx,
	.break_ctl	= sm_uart_break_ctl,
	.startup	= sm_uart_startup,
	.shutdown	= sm_uart_shutdown,
	.set_termios	= sm_uart_set_termios,
	.type		= sm_uart_type,
	.release_port	= sm_uart_release_port,
	.request_port	= sm_uart_request_port,
	.config_port	= sm_uart_config_port,
	.verify_port	= sm_uart_verify_port,
};

/*
 * Console support - this is the critical part for early output
 */
#ifdef CONFIG_SERIAL_SMOLL_UART_CONSOLE

static void sm_uart_console_putchar(struct uart_port *port, unsigned char ch)
{
	sm_uart_wait_for_xmitr(port);
	sm_uart_write(port, SM_UART_TX, ch);
}

static void sm_uart_console_write(struct console *co, const char *s,
				  unsigned int count)
{
	struct uart_port *port = &sm_uart_ports[co->index];
	unsigned long flags;
	int locked = 1;

	if (!port->membase)
		return;

	if (oops_in_progress)
		locked = uart_port_trylock_irqsave(port, &flags);
	else
		uart_port_lock_irqsave(port, &flags);

	uart_console_write(port, s, count, sm_uart_console_putchar);

	if (locked)
		uart_port_unlock_irqrestore(port, flags);
}

static int sm_uart_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	pr_info("SmollUART: console_setup called for index %d\n", co->index);

	if (co->index < 0 || co->index >= SM_UART_NR_UARTS)
		co->index = 0;

	port = &sm_uart_ports[co->index];
	if (!port->membase) {
		pr_err("SmollUART: console_setup failed - membase is NULL\n");
		return -ENODEV;
	}

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct console sm_uart_console = {
	.name	= SM_UART_NAME,
	.write	= sm_uart_console_write,
	.device	= uart_console_device,
	.setup	= sm_uart_console_setup,
	.flags	= CON_PRINTBUFFER,
	.index	= -1,
	.data	= &sm_uart_driver,
};

static int __init sm_uart_console_init(void)
{
	pr_info("SmollUART: console_init called\n");
	register_console(&sm_uart_console);
	return 0;
}
console_initcall(sm_uart_console_init);

#define SM_UART_CONSOLE	(&sm_uart_console)
#else
#define SM_UART_CONSOLE	NULL
#endif

/*
 * UART driver structure
 */
static struct uart_driver sm_uart_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= "smolluart",
	.dev_name	= SM_UART_NAME,
	.major		= SM_UART_MAJOR,
	.minor		= SM_UART_MINOR,
	.nr		= SM_UART_NR_UARTS,
	.cons		= SM_UART_CONSOLE,
};

/*
 * Platform driver
 */
static int sm_uart_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct uart_port *port;
	int irq, ret;

	pr_info("SmollUART: probe called\n");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	port = &sm_uart_ports[0];
	
	/* Map the registers early so console can use them */
	port->membase = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!port->membase)
		return -ENOMEM;

	port->mapbase = res->start;
	port->irq = irq;
	port->iotype = UPIO_MEM;
	port->flags = UPF_BOOT_AUTOCONF | UPF_FIXED_PORT | UPF_FIXED_TYPE;
	port->dev = &pdev->dev;
	port->uartclk = 100000000;
	port->ops = &sm_uart_ops;
	port->line = 0;
	port->type = PORT_SMOLLUART;
	port->fifosize = 1;

	dev_info(&pdev->dev, "SmollUART at 0x%08llx, irq %d\n",
		 (unsigned long long)res->start, irq);

	ret = uart_add_one_port(&sm_uart_driver, port);
	if (ret) {
		dev_err(&pdev->dev, "uart_add_one_port failed: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, port);
	return 0;
}

static void sm_uart_remove(struct platform_device *pdev)
{
	struct uart_port *port = platform_get_drvdata(pdev);

	if (port)
		uart_remove_one_port(&sm_uart_driver, port);
}

static const struct of_device_id sm_uart_of_match[] = {
	{ .compatible = "smolluart", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sm_uart_of_match);

static struct platform_driver sm_uart_platform_driver = {
	.probe		= sm_uart_probe,
	.remove		= sm_uart_remove,
	.driver		= {
		.name	= "smolluart",
		.of_match_table = sm_uart_of_match,
	},
};

static int __init sm_uart_init(void)
{
	int ret;

	pr_info("SmollUART: init called\n");

	ret = uart_register_driver(&sm_uart_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&sm_uart_platform_driver);
	if (ret)
		uart_unregister_driver(&sm_uart_driver);

	return ret;
}

static void __exit sm_uart_exit(void)
{
	platform_driver_unregister(&sm_uart_platform_driver);
	uart_unregister_driver(&sm_uart_driver);
}

module_init(sm_uart_init);
module_exit(sm_uart_exit);

MODULE_DESCRIPTION("SmollUART serial driver");
MODULE_LICENSE("GPL");
