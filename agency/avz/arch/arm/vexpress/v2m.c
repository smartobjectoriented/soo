/*
 * Copyright (C) 2016,2017 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <mach/vexpress.h>
#include <asm/page.h>
#include <asm/time.h>
#include <asm/smp.h>

#include <avz/spinlock.h>

#include <asm/io.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>

#include <asm/hardware/gic.h>
#include <asm/hardware/arm_timer.h>

#include <mach/motherboard.h>

#include "core.h"

#define V2M_PA_CS0	0x40000000
#define V2M_PA_CS1	0x44000000
#define V2M_PA_CS2	0x48000000
#define V2M_PA_CS3	0x4c000000
#define V2M_PA_CS7	0x10000000


static struct map_desc v2m_io_desc[] __initdata = {
	{
		.virtual	= V2M_PERIPH,
		.pfn		= __phys_to_pfn(V2M_PA_CS7),
		.length		= SZ_128K,
		.type		= MT_DEVICE,
	},
	{
		.virtual = 	VEXPRESS_UART0_VIRT,
		.pfn = __phys_to_pfn(VEXPRESS_UART0_PHYS),
		.length = SZ_4K,
		.type = MT_DEVICE,
	},
};


static void __init v2m_map_io(void)
{
	iotable_init(v2m_io_desc, ARRAY_SIZE(v2m_io_desc));
}


static void __init v2m_init_irq(void)
{
	printk("Vexpress GIQ IRQ init: mapping & initializing...\n");

	gic_init(0, 29, ioremap(VEXPRESS_GIC_DIST_PHYS, VEXPRESS_GIC_DIST_SIZE), ioremap(VEXPRESS_GIC_CPU_PHYS, VEXPRESS_GIC_CPU_SIZE));
}

static void __init v2m_init(void)
{

}

MACHINE_START(VEXPRESS, "ARM-Versatile Express")
  .smp		= smp_ops(vexpress_smp_ops),
  .map_io	= v2m_map_io,
  .init_irq	= v2m_init_irq,
  .init_machine	= v2m_init,
MACHINE_END
