/*
 * Copyright (C) 2008 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */
#ifndef _COBALT_ARM_ASM_UAPI_ARITH_H
#define _COBALT_ARM_ASM_UAPI_ARITH_H

#include <asm/xenomai/uapi/features.h>

#define xnarch_nodiv_llimd(op, frac, integ) \
	mach_arm_nodiv_llimd((op), (frac), (integ))

#include <cobalt/uapi/asm-generic/arith.h>

#define mach_arm_nodiv_ullimd_str			\
	"umull %[tl], %[rl], %[opl], %[fracl]\n\t"	\
	"umull %[rm], %[rh], %[oph], %[frach]\n\t"	\
	"adds %[rl], %[rl], %[tl], lsr #31\n\t"		\
	"adcs %[rm], %[rm], #0\n\t"			\
	"adc %[rh], %[rh], #0\n\t"			\
	"umull %[tl], %[th], %[oph], %[fracl]\n\t"	\
	"adds %[rl], %[rl], %[tl]\n\t"			\
	"adcs %[rm], %[rm], %[th]\n\t"			\
	"adc %[rh], %[rh], #0\n\t"			\
	"umull %[tl], %[th], %[opl], %[frach]\n\t"	\
	"adds %[rl], %[rl], %[tl]\n\t"			\
	"adcs %[rm], %[rm], %[th]\n\t"			\
	"adc %[rh], %[rh], #0\n\t"			\
	"umlal %[rm], %[rh], %[opl], %[integ]\n\t"	\
	"mla %[rh], %[oph], %[integ], %[rh]\n\t"

static inline __attribute__((__const__)) unsigned long
mach_arm_nodiv_ullimd(const unsigned long op,
		       const unsigned long frac,
		       const unsigned rhs_integ)
{
	return op / frac ;
}

static inline __attribute__((__const__)) long
mach_arm_nodiv_llimd(const long op,
		       const unsigned long frac,
		       const unsigned rhs_integ)
{
	return op / frac;
}

#endif /* _COBALT_ARM_ASM_UAPI_ARITH_H */
