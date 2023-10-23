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
#include <linux/serial_core.h>
#include <linux/of.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/irq_regs.h>

#include <soo/evtchn.h>
#include <soo/hypervisor.h>
#include <soo/avzcons.h>

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
	return vfb_active;
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
	struct device_node *np;
	int active = 0;
	u32 focus_max = N_SWITCH_FOCUS - 1;

#if 0 /* Interactions with RT domain? */
	int next = 1;
#endif
	int next = 2;

	np = of_find_node_by_name(NULL, "console");
	if (np)
		of_property_read_u32((const struct device_node *) np, "focus-dom-max", &focus_max);

/* Debugging purpose - enabled forces to forward to an ME */
#if 0
	me_cons_sendc(1, ch);
#endif

	if ((SWITCH_CODE != 0) && (ch == SWITCH_CODE)) {

		/* We eat CTRL-<switch_char> in groups of 2 to switch console input. */
		if (++switch_code_count == 1) {

			active = (avzcons_get_focus() + 1) % (focus_max + 1);
			active = ((active == 1) ? active+1 : active);

			next = (active + 1) % (focus_max + 1);
			next = ((next == 1) ? next+1 : next);

			avzcons_set_focus(active);

			switch_code_count = 0;

			lprintk("*** Serial input -> %s (type 'CTRL-%c' twice to switch input to %s).\n", input_str[active], 'a', input_str[next]);

			return 1;
		}

	} else {
		switch_code_count = 0;

		switch (avzcons_get_focus()) {
		default:
		case 0: /* Input to the agency */
			return 0;

		case 1: /* RT domain */
		case 2: /* Input to ME #1 */
		case 3: /* Input to ME #2 */
		case 4: /* Input to ME #3 */
		case 5: /* Input to ME #4 */
		case 6: /* Input to ME #5 */
#ifdef CONFIG_VUART_BACKEND
			me_cons_sendc(avzcons_get_focus(), ch);
#endif /* CONFIG_VUART_BACKEND */
			return 1;

		case 7: /* Input to avz */
			avz_hypercall(__HYPERVISOR_console_io, CONSOLEIO_process_char, 1, (long) &ch, 0);
			return 1;
		}
	}

	return 0;

}

