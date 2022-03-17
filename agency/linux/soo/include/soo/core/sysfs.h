
/*
 * Copyright (C) 2020 Daniel Rossier <daniel.rossier@soo.tech>
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

#ifndef SOO_CORE_SYSFS_H
#define SOO_CORE_SYSFS_H

/* Attribute names */

typedef enum {
	/* SOO */
	agencyUID,
	name,

	/** SOOlink **/

	/**** Discovery ****/
	buffer_count, neighbours,

	/** Backend **/

	/**** vsensej ****/
	vsensej_js

} soo_sysfs_attr_t;

/* These callback types are used to make use of show/store sysfs callback as generic as possible. */
typedef void (*sysfs_handler_t)(char *str);

void soo_sysfs_init(void);

void soo_sysfs_register(soo_sysfs_attr_t attr, sysfs_handler_t show_handler, sysfs_handler_t store_handler);

#endif /* SOO_CORE_SYSFS_H */
