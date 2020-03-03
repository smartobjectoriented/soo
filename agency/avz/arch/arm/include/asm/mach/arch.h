/*
 *  linux/include/asm-arm/mach/arch.h
 *
 *  Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_H__
#ifndef __ASSEMBLY__

#include <soo/uapi/physdev.h>

#include <avz/compiler.h>

struct tag;
struct meminfo;
struct sys_timer;
struct smp_operations;

#define smp_ops(ops) (&(ops))
#define smp_init_ops(ops) (&(ops))

struct machine_desc {
	/*
	 * Note! The first four elements are used
	 * by assembler code in head-armv.S
	 */
	unsigned int	nr;		/* architecture number	*/

	const char	*name;		/* architecture name	*/
	unsigned long	boot_params;	/* tagged list		*/

	struct smp_operations *smp;     /* SMP operations       */

	void (*map_io)(void);/* IO mapping function	*/
	void (*init_irq)(void);
	struct sys_timer *timer;		/* system tick timer	*/
	void (*init_machine)(void);
};

extern struct machine_desc *mdesc;
extern struct sys_timer *system_timer;

/*
 * Set of macros to define architecture features.  This is built into
 * a table by the linker.
 */
#define MACHINE_START(_type,_name)			\
static const struct machine_desc __mach_desc_##_type	\
 __attribute_used__					\
 __attribute__((__section__(".arch.info.init"))) = {	\
	.nr		= MACH_TYPE_##_type,		\
	.name		= _name,

#define MACHINE_END				\
};

#endif
#endif /* __ARCH_H__ */
