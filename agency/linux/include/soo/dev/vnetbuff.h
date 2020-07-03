/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2020 Julien Quartier <julien.quartier@bluewin.ch>
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

#ifndef VNETBUFF_H
#define VNETBUFF_H

#include <soo/hypervisor.h>

#include <soo/uapi/avz.h>

#include <soo/grant_table.h>
#include <soo/vbstore.h>
#include <linux/types.h>



#define PAGE_COUNT 10
#define CHUNK_SIZE 128
#define CHUNK_COUNT PAGE_SIZE / CHUNK_SIZE

#define READ_ONLY 1

struct vbuff_buff {
	char *front_data;
	char *data;
	uint8_t spots[CHUNK_COUNT];
	grant_ref_t grant;
};

struct vbuff_data{
	int index;
	unsigned int offset;
	unsigned int size;
};

int vbuff_put(struct vbuff_data *buff_data, void* data, size_t size);
void* vbuff_get(struct vbuff_buff* buffs, struct vbuff_data *buff_data);
void vbuff_remove(struct vbuff_buff* buffs, struct vbuff_data *buff_data);


#endif //VNETBUFF_H
