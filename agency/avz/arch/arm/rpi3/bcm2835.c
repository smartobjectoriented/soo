/*
 * Copyright (C) 2018 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <mach/bcm2835.h>
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

static struct map_desc bcm2835_io_desc[] __initdata = {
	{
		.virtual = BCM2835_UART0_VIRT,
		.pfn = __phys_to_pfn(BCM2835_UART0_PHYS),
		.length = SZ_4K,
		.type = MT_DEVICE,
	},
};

static void __init bcm2835_map_io(void) {
	iotable_init(bcm2835_io_desc, ARRAY_SIZE(bcm2835_io_desc));
}

static void __init bcm2835_init_irq(void) {

	printk("bcm2835 Interrupt controller init: mapping & initializing...\n");
	bcm2836_arm_irqchip_l1_intc_init(ioremap(BCM2836_INTC_PHYS, BCM2836_INTC_SIZE));
}

static void __init bcm2835_init(void) {
	printk("BCM2835 RPI 3 Model B initializing board...\n");
}

MACHINE_START(BCM2835, "Broadcomm BCM 2835 RPI 3 Model B")
	.smp		= smp_ops(bcm2836_smp_ops),
	.map_io		= bcm2835_map_io,
	.init_irq	= bcm2835_init_irq,
	.init_machine	= bcm2835_init,

MACHINE_END
