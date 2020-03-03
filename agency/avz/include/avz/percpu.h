#ifndef __PERCPU_H__
#define __PERCPU_H__

#include <avz/config.h>
#include <asm/percpu.h>

/*
 * Separate out the type, so (int[3], foo) works.
 *
 * The _##name concatenation is being used here to prevent 'name' from getting
 * macro expanded, while still allowing a per-architecture symbol name prefix.
 */
#define DEFINE_PER_CPU(type, name) __DEFINE_PER_CPU(type, _##name, )
#define DEFINE_PER_CPU_READ_MOSTLY(type, name) \
	__DEFINE_PER_CPU(type, _##name, .read_mostly)

#define this_cpu(var)    __get_cpu_var(var)

/* Linux compatibility. */
#define get_cpu_var(var) this_cpu(var)
#define put_cpu_var(var)

#endif /* __PERCPU_H__ */
