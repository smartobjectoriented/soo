/*
 * Copyright (C) 2005 Stelian Pop
 *
 * Xenomai is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Xenomai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xenomai; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#ifndef _COBALT_ARM_ASM_THREAD_H
#define _COBALT_ARM_ASM_THREAD_H

struct xnthread;
struct ipipe_trap_data;
struct xnarchtcb;
struct thread_info;

struct xnarchtcb {

	/* Need to be here at first since used by __asm_thread_switch */

	unsigned long   sp;
	unsigned long   pc;

	struct task_struct *task;

	unsigned long	start_pc;

};


void xnarch_switch_to(struct xnthread *out, struct xnthread *in);

/* Standard Linux RT task stack size */
#define XNTHREAD_STACK_SIZE 	(THREAD_SIZE)

void xnarch_cleanup_thread(struct xnthread *thread);
void xnarch_init_thread(struct xnthread *thread);

#endif /* !_COBALT_ARM_ASM_THREAD_H */
