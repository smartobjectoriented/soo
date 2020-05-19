/*
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <soo/evtchn.h>
#include <soo/hypervisor.h>
#include <soo/avzcons.h>


/* 1 is the code for CTRL-A */
#define SWITCH_CODE 1

#define N_SWITCH_FOCUS 7

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
	static char *input_str[N_SWITCH_FOCUS] = { "Agency domain", "ME-1", "ME-2", "ME-3", "ME-4", "ME-5", "Agency AVZ Hypervisor" };
	int active = 0;
	int next = 1;

	if ((SWITCH_CODE != 0) && (ch == SWITCH_CODE)) {
		/* We eat CTRL-<switch_char> in groups of 2 to switch console input. */
		if (++switch_code_count == 1) {

			active = avzcons_set_focus((avzcons_get_focus() + 1) % N_SWITCH_FOCUS);
			next = (avzcons_get_focus() + 1) % N_SWITCH_FOCUS;
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
		case 1: /* Input to ME #1 */
		case 2: /* Input to ME #2 */
		case 3: /* Input to ME #3 */
		case 4: /* Input to ME #4 */
		case 5: /* Input to ME #5 */
#ifdef CONFIG_VUART_BACKEND
			me_cons_sendc(avzcons_get_focus(), ch);
#endif /* CONFIG_VUART_BACKEND */
			return 1;
		case 6: /* Input to avz */
			hypercall_trampoline(__HYPERVISOR_console_io, CONSOLEIO_process_char, 1, (long) &ch, 0);
			return 1;
		}
	}

	return 0;

}

