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

#include <avz/types.h>

#include <logbool.h>

#include <avz/config.h>
#include <avz/init.h>
#include <avz/lib.h>
#include <avz/types.h>
#include <avz/sched.h>
#include <avz/irq.h>
#include <avz/event.h>

#include <asm/current.h>
#include <asm/page.h>
#include <asm/domain.h>
#include <asm/hypercall.h>
#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <asm/smp.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/physdev.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/avz.h>

#include <soo/soo.h>

#include <asm/system.h>

int do_physdev_op(int cmd, void *args)
{
	int val;

	switch (cmd) {

	case PHYSDEVOP_dump_page:
	{
		memcpy(&val, args, sizeof(int));

		switch_mm(NULL, idle_domain[smp_processor_id()]->vcpu[0]);

		dump_page(val);

		switch_mm(idle_domain[smp_processor_id()]->vcpu[0], NULL);

		break;
	}

	case PHYSDEVOP_dump_logbool:
	{
		/* Do not care about the parameter */
		dump_all_logbool(' ');

		break;
	}

	default:
		BUG();
		break;
	}

	return 0;
}
