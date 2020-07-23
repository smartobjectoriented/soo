
/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/major.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/serial_core.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/irq_regs.h>

#include <soo/evtchn.h>
#include <soo/hypervisor.h>
#include <soo/avzcons.h>
#include <soo/dev/vfb.h>
#include <soo/dev/vinput.h>
#include <soo/guest_api.h>

/* 1 is the code for CTRL-A */
#define SWITCH_CODE 1

#define N_SWITCH_FOCUS 8

/* Volatile to ensure the value is read from memory and no optimization occurs */
static int volatile avzcons_active = 0;
static int volatile vfb_active = 0;

/* Provided by vUART backend */
void me_cons_sendc(domid_t domid, uint8_t ch);

int avzcons_get_focus(void)
{
	return avzcons_active;
}

int avzcons_set_focus(int next_domain)
{
	avzcons_active = next_domain;
	return avzcons_active;
}

int vfb_get_focus(void)
{
	return vfb_active;
}

int vfb_set_focus(int next_domain)
{
	vfb_active = next_domain;
	vfb_set_active_domfb(vfb_active);
	vinput_set_current(vfb_active);
	return vfb_active;
}

/* Given the active domain id, returns the next domain id that will be activated. */
int get_next(int active)
{
	ME_desc_t me_desc;
	int next = (active + 1) % N_SWITCH_FOCUS;

	if (next == 0) {
		/* Agency domain */
		return next;
	}
	else if (next == 1) {
		/* Agency-RT domain - skip for now */
		return get_next(next);
	}
	else if (next < 7) {
		/* ME - 2 to 6 */
		get_ME_desc(next, &me_desc);
		return me_desc.size == 0 ? get_next(next) : next;
	}
	else {
		/* Agency AVZ Hypervisor - skip */
		return get_next(next);
	}
}


/*
 * avz_switch_console() - Allow the user to give input focus to various input sources (agency, MEs, avz)
 *
 * Warning !! We are in interrupt context top half when the function is called and a lock is pending
 * on the UART. Use of printk() is forbidden and we need to use lprintk() to avoid deadlock.
 *
 */
int avz_switch_console(char ch)
{
	static int switch_code_count = 0;
	static char *input_str[N_SWITCH_FOCUS] = { "Agency domain", "Agency-RT domain", "ME-1(2)", "ME-2(3)", "ME-3(4)", "ME-4(5)", "ME-5(6)", "Agency AVZ Hypervisor" };

	int active, next;

/* Debugging purpose - enabled forces to forward to an ME */
#if 0
	me_cons_sendc(1, ch);
#endif

	if ((SWITCH_CODE != 0) && (ch == SWITCH_CODE)) {
		/* We eat CTRL-<switch_char> in groups of 2 to switch console input. */
		if (++switch_code_count == 1) {

			active = get_next(avzcons_get_focus());
			next = get_next(active);

			avzcons_set_focus(active);
			vfb_set_focus(active);

			switch_code_count = 0;

			lprintk("*** Serial input -> %s (type 'CTRL-%c' twice to switch input to %s).\n", input_str[active], 'a', input_str[next]);
			return 1;
		}
	}
	else {
		switch_code_count = 0;

		switch (avzcons_get_focus()) {
		default:
		case 0: /* Input to the agency */
			return 0;
#if 0
		case 1: /* RT domain */
#endif
		case 2: /* Input to ME #2 */

		case 3: /* Input to ME #3 */
		case 4: /* Input to ME #4 */
		case 5: /* Input to ME #5 */
		case 6: /* Input to ME #6 */
#ifdef CONFIG_VUART_BACKEND
			me_cons_sendc(avzcons_get_focus(), ch);
#endif /* CONFIG_VUART_BACKEND */
			return 1;
		case 7: /* Input to avz */
			hypercall_trampoline(__HYPERVISOR_console_io, CONSOLEIO_process_char, 1, (long) &ch, 0);
			return 1;
		}
	}

	return 0;

}
