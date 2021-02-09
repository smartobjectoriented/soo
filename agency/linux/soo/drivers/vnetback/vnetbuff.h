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
#include <linux/mutex.h>




#define PAGE_COUNT 10
#define CHUNK_SIZE 128
#define CHUNK_COUNT PAGE_SIZE / CHUNK_SIZE

#define READ_ONLY 1



struct vbuff_buff {
	uint8_t *data;
	size_t size;
	grant_ref_t grant;
	struct mutex mutex;
	size_t prod;
};

struct vbuff_data{
	uint32_t offset;
	uint32_t size;
	uint64_t timestamp;
};

void vbuff_init(struct vbuff_buff* buff, grant_ref_t grant, struct vbus_device *vdev);
void vbuff_free(struct vbuff_buff* buff, struct vbus_device *vdev);

int vbuff_put(struct vbuff_buff* buff, struct vbuff_data *buff_data, uint8_t** data, size_t size);
uint8_t* vbuff_get(struct vbuff_buff* buffs, struct vbuff_data *buff_data);
uint8_t* vbuff_print(struct vbuff_buff* buff, struct vbuff_data *buff_data);

#endif //VNETBUFF_H
