/*
 * Copyright (C) 2014-2018 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <types.h>

#include <logbool.h>

#include <config.h>
#include <types.h>
#include <sched.h>
#include <event.h>

#include <device/irq.h>
#include <device/arch/gic.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/physdev.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/avz.h>

#include <soo/soo.h>

int do_physdev_op(int cmd, void *args)
{
	int val;
	struct domain *__current;
	addrspace_t prev_addrspace;
	send_ipi_args_t send_ipi_args;

	switch (cmd) {

	case PHYSDEVOP_dump_page:
	{
		memcpy(&val, args, sizeof(int));

		__current = current;
		get_current_addrspace(&prev_addrspace);
		switch_mm(idle_domain[smp_processor_id()], &idle_domain[smp_processor_id()]->addrspace);

		dump_page(val);

		switch_mm(__current, &prev_addrspace);

		break;
	}

	case PHYSDEVOP_dump_logbool:
	{
		/* Do not care about the parameter */
		dump_all_logbool(' ');

		break;
	}

	case PHYSDEVOP_send_ipi:

		memcpy(&send_ipi_args, args, sizeof(send_ipi_args_t));

		smp_cross_call(send_ipi_args.cpu_mask, send_ipi_args.ipinr);
		break;

	default:
		BUG();
		break;
	}

	return 0;
}
