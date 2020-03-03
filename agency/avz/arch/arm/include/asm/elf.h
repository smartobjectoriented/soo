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

#ifndef __ASMARM_ELF_H
#define __ASMARM_ELF_H

#ifndef __ASSEMBLY__
/*
 * ELF register definitions..
 */
#include <asm/ptrace.h>


typedef unsigned long elf_greg_t;
typedef unsigned long elf_freg_t[3];

#define ELF_NGREG (sizeof (cpu_user_regs_t) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef struct user_fp elf_fpregset_t;
#endif

#define EM_ARM	40
#define EF_ARM_APCS26 0x08
#define EF_ARM_SOFT_FLOAT 0x200
#define EF_ARM_EABI_MASK 0xFF000000

#define R_ARM_NONE	0
#define R_ARM_PC24	1
#define R_ARM_ABS32	2
#define R_ARM_CALL	28
#define R_ARM_JUMP24	29


/*
 * HWCAP flags - for elf_hwcap (in kernel) and AT_HWCAP
 */
#define HWCAP_SWP       (1 << 0)
#define HWCAP_HALF      (1 << 1)
#define HWCAP_THUMB     (1 << 2)
#define HWCAP_26BIT     (1 << 3)        /* Play it safe */
#define HWCAP_FAST_MULT (1 << 4)
#define HWCAP_FPA       (1 << 5)
#define HWCAP_VFP       (1 << 6)
#define HWCAP_EDSP      (1 << 7)
#define HWCAP_JAVA      (1 << 8)
#define HWCAP_IWMMXT    (1 << 9)
#define HWCAP_CRUNCH    (1 << 10)
#define HWCAP_THUMBEE   (1 << 11)
#define HWCAP_NEON      (1 << 12)
#define HWCAP_VFPv3     (1 << 13)
#define HWCAP_VFPv3D16  (1 << 14)
#define HWCAP_TLS       (1 << 15)
#define HWCAP_VFPv4     (1 << 16)
#define HWCAP_IDIVA     (1 << 17)
#define HWCAP_IDIVT     (1 << 18)
#define HWCAP_IDIV      (HWCAP_IDIVA | HWCAP_IDIVT)

#ifdef __KERNEL__
#ifndef __ASSEMBLY__
/*
 * This yields a mask that user programs can use to figure out what
 * instruction set this cpu supports.
 */
#define ELF_HWCAP	(elf_hwcap)
extern unsigned int elf_hwcap;

/*
 * This yields a string that ld.so will use to load implementation
 * specific libraries for optimization.  This is more specific in
 * intent than poking at uname or /proc/cpuinfo.
 *
 * For now we just provide a fairly general string that describes the
 * processor family.  This could be made more specific later if someone
 * implemented optimisations that require it.  26-bit CPUs give you
 * "v1l" for ARM2 (no SWP) and "v2l" for anything else (ARM1 isn't
 * supported).  32-bit CPUs give you "v3[lb]" for anything based on an
 * ARM6 or ARM7 core and "armv4[lb]" for anything based on a StrongARM-1
 * core.
 */
#define ELF_PLATFORM_SIZE 8
#define ELF_PLATFORM	(elf_platform)

extern char elf_platform[];
#endif

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) ((x)->e_machine == EM_ARM && ELF_PROC_OK(x))

/*
 * 32-bit code is always OK.  Some cpus can do 26-bit, some can't.
 */
#define ELF_PROC_OK(x)	(ELF_THUMB_OK(x) && ELF_26BIT_OK(x))

#define ELF_THUMB_OK(x) \
	((elf_hwcap & HWCAP_THUMB && ((x)->e_entry & 1) == 1) || \
	 ((x)->e_entry & 3) == 0)

#define ELF_26BIT_OK(x) \
	((elf_hwcap & HWCAP_26BIT && (x)->e_flags & EF_ARM_APCS26) || \
	  ((x)->e_flags & EF_ARM_APCS26) == 0)

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE	4096

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#define ELF_ET_DYN_BASE	(2 * TASK_SIZE / 3)

/* When the program starts, a1 contains a pointer to a function to be
   registered with atexit, as per the SVR4 ABI.  A value of 0 means we
   have no such handler.  */
#define ELF_PLAT_INIT(_r, load_addr)	(_r)->ARM_r0 = 0

/*
 * Since the FPA coprocessor uses CP1 and CP2, and iWMMXt uses CP0
 * and CP1, we only enable access to the iWMMXt coprocessor if the
 * binary is EABI or softfloat (and thus, guaranteed not to use
 * FPA instructions.)
 */
#define SET_PERSONALITY(ex, ibcs2)					\
	do {								\
		if ((ex).e_flags & EF_ARM_APCS26) {			\
			set_personality(PER_LINUX);			\
		} else {						\
			set_personality(PER_LINUX_32BIT);		\
			if (elf_hwcap & HWCAP_IWMMXT && (ex).e_flags & (EF_ARM_EABI_MASK | EF_ARM_SOFT_FLOAT)) \
				set_thread_flag(TIF_USING_IWMMXT);	\
			else						\
				clear_thread_flag(TIF_USING_IWMMXT);	\
		}							\
	} while (0)

#endif

#ifndef __ASSEMBLY__
typedef struct {
    unsigned long dummy;
} ELF_Gregset;


#endif

#endif
