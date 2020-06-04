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

#include <avz/stdarg.h>
#include <avz/config.h>
#include <avz/init.h>
#include <avz/lib.h>
#include <avz/errno.h>
#include <avz/event.h>
#include <avz/spinlock.h>
#include <avz/console.h>
#include <avz/serial.h>
#include <avz/softirq.h>
#include <avz/keyhandler.h>
#include <avz/mm.h>
#include <avz/delay.h>
#include <avz/types.h>

#include <soo/uapi/console.h>

#include <asm/domain.h>
#include <asm/current.h>
#include <asm/debugger.h>
#include <asm/io.h>
#include <asm/div64.h>

#include <mach/uart.h>

#define AVZ_BANNER		"*********** SOO - Agency Virtualizer SOO.tech Technology - HEIG-VD, REDS Institute ***********\n\n\n"

DEFINE_SPINLOCK(console_lock);

/* To manage the final virtual address of the UART */
extern uint32_t __uart_vaddr;

extern void printch(char c);

void serial_puts(const char *s)
{
	char c;

	while ((c = *s++) != '\0')
		printch(c);
}

void console_init_post(void) {
	__uart_vaddr = (uint32_t) ioremap(CONFIG_DEBUG_UART_PHYS, PAGE_SIZE);
}

static void sercon_puts(const char *s)
{
	serial_puts(s);
}

/* (DRE) Perform hypercall to process char addressed to the keyhandler mechanism */
void process_char(char ch) {

	struct cpu_user_regs regs;

	register unsigned int r0 __asm__("r0");
	register unsigned int r1 __asm__("r1");
	register unsigned int r2 __asm__("r2");
	register unsigned int r3 __asm__("r3");
	register unsigned int r4 __asm__("r4");
	register unsigned int r5 __asm__("r5");
	register unsigned int r6 __asm__("r6");
	register unsigned int r7 __asm__("r7");
	register unsigned int r8 __asm__("r8");
	register unsigned int r9 __asm__("r9");
	register unsigned int r10 __asm__("r10");
	register unsigned int r11 __asm__("r11");
	register unsigned int r12 __asm__("r12");
	register unsigned int r13 __asm__("r13");
	register unsigned int r14 __asm__("r14");
	register unsigned int r15;

	asm("mov %0, pc":"=r"(r15));

	regs.r0 = r0;
	regs.r1 = r1;
	regs.r2 = r2;
	regs.r3 = r3;
	regs.r4 = r4;
	regs.r5 = r5;
	regs.r6 = r6;
	regs.r7 = r7;
	regs.r8 = r8;
	regs.r9 = r9;
	regs.r10 = r10;
	regs.r11 = r11;
	regs.r12 = r12;
	regs.r13 = r13;
	regs.r14 = r14;
	regs.r15 = r15;

	handle_keypress(ch, &regs);
}

long do_console_io(int cmd, int count, char *buffer)
{
	char kbuf;
	char __buffer[CONSOLEIO_BUFFER_SIZE];
	int ret = 0;

	switch (cmd) {

	case CONSOLEIO_process_char:
		memcpy(&kbuf, buffer, sizeof(char));

		process_char(kbuf);
		ret = 1;
		break;

	case CONSOLEIO_write_string:

		ret = strlen(buffer);

		memcpy(__buffer, buffer, ret);

		printk("%s", buffer);
		break;

	default:
		BUG();
		break;
	}

	return ret;
}


/*
 * *****************************************************
 * *************** GENERIC CONSOLE I/O *****************
 * *****************************************************
 */

static void __putstr(const char *str)
{
	sercon_puts(str);
}

/* The console_lock must be hold. */
static void __printk(char *buf) {
	char *p, *q;

	p = buf;

	while ((q = strchr(p, '\n')) != NULL)
	{
		*q = '\0';
		__putstr(p);
		__putstr("\n");
		p = q + 1;
	}

	if (*p != '\0')
		__putstr(p);
}

void printk(const char *fmt, ...)
{
	static char   buf[1024];
	va_list       args;

	spin_lock(&console_lock);

	va_start(args, fmt);
	(void)vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	__printk(buf);

	spin_unlock(&console_lock);
}

/* Just to keep compatibility with uapi/soo in Linux */
void lprintk(char *fmt, ...) {
	static char   buf[1024];
	va_list       args;

	spin_lock(&console_lock);

	va_start(args, fmt);
	(void)vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	__printk(buf);

	spin_unlock(&console_lock);
}


void printk_buffer(void *buffer, int n) {
	int i;

	for (i = 0 ; i < n ; i++)
		printk("%02x ", ((char *) buffer)[i]);
	printk("\n");
}

void printk_buffer_separator(void *buffer, int n, char separator) {
	int i;

	for (i = 0 ; i < n ; i++)
		printk("%02x%c", ((char *) buffer)[i], separator);
	printk("\n");
}

void __init console_init(void)
{
	__putstr(AVZ_BANNER);

	printk(" SOO Agency Virtualizer -- v2020.2.0\n");

	printk(" Copyright (c) 2014-2020 HEIG-VD - REDS Institute, Switzerland / Smart Object Oriented technology\n");
	printk("\n\n\n");


}



