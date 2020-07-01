/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
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

#if 0
#define DEBUG
#endif

#include <common.h>

#include <device/device.h>
#include <device/driver.h>
#include <device/irq.h>

#include <device/arch/gic.h>

#include <asm/io.h>

#include <soo/evtchn.h>

/*
 * Interface to generic handling in irq.c
 */

static void dynirq_handle(cpu_regs_t *regs) {

	/* Perform the VIRQ interrupt processing */
	evtchn_do_upcall(regs);

}

void mask_dynirq(uint32_t irq) {
	mask_evtchn(evtchn_from_irq(irq));
}

void unmask_dynirq(uint32_t irq) {
	unmask_evtchn(evtchn_from_irq(irq));
}

static void enable_dynirq(unsigned int irq) {
	unmask_dynirq(irq);
}

static void disable_dynirq(unsigned int irq) {
	mask_dynirq(irq);
}

static int so3virt_irq_init(dev_t *dev) {

	DBG("%s\n", __FUNCTION__);

	DBG("%s 0x%08x bit %d\n", __func__, (unsigned int) &regs->gicd_spendsgirn[54/32], 54 % 32);

	irq_ops.irq_mask = mask_dynirq;
	irq_ops.irq_unmask = unmask_dynirq;

	irq_ops.irq_enable = enable_dynirq;
	irq_ops.irq_disable = disable_dynirq;

	irq_ops.irq_handle = dynirq_handle;

	return 0;
}

REGISTER_DRIVER_POSTCORE("so3virt-irq", so3virt_irq_init);
