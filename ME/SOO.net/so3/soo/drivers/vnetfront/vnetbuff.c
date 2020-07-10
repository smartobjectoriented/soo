//
// Created by julien on 6/30/20.
//



#include <heap.h>
#include <mutex.h>
#include <delay.h>
#include <memory.h>
#include <asm/mmu.h>
#include <soo/gnttab.h>
#include <soo/grant_table.h>

#include <soo/dev/vnetbuff.h>

void vbuff_init(struct vbuff_buff* buff){
        buff->data_phys = (uint8_t*)get_contig_free_pages(PAGE_COUNT);
        buff->data = io_map(buff->data_phys, PAGE_COUNT * PAGE_SIZE);
        buff->size = PAGE_COUNT * PAGE_SIZE;
        buff->grant = GRANT_INVALID_REF;
        mutex_init(&buff->mutex);
}

void vbuff_free(struct vbuff_buff* buff){
        free_contig_vpages(buff->data, PAGE_COUNT);
        memset(buff, 0, sizeof(struct vbuff_buff));
}

/**
 * Find a buffer in which a suitable spot is available
 *
 * data parameter:
 *  If data value is NULL, data is set to a pointer to the best chunk.
 *  If data value is a pointer the value is copied inside the buffer
 */
int vbuff_put(struct vbuff_buff* buff, struct vbuff_data *buff_data, void** data, size_t size){
        mutex_lock(&buff->mutex);

        /* not enough space in the circular buffer */
        if(buff->size < size){
                mutex_unlock(&buff->mutex);
                return -1;
        }

        //buff->prod = 3900;

        /* if putting data in the buffer offerflow, set the productor back at the begining */
        if(buff->prod + size >= buff->size)
                buff->prod = 0;

        buff_data->offset = buff->prod;
        buff_data->size = size;

        if(*data == NULL)
                *data = (void*)buff->data + buff_data->offset;
        else
                memcpy(buff->data + buff_data->offset, *data, size);

        buff->prod += size;

        mutex_unlock(&buff->mutex);

        return 0;
}


uint8_t* vbuff_get(struct vbuff_buff* buff, struct vbuff_data *buff_data){
        printk("[Get Buff] offset: %d length: %d", buff_data->offset, buff_data->size);
        return buff->data + buff_data->offset;
}

/*
 * Update grants for a specific device
 */
void vbuff_update_grant(struct vbuff_buff* buff, struct vbus_device *dev){
        int res, i = 0;

        mutex_lock(&buff->mutex);

        if(buff->grant != GRANT_INVALID_REF)
                gnttab_end_foreign_access_ref(buff->grant);

        /* TODO save ref */
        while(i < PAGE_COUNT){
                res = gnttab_grant_foreign_access(dev->otherend_id, phys_to_pfn((uint32_t)(buff->data_phys + i * PAGE_SIZE)), 0);
                if (res < 0)
                        BUG();

                if(i == 0)
                        buff->grant = res;

                i++;
        }




        mutex_unlock(&buff->mutex);
}