/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <avz/kernel.h>
#include <avz/init.h>
#include <avz/cpumask.h>

#include <mach/a64.h>

#include <asm/page.h>
#include <asm/time.h>
#include <asm/smp.h>

#include <asm/io.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/hardware/gic.h>
#include <asm/mach/irq.h>

#include <mach/system.h>

/* DEB: for debugging purposes */
#define SUNXI_GPIO_CTRL_REG_PHYS	0x01c20800
#define SUNXI_GPIO_PC_CFG_REG		(2 * 0x24)
#define SUNXI_GPIO_PC_DATA_REG		(2 * 0x24 + 0x10)
#define SUNXI_GPIO_REGS_SIZE		0x24


/* DEB: debugging with GPIO pins */
void __iomem *membase_gpio;

/* DEB: for debugging purposes, use the PG0-9 pins */
void sun50i_gpio_set(int pin, int value) {
	unsigned int regval;

	if ((pin < 0) || (pin > 7))
		return;

	regval = readl(membase_gpio + SUNXI_GPIO_PC_DATA_REG);

	if (value)
		writel(regval | (1 << pin), membase_gpio + SUNXI_GPIO_PC_DATA_REG);
	else
		writel(regval & (~(1 << pin)), membase_gpio + SUNXI_GPIO_PC_DATA_REG);
}

void ll_gpio_set(int pin, int value) {
	sun50i_gpio_set(pin, value);
}



static void __init sun50i_ll_gpio_init(void) {
	membase_gpio = ioremap(SUNXI_GPIO_CTRL_REG_PHYS, SUNXI_GPIO_REGS_SIZE);
	writel((1 << 8) | (1 << 12) | (1 << 16) | (1 << 28), membase_gpio + SUNXI_GPIO_PC_CFG_REG);
}

static void __init sun50i_map_io(void) {
	sun50i_ll_gpio_init();
}

static void __init sun50i_init_irq(void) {

	printk("sun50i GIQ IRQ init: mapping & initializing...\n");

	gic_init(0, 32, ioremap(SUNXI_GIC_DIST_PHYS, SUNXI_GIC_DIST_SIZE), ioremap(SUNXI_GIC_CPU_PHYS, SUNXI_GIC_CPU_SIZE));
}

static void __init sun50i_init(void) {
	printk("sun50i initializing board...\n");
}

MACHINE_START(SUN50I, "Allwinner sun50i (A64) Family")
	.smp		= smp_ops(sun50i_smp_ops),
	.map_io		= sun50i_map_io,
	.init_irq	= sun50i_init_irq,
	.init_machine	= sun50i_init,

MACHINE_END
