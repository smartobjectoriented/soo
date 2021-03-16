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

#include <smp.h>
#include <io.h>
#include <spinlock.h>
#include <memory.h>
#include <time.h>

#include <asm/cacheflush.h>

#include <mach/vexpress.h>

#include <device/arch/gic.h>

extern void secondary_startup(void);
extern void vexpress_secondary_startup(void);

static DEFINE_SPINLOCK(boot_lock);


/*
 * Called from CPU #0
 */
void smp_boot_secondary(unsigned int cpu) {

	/*
	 * Set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	/*
	 * This is really belt and braces; we hold unintended secondary
	 * CPUs in the holding pen until we're ready for them.  However,
	 * since we haven't sent them a soft interrupt, they shouldn't
	 * be there.
	 */
	write_pen_release(cpu);

	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * the boot monitor to read the system wide flags register,
	 * and branch to the address found there.
	 */
	smp_cross_call(cpu, IPI_WAKEUP);

	do {
		dmb();
		if (pen_release == -1)
			break;

		udelay(1000);
	} while (1);

	gic_secondary_init(0);

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	if (pen_release != -1)
		BUG();

}

/* Secondary CPU kernel startup is a 2 step process. The primary CPU
 * starts the secondary CPU by giving it the address of the kernel and
 * then sending it an event to wake it up. The secondary CPU then
 * starts the kernel and tells the primary CPU it's up and running.
 *
 * platform_secondary_init() is called during the bootstrap on the second CPU and
 * is called by arch/arm/kernel/smp.c:secondary_start_kernel() which is itself called
 * by arch/arm/kernel/head.S: secondary_startup(), called by headsmp.S
 *
 */
void smp_secondary_init(unsigned int cpu)
{

	gic_secondary_init(0);

	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */
	write_pen_release(-1);

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

/*
 * Called from CPU #0
 */
void smp_prepare_cpus(unsigned int max_cpus)
{
	unsigned int sysreg_base;

	sysreg_base = (unsigned int) io_map(VEXPRESS_SYSREG_BASE, VEXPRESS_SYSREG_SIZE);
	if (!sysreg_base) {
		printk("!!!! BOOTUP jump vectors can't be used !!!!\n");
		BUG();
	}

	writel(~0, sysreg_base + SYS_FLAGSCLR);
	writel(virt_to_phys(vexpress_secondary_startup), sysreg_base + SYS_FLAGSSET);

};
