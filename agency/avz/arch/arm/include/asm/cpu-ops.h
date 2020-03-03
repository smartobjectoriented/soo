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
#ifndef __ASM_CPU_OPS_H__
#define __ASM_CPU_OPS_H__

#ifndef __ASSEMBLY__
#define DECLARE_CPU_OP(gop, nop)	\
	typeof (nop) gop		\
	__attribute__((weak, alias(#nop)))

void cpu_halt(int mode);
void cpu_idle(void);

/*
 * MMU Operations
 */

void cpu_switch_ttb(unsigned long);
unsigned long cpu_get_ttb(void);

#endif

#ifdef __ASSEMBLY__
#define DECLARE_CPU_OP(gop, nop)	 \
	.set gop, nop			;\
	.global gop			;
#endif

#endif

