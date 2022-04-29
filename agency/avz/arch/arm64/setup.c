/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

/*
 * Low-level ARM-specific setup
 */

#include <types.h>
#include <config.h>
#include <string.h>

/* To keep the original CPU ID so that we can avoid
 * undesired activities running on another CPU.
 */
uint32_t origin_cpu;

/* Low-level initialization before the main boostrap process.
 */
void setup_arch(void) {

	memset(&pseudo_usr_mode, 0, NR_CPUS * sizeof(unsigned int));

}
