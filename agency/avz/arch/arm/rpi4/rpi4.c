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

#include <mach/rpi4.h>

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

static struct map_desc rpi4_io_desc[] __initdata = {
	{
		.virtual = RPI4_UART0_VIRT,
		.pfn = __phys_to_pfn(RPI4_UART0_PHYS),
		.length = SZ_4K,
		.type = MT_DEVICE,
	},
};

static void __init rpi4_map_io(void) {
	iotable_init(rpi4_io_desc, ARRAY_SIZE(rpi4_io_desc));
}

static void __init rpi4_init_irq(void) {

	printk("Raspberry Pi 4 Interrupt controller init: mapping & initializing...\n");
	printk("GIC Interrupt controller init: mapping & initializing...\n");

	gic_init(0, 29, ioremap(RPI4_GIC_DIST_PHYS, RPI4_GIC_DIST_SIZE), ioremap(RPI4_GIC_CPU_PHYS, RPI4_GIC_CPU_SIZE));
}

static void __init rpi4_init(void) {
	printk("Raspberry Pi 4 initializing board...\n");
}

MACHINE_START(RPI4, "Broadcomm Raspberry Pi 4 Model B")
	.smp		= smp_ops(rpi4_smp_ops),
	.map_io		= rpi4_map_io,
	.init_irq	= rpi4_init_irq,
	.init_machine	= rpi4_init,

MACHINE_END
