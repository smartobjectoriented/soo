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


#include <console.h>
#include <spinlock.h>

#include <asm/processor.h>
#include <asm/domain.h>
#include <asm/backtrace.h>

extern spinlock_t console_lock;

void panic(const char *fmt, ...)
{
	va_list args;
	unsigned long flags;
	static DEFINE_SPINLOCK(lock);
	static char buf[128];

	/* Protects buf[] and ensure multi-line message prints atomically. */
	spin_lock_init(&console_lock);

	spin_lock_irqsave(&lock, flags);

	va_start(args, fmt);
	(void)vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	printk("\n****************************************\n");
	printk("Panic on CPU %d:\n", smp_processor_id());
	printk("%s", buf);
	printk("****************************************\n\n");

	printk("Manual reset required ('noreboot' specified)\n");

	spin_unlock_irqrestore(&lock, flags);

	dump_all_execution_state();

	while (1);
}

void __bug(char *file, int line)
{
	printk("AVZ BUG at %s:%d\n", file, line);
	dump_execution_state();
	panic("AVZ BUG at %s:%d\n", file, line);
	for ( ; ; ) ;
}

void __warn(char *file, int line)
{
	printk("AVZ WARN at %s:%d\n", file, line);
	dump_execution_state();
}

void __fault_trap(uint32_t far, uint32_t fsr, uint32_t lr) {

	printk("### %s details are far: %x fsr: %x lr(r14)-8: %x ###\n", __func__, far, fsr, lr-8);

	while(1);
}



