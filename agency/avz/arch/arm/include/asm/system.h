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

#ifndef __ASM_ARM_SYSTEM_H
#define __ASM_ARM_SYSTEM_H

#define CPU_ARCH_UNKNOWN	0
#define CPU_ARCH_ARMv3		1
#define CPU_ARCH_ARMv4		2
#define CPU_ARCH_ARMv4T		3
#define CPU_ARCH_ARMv5		4
#define CPU_ARCH_ARMv5T		5
#define CPU_ARCH_ARMv5TE	6
#define CPU_ARCH_ARMv5TEJ	7
#define CPU_ARCH_ARMv6		8
#define CPU_ARCH_ARMv7		9
#define CPU_ARCH_ARMv7M   10

#include <soo/uapi/arch-arm.h>

/*
 *  CR1 bits (CP#15 CR1)
 */
#define CR_M	(1 << 0)	/* MMU enable				*/
#define CR_A	(1 << 1)	/* Alignment abort enable		*/
#define CR_C	(1 << 2)	/* Dcache enable			*/
#define CR_W	(1 << 3)	/* Write buffer enable			*/
#define CR_P	(1 << 4)	/* 32-bit exception handler		*/
#define CR_D	(1 << 5)	/* 32-bit data address range		*/
#define CR_L	(1 << 6)	/* Implementation defined		*/
#define CR_B	(1 << 7)	/* Big endian				*/
#define CR_S	(1 << 8)	/* System MMU protection		*/
#define CR_R	(1 << 9)	/* ROM MMU protection			*/
#define CR_F	(1 << 10)	/* Implementation defined		*/
#define CR_Z	(1 << 11)	/* Implementation defined		*/
#define CR_I	(1 << 12)	/* Icache enable			*/
#define CR_V	(1 << 13)	/* Vectors relocated to 0xffff0000	*/
#define CR_RR	(1 << 14)	/* Round Robin cache replacement	*/
#define CR_L4	(1 << 15)	/* LDR pc can set T bit			*/
#define CR_DT	(1 << 16)
#define CR_IT	(1 << 18)
#define CR_ST	(1 << 19)
#define CR_FI	(1 << 21)	/* Fast interrupt (lower latency mode)	*/
#define CR_U	(1 << 22)	/* Unaligned access operation		*/
#define CR_XP	(1 << 23)	/* Extended page tables			*/
#define CR_VE	(1 << 24)	/* Vectored interrupts			*/
#define CR_EE   (1 << 25)       /* Exception (Big) Endian               */
#define CR_TRE  (1 << 28)       /* TEX remap enable                     */
#define CR_AFE  (1 << 29)       /* Access flag enable                   */
#define CR_TE   (1 << 30)       /* Thumb exception enable               */

#define CPUID_ID	0
#define CPUID_CACHETYPE	1
#define CPUID_TCM	2
#define CPUID_TLBTYPE	3

#define PSR_MODE_USR26  0x00000000
#define PSR_MODE_FIQ26  0x00000001
#define PSR_MODE_IRQ26  0x00000002
#define PSR_MODE_SVC26  0x00000003
#define PSR_MODE_USR    0x00000010
#define PSR_MODE_FIQ    0x00000011
#define PSR_MODE_IRQ    0x00000012
#define PSR_MODE_SVC    0x00000013
#define PSR_MODE_ABT    0x00000017
#define PSR_MODE_UND    0x0000001b
#define PSR_MODE_SYS    0x0000001f
#define PSR_MODE_MASK   0x0000001f
#define PSR_STATUS_T    0x00000020
#define PSR_STATUS_F    0x00000040
#define PSR_STATUS_I    0x00000080
#define PSR_STATUS_J    0x01000000
#define PSR_STATUS_Q    0x08000000
#define PSR_STATUS_V    0x10000000
#define PSR_STATUS_C    0x20000000
#define PSR_STATUS_Z    0x40000000
#define PSR_STATUS_N    0x80000000
#define PCMASK          0


/*
 * This is used to ensure the compiler did actually allocate the register we
 * asked it for some inline assembly sequences.  Apparently we can't trust
 * the compiler from one version to another so a bit of paranoia won't hurt.
 * This string is meant to be concatenated with the inline asm string and
 * will cause compilation to stop on mismatch.
 * (for details, see gcc PR 15089)
 */
#define __asmeq(x, y)  ".ifnc " x "," y " ; .err ; .endif\n\t"

#ifndef __ASSEMBLY__

#define sev()		asm volatile("sev" : : : "memory")
#define isb(option) __asm__ __volatile__ ("isb " #option : : : "memory")
#define dsb(option) __asm__ __volatile__ ("dsb " #option : : : "memory")
#define dmb(option) __asm__ __volatile__ ("dmb " #option : : : "memory")

#include <asm/processor.h>

#define read_barrier_depends() do { } while(0)
#define set_mb(var, value)  do { var = value; mb(); } while (0)
#define set_wmb(var, value) do { var = value; wmb(); } while (0)
#define nop() __asm__ __volatile__("mov\tr0,r0\t@ nop\n\t");

#define barrier() __asm__ __volatile__("": : :"memory")
#define cpu_relax()                     barrier()

#define vectors_high()  (cr_alignment & CR_V)

extern int cpu_architecture(void);

#define set_cr(x)					\
	__asm__ __volatile__(				\
	"mcr	p15, 0, %0, c1, c0, 0	@ set CR"	\
	: : "r" (x) : "cc")

#define get_cr()					\
	({						\
	unsigned int __val;				\
	__asm__ __volatile__(				\
	"mrc	p15, 0, %0, c1, c0, 0	@ get CR"	\
	: "=r" (__val) : : "cc");			\
	__val;						\
	})


#define local_irq_save(x)                                   \
          ({                                                      \
          __asm__ __volatile__(                                   \
          "mrs    %0, cpsr                @ local_irq_save\n"     \
          "cpsid  i"                                              \
          : "=r" (x) : : "memory", "cc");                         \
          })

#define local_irq_enable()  __asm__("cpsie i        @ __sti" : : : "memory", "cc")
#define local_irq_disable() __asm__("cpsid i        @ __cli" : : : "memory", "cc")
#define local_fiq_enable()  __asm__("cpsie f    @ __stf" : : : "memory", "cc")
#define local_fiq_disable() __asm__("cpsid f    @ __clf" : : : "memory", "cc")

/*
 * Save the current interrupt enable state.
 */
#define local_save_flags(x)					\
	({							\
	__asm__ __volatile__(					\
	"mrs	%0, cpsr		@ local_save_flags"	\
	: "=r" (x) : : "memory", "cc");				\
	})

/*
 * restore saved IRQ & FIQ state
 */
#define local_irq_restore(x)					\
	__asm__ __volatile__(					\
	"msr	cpsr_c, %0		@ local_irq_restore\n"	\
	:							\
	: "r" (x)						\
	: "memory", "cc")

#define local_irq_is_disabled()			\
({					\
	unsigned long flags;		\
	local_save_flags(flags);	\
	(int)(flags & PSR_I_BIT);	\
})

#define local_irq_is_enabled() \
({ \
       unsigned long flags; \
       local_save_flags(flags); \
       !(flags & PSR_I_BIT); \
})


/*
 * switch_to(prev, next) should switch from task `prev' to `next'
 * `prev' will never be the same as `next'.  schedule() itself
 * contains the memory barrier to tell GCC not to cache `current'.
 */
struct vcpu;
void __switch_to( struct vcpu *, struct vcpu_guest_context *, struct vcpu_guest_context *);

#define switch_to(prev,next,last)                                       \
do {                                                                    \
         __switch_to(prev,&prev->arch.guest_context, &next->arch.guest_context);   \
} while (0)

extern unsigned long cr_no_alignment;	/* defined in entry-armv.S */
extern unsigned long cr_alignment;	/* defined in entry-armv.S */

#define smp_mb()		dmb()

extern void printk(const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));

static void __bad_xchg(volatile void *ptr, int size) {
	printk("!! __bad_xchg called! Failure...\n");
	while (1);
}

static inline unsigned long __xchg(unsigned long x, volatile void *ptr, int size)
{
	unsigned long ret;
	unsigned int tmp;

	switch (size) {

	case 1:
		asm volatile("@	__xchg1\n"
		"1:	ldrexb	%0, [%3]\n"
		"	strexb	%1, %2, [%3]\n"
		"	teq	%1, #0\n"
		"	bne	1b"
			: "=&r" (ret), "=&r" (tmp)
			: "r" (x), "r" (ptr)
			: "memory", "cc");
		break;

	case 4:
		asm volatile("@	__xchg4\n"
		"1:	ldrex	%0, [%3]\n"
		"	strex	%1, %2, [%3]\n"
		"	teq	%1, #0\n"
		"	bne	1b"
			: "=&r" (ret), "=&r" (tmp)
			: "r" (x), "r" (ptr)
			: "memory", "cc");
		break;

	default:
		__bad_xchg(ptr, size), ret = 0;
		break;
	}

	return ret;
}

#define xchg(ptr,x) \
	((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))

#endif /* __ASSEMBLY__ */


#endif
