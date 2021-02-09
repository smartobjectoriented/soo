/*
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


#include <soo/gnttab.h>
#include <soo/grant_table.h>
#include <soo/evtchn.h>
#include <soo/uapi/console.h>
#include <soo/vbus.h>

#include "vnetbuff.h"

/**
 * vbuff_map_buffer_valloc
 * @dev: vbus device
 * @gnt_ref: grant reference
 * @vaddr: pointer to address to be filled out by mapping
 *
 * Based on Rusty Russell's skeleton driver's map_page.
 * Map a page of memory into this domain from another domain's grant table.
 * vbus_map_ring_valloc allocates a page of virtual address space, maps the
 * page to that address, and sets *vaddr to that address.
 * Returns 0 on success, and GNTST_* (see include/interface/grant_table.h)
 * or -ENOMEM on error. If an error is returned, device will switch to
 * VbusStateClosing and the error message will be saved in VBstore.
 */
int vbuff_map_buffer_valloc(struct vbus_device *dev, int gnt_ref, void **vaddr)
{
	struct gnttab_map_grant_ref op = {
		.flags = GNTMAP_host_map,
		.ref   = gnt_ref,
		.dom   = dev->otherend_id,
	};
	struct vm_struct *area;

	DBG("%u\n", gnt_ref);

	*vaddr = NULL;

	area = alloc_vm_area(PAGE_SIZE * PAGE_COUNT, NULL);
	if (!area)
		BUG();

	gnttab_set_map_op(&op, area->addr, GNTMAP_host_map | GNTMAP_readonly, gnt_ref, dev->otherend_id, 0, PAGE_SIZE * PAGE_COUNT);

	if (gnttab_map(&op) || op.status != GNTST_okay) {
		free_vm_area(area);
		DBG(VFB_PREFIX "mapping in shared page %d from domain %d failed for device %s\n", fb_ref, watch->dev->otherend_id, watch->dev->nodename);
		BUG();
	}

	*vaddr = area->addr;

	return 0;
}

void vbuff_init(struct vbuff_buff* buff, grant_ref_t grant, struct vbus_device *vdev){
	vbuff_map_buffer_valloc(vdev, (int)grant, (void**)&buff->data);
	buff->size = PAGE_COUNT * PAGE_SIZE;
	buff->grant = grant;
	buff->prod = 0;
	mutex_init(&buff->mutex);
}

void vbuff_free(struct vbuff_buff* buff, struct vbus_device *vdev){
	vbus_unmap_ring_vfree(vdev, buff->data);
	memset(buff, 0, sizeof(struct vbuff_buff));
}

/**
 * Find a buffer in which a suitable spot is available
 *
 * data parameter:
 *  If data value is NULL, data is set to a pointer to the best chunk.
 *  If data value is a pointer the value is copied inside the buffer
 */
int vbuff_put(struct vbuff_buff* buff, struct vbuff_data *buff_data, uint8_t** data, size_t size){
	mutex_lock(&buff->mutex);

	/* not enough space in the circular buffer */
	if(buff->size < size){
		mutex_unlock(&buff->mutex);
		return -1;
	}


	/* if putting data in the buffer offerflow, set the productor back at the begining */
	if(buff->prod + size >= buff->size)
		buff->prod = 0;

	buff_data->offset = buff->prod;
	buff_data->size = size;
	buff_data->timestamp = 0 /* ktime_divns(ktime_get_ns(), 1000000ul)*/; /* time in ms */

	if(*data == NULL)
		*data = buff->data + buff_data->offset;
	else
		memcpy(buff->data + buff_data->offset, *data, size);

	buff->prod += size;

	mutex_unlock(&buff->mutex);

	return 0;
}



uint8_t* vbuff_get(struct vbuff_buff* buff, struct vbuff_data *buff_data){
	DBG("[Get Buff] offset: %d length: %d", buff_data->offset, buff_data->size);
	return buff->data + buff_data->offset;
}

uint8_t* vbuff_print(struct vbuff_buff* buff, struct vbuff_data *buff_data){
	uint8_t *data = vbuff_get(buff, buff_data);
	int i = 0;

	printk("[Print Buff] offset: %d length: %d\n", buff_data->offset, buff_data->size);

	while(i < buff_data->size){
		printk(KERN_CONT "%02x ", data[i]);
		i++;
	}

}
