/*
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/skbuff.h>

#include <uapi/linux/if_ether.h>

#include <soo/soolink/soolink.h>

/*
 * Delay in ms to give a chance to Linux netdev to get initialized.
 */
#define NET_DEV_DETECT_DELAY	(3 * 1000)

typedef struct {
	sl_desc_t *sl_desc;
	void * volatile data;
	size_t size;
	struct list_head list;
} plugin_send_args_t;

typedef struct {
	req_type_t req_type;
	void * volatile data;
	size_t size;
	uint8_t	mac[ETH_ALEN];
} plugin_recv_args_t;

typedef struct {
	agencyUID_t agencyUID;
	uint8_t mac[ETH_ALEN];
	struct list_head list;   /* Take part of the list of neighbours */
} plugin_remote_soo_desc_t;

