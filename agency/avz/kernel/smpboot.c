/*
 * Copyright (C) 2014-2018 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <avz/config.h>
#include <avz/init.h>
#include <avz/types.h>
#include <avz/cpumask.h>
#include <avz/percpu.h>

#include <asm/cache.h>
#include <asm/smp.h>


/* representing HT and core siblings of each logical CPU */
cpumask_t cpu_core_map[NR_CPUS] __read_mostly;

/* bitmap of online cpus */
cpumask_t cpu_online_map __read_mostly;

cpumask_t cpu_possible_map;


/* ID of the PCPU we're running on */
DEFINE_PER_CPU(unsigned int, cpu_id);

/* representing HT and core siblings of each logical CPU */
DEFINE_PER_CPU_READ_MOSTLY(cpumask_t, cpu_core_map);

void __init smp_clear_cpu_maps (void)
{
    cpus_clear(cpu_possible_map);
    cpus_clear(cpu_online_map);
    cpu_set(0, cpu_online_map);
    cpu_set(0, cpu_possible_map);
}

int __init smp_get_max_cpus (void)
{
    int i, max_cpus = 0;

    for (i = 0; i < NR_CPUS; i++)
        if (cpu_possible(i))
            max_cpus++;

    return max_cpus;
}

void __init smp_prepare_cpus (unsigned int max_cpus)
{
	cpumask_copy(&cpu_present_map, &cpu_possible_map);

	platform_smp_prepare_cpus(max_cpus);
}

