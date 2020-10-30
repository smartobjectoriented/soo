/*
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
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

#ifndef ECO_STABILITY_H
#define ECO_STABILITY_H

#include <soo/soo.h>

#define MAX_INERTIA	30

#define MAX_NAME_SIZE	16
#define MAX_DESC	6

#define MAX_MIGRATION_COUNT	3

typedef struct {
	bool		active;
	uint32_t	age;
	uint32_t	last_age;
	uint8_t		inertia;
	uint32_t	change_count;

	/* Generic fields */
	uint8_t		name[MAX_NAME_SIZE];
	uint8_t		agencyUID[SOO_AGENCY_UID_SIZE];
} soo_presence_data_t;


uint32_t get_my_id(soo_presence_data_t *presence);
void merge_info(spinlock_t *lock,
		soo_presence_data_t *local_presence, void *local_info_ptr,
		soo_presence_data_t *incoming_presence, void *recv_data_ptr,
		soo_presence_data_t *tmp_presence, void *tmp_info_ptr,
		size_t info_size);
void inc_age_reset_inertia(spinlock_t *lock, soo_presence_data_t *presence);
void watch_ages(spinlock_t *lock,
		soo_presence_data_t *local_presence, void *local_info_ptr,
		soo_presence_data_t *tmp_presence, void *tmp_info_ptr,
		size_t info_size);

#endif /* ECO_STABILITY_H */
