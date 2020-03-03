/*
 * Copyright (C) 2014-2016 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <avz/init.h>
#include <avz/lib.h>
#include <avz/cpumask.h>
#include <avz/mm.h>
#include <avz/errno.h>

#include <asm/config.h>
#include <asm/percpu.h>

extern char __per_cpu_start[], __per_cpu_data_end[], __per_cpu_end[];
unsigned long __per_cpu_offset[NR_CPUS];

#define INVALID_PERCPU_AREA (-(long)__per_cpu_start)
#define PERCPU_ORDER (get_order_from_bytes(__per_cpu_data_end-__per_cpu_start))

void __init percpu_init_areas(void)
{
    unsigned int cpu;

    for (cpu = 1; cpu < NR_CPUS; cpu++)
        __per_cpu_offset[cpu] = INVALID_PERCPU_AREA;
}

int init_percpu_area(unsigned int cpu)
{
    char *p;
    if (__per_cpu_offset[cpu] != INVALID_PERCPU_AREA)
        return -EBUSY;

    if ((p = alloc_heap_pages(PERCPU_ORDER, 0)) == NULL)
        return -ENOMEM;

    memset(p, 0, __per_cpu_data_end - __per_cpu_start);
    __per_cpu_offset[cpu] = p - __per_cpu_start;

    return 0;
}




