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

#ifndef __ASM_ARM_DIV64
#define __ASM_ARM_DIV64

#include <asm/system.h>

/*
 * The semantics of do_div() are:
 *
 * uint32_t do_div(uint64_t *n, uint32_t base)
 * {
 * 	uint32_t remainder = *n % base;
 * 	*n = *n / base;
 * 	return remainder;
 * }
 *
 * In other words, a 64-bit dividend with a 32-bit divisor producing
 * a 64-bit result and a 32-bit remainder.  To accomplish this optimally
 * we call a special __do_div64 helper with completely non standard
 * calling convention for arguments and results (beware).
 */

#ifdef __ARMEB__
#define __xh "r0"
#define __xl "r1"
#else
#define __xl "r0"
#define __xh "r1"
#endif

#define do_div(n,base)						\
({								\
	register unsigned int __base      asm("r4") = base;	\
	register unsigned long long __n   asm("r0") = n;	\
	register unsigned long long __res asm("r2");		\
	register unsigned int __rem       asm(__xh);		\
	asm(	__asmeq("%0", __xh)				\
		__asmeq("%1", "r2")				\
		__asmeq("%2", "r0")				\
		__asmeq("%3", "r4")				\
		"bl	__do_div64"				\
		: "=r" (__rem), "=r" (__res)			\
		: "r" (__n), "r" (__base)			\
		: "ip", "lr", "cc");				\
	n = __res;						\
	__rem;							\
})

#endif
