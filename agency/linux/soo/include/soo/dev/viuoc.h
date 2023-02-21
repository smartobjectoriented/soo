/*
 * Copyright (C) 2023 A.Gabriel Catel Torres <arzur.cateltorres@heig-vd.ch>
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

#ifndef VIUOC_H
#define VIUOC_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <soo/vdevback.h>
#include <soo/uapi/iuoc.h>

#define VIUOC_NAME			"viuoc"
#define VIUOC_PREFIX		"[" VIUOC_NAME " backend] "
#define NB_DATA_MAX 			15

typedef struct {
	iuoc_data_t me_data;
} viuoc_request_t;

typedef struct  {
	iuoc_data_t me_data;
} viuoc_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(viuoc, viuoc_request_t, viuoc_response_t);

/*
 * General structure for this virtual device (backend side)
 */
typedef struct {

	/* Must be the first field */
	vdevback_t vdevback;

	viuoc_back_ring_t ring;
	unsigned int irq;

} viuoc_t;

typedef struct {
    struct list_head list;
    int32_t id;
} domid_priv_t;

/**
 * @brief Allows to send a data from the FE to the BE
 * 
 * @param iuoc_data: structure containing the ME data information
 */
void viuoc_send_data_to_fe(iuoc_data_t iuoc_data);


#endif /* VIUOC_H */
