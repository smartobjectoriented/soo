/*
 * This was automagically generated from arch/arm/tools/mach-types!
 * Do NOT edit
 */

#ifndef __ASM_ARM_MACH_TYPE_H
#define __ASM_ARM_MACH_TYPE_H

#include <linux/config.h>

#ifndef __ASSEMBLY__
/* The type of machine we're running on */
extern unsigned int __machine_arch_type;
#endif

/* see arch/arm/kernel/arch.c for a description of these */
#define MACH_TYPE_VEXPRESS             2272
#define MACH_TYPE_BCM2835              4828
#define MACH_TYPE_SUN50I               9878
#define MACH_TYPE_RPI4                 4828

#ifdef CONFIG_MACH_VEXPRESS
# ifdef machine_arch_type
#  undef machine_arch_type
#  define machine_arch_type	__machine_arch_type
# else
#  define machine_arch_type	MACH_TYPE_VEXPRESS
# endif
# define machine_is_vexpress()	(machine_arch_type == MACH_TYPE_VEXPRESS)
#else
# define machine_is_vexpress()	(0)
#endif

#ifdef CONFIG_MACH_BCM2835
# ifdef machine_arch_type
#  undef machine_arch_type
#  define machine_arch_type	__machine_arch_type
# else
#  define machine_arch_type	MACH_TYPE_BCM2835
# endif
# define machine_is_bcm2835()	(machine_arch_type == MACH_TYPE_BCM2835)
#else
# define machine_is_bcm2835()	(0)
#endif

#ifdef CONFIG_MACH_SUN50I
# ifdef machine_arch_type
#  undef machine_arch_type
#  define machine_arch_type	__machine_arch_type
# else
#  define machine_arch_type	MACH_TYPE_SUN50I
# endif
# define machine_is_sun50i()	(machine_arch_type == MACH_TYPE_SUN50I)
#else
# define machine_is_sun50i()	(0)
#endif

#ifdef CONFIG_MACH_RPI4
# ifdef machine_arch_type
#  undef machine_arch_type
#  define machine_arch_type	__machine_arch_type
# else
#  define machine_arch_type	MACH_TYPE_RPI4
# endif
# define machine_is_rpi4()	(machine_arch_type == MACH_TYPE_RPI4)
#else
# define machine_is_rpi4()	(0)
#endif

/*
 * These have not yet been registered
 */

#ifndef machine_arch_type
#define machine_arch_type	__machine_arch_type
#endif

#endif
