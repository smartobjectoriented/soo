/*
 *  linux/include/asm-arm/processor.h
 *
 *  Copyright (C) 1995-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARM_PROCESSOR_H
#define __ASM_ARM_PROCESSOR_H

#include <asm/cpregs.h>
#include <linux/stringify.h>
/*
 * Default implementation of macro that returns current
 * instruction pointer ("program counter").
 */
#define current_text_addr() ({ __label__ _l; _l: &&_l;})

/* Layout as used in assembly, with src/dest registers mixed in */
#define __CP32(r, coproc, opc1, crn, crm, opc2) coproc, opc1, r, crn, crm, opc2
#define __CP64(r1, r2, coproc, opc, crm) coproc, opc, r1, r2, crm
#define CP32(r, name...) __CP32(r, name)
#define CP64(r, name...) __CP64(r, name)

/* Stringified for inline assembly */
#define LOAD_CP32(r, name...)  "mrc " __stringify(CP32(%r, name)) ";"
#define STORE_CP32(r, name...) "mcr " __stringify(CP32(%r, name)) ";"
#define LOAD_CP64(r, name...)  "mrrc " __stringify(CP64(%r, %H##r, name)) ";"
#define STORE_CP64(r, name...) "mcrr " __stringify(CP64(%r, %H##r, name)) ";"

/* Issue a CP operation which takes no argument,
 * uses r0 as a placeholder register. */
#define CMD_CP32(name...)      "mcr " __stringify(CP32(r0, name)) ";"

#ifndef __ASSEMBLY__

/* C wrappers */
#define READ_CP32(name...) ({                                   \
    register uint32_t _r;                                       \
    asm volatile(LOAD_CP32(0, name) : "=r" (_r));               \
    _r; })

#define WRITE_CP32(v, name...) do {                             \
    register uint32_t _r = (v);                                 \
    asm volatile(STORE_CP32(0, name) : : "r" (_r));             \
} while (0)

#define READ_CP64(name...) ({                                   \
    register uint64_t _r;                                       \
    asm volatile(LOAD_CP64(0, name) : "=r" (_r));               \
    _r; })

#define WRITE_CP64(v, name...) do {                             \
    register uint64_t _r = (v);                                 \
    asm volatile(STORE_CP64(0, name) : : "r" (_r));             \
} while (0)

/*
 * C wrappers for accessing system registers.
 *
 * Registers come in 3 types:
 * - those which are always 32-bit regardless of AArch32 vs AArch64
 *   (use {READ,WRITE}_SYSREG32).
 * - those which are always 64-bit regardless of AArch32 vs AArch64
 *   (use {READ,WRITE}_SYSREG64).
 * - those which vary between AArch32 and AArch64 (use {READ,WRITE}_SYSREG).
 */
#define READ_SYSREG32(R...)     READ_CP32(R)
#define WRITE_SYSREG32(V, R...) WRITE_CP32(V, R)

#define READ_SYSREG64(R...)     READ_CP64(R)
#define WRITE_SYSREG64(V, R...) WRITE_CP64(V, R)

#define READ_SYSREG(R...)       READ_SYSREG32(R)
#define WRITE_SYSREG(V, R...)   WRITE_SYSREG32(V, R)

/* Erratum 766422: only Cortex A15 r0p4 is affected */
#define cpu_has_erratum_766422()                             \
    (unlikely(current_cpu_data.midr.bits == 0x410fc0f4))

#endif /* __ASSEMBLY__ */

#include <asm/ptrace.h>
#include <asm/procinfo.h>
#include <asm/types.h>

#define cpu_relax()			barrier()

#define ARCH_HAS_PREFETCH
#define prefetch(ptr)				\
	({					\
		__asm__ __volatile__(		\
		"pld\t%0"			\
		:				\
		: "o" (*(char *)(ptr))		\
		: "cc");			\
	})

#define ARCH_HAS_PREFETCHW
#define prefetchw(ptr)	prefetch(ptr)

#define ARCH_HAS_SPINLOCK_PREFETCH
#define spin_lock_prefetch(x) do { } while (0)

#define __HVC(imm16) __inst_arm_thumb32(				\
	0xE1400070 | (((imm16) & 0xFFF0) << 4) | ((imm16) & 0x000F),	\
	0xF7E08000 | (((imm16) & 0xF000) << 4) | ((imm16) & 0x0FFF)	\
)

#define __ERET	__inst_arm_thumb32(					\
	0xE160006E,							\
	0xF3DE8F00							\
)

#define __MSR_ELR_HYP(regnum)	__inst_arm_thumb32(			\
	0xE12EF300 | regnum,						\
	0xF3808E30 | (regnum << 16)					\
)


#define __SMC(imm4) __inst_arm_thumb32(					\
	0xE1600070 | (((imm4) & 0xF) << 0),				\
	0xF7F08000 | (((imm4) & 0xF) << 16)				\
)

#ifndef __ASSEMBLY__

static inline int smp_processor_id(void) {
	int cpu;

	/* Read Multiprocessor ID register */
	asm volatile ("mrc p15, 0, %0, c0, c0, 5": "=r" (cpu));

	/* Mask out all but CPU ID bits */
	return (cpu & 0x3);
}

#endif /* !__ASSEMBLY__ */

#endif /* __ASM_ARM_PROCESSOR_H */
