//
// Created by julien on 6/30/20.
//



#include <memory.h>
#include <heap.h>
#include <soo/gnttab.h>
#include <soo/grant_table.h>

#include <soo/dev/vnetbuff.h>

void vbuff_init(struct vbuff_buff* buffs){
        int i = 0;

        // Create a grant handle for each page in the buffer
        while(i < PAGE_COUNT){
                buffs[i].data = (uint8_t*)get_free_vpage();
                buffs[i].spots[0] = CHUNK_COUNT;
                buffs[i].grant = gnttab_grant_foreign_access(ME_domID(), (unsigned long)buffs[i].data, READ_ONLY);
                i++;
        }
}

void vbuff_free(struct vbuff_buff* buff){
        int i = 0;
        while(i < PAGE_COUNT){
                gnttab_end_foreign_access_ref(buff[i++].grant);
        }

        free(buff->data);
}


/**
 * Try to fit data into the buffer. (Using best fit algorithm)
 *
 * Return The offset in bytes from the pointer of the buffer
 *      Return a negative value if the data can't be fitted.
 */
int inline _vbuff_put(struct vbuff_buff* buff, struct vbuff_data *buff_data, void** data, size_t size){
        uint16_t best_spot = -1;
        uint16_t chunk_needed = (size + CHUNK_SIZE - 1) / ((uint32_t)CHUNK_SIZE);
        uint16_t best_size = CHUNK_COUNT + 1;
        uint16_t tot_chunk_seen = 0;
        void* chunk_ptr = NULL;
        int i = 0;

        while(i < CHUNK_COUNT){
                tot_chunk_seen += buff->spots[i];

                /* Better spot found (closer to the need size)*/
                if(buff->spots[i] < best_size && buff->spots[i] >= chunk_needed){
                        best_size = buff->spots[i];
                        best_spot = i;
                }

                i++;

                /* best spot found no need to search more */
                if(best_size == chunk_needed)
                        break;

                /* No more free chunks cluster left */
                if(tot_chunk_seen == CHUNK_COUNT)
                        break;
        }

        if(best_spot < 0 || best_spot >= CHUNK_COUNT)
                return -1;

        buff->spots[best_spot + chunk_needed] = (char)(best_size - chunk_needed);
        buff->spots[best_spot] = 0;


        chunk_ptr = buff->data + best_spot * CHUNK_SIZE;

        if(*data == NULL)
                *data = chunk_ptr;
        else
                memcpy(chunk_ptr, *data, size);

        /* Details allowing the other domain to retrieve data */
        buff_data->grant = buff->grant;

        buff_data->offset = best_spot * CHUNK_SIZE;

        buff_data->size = size;


        return 0;
}

/**
 * Find a buffer in which a suitable spot is available
 *
 * data parameter:
 *  If data value is NULL, data is set to a pointer to the best chunk.
 *  If data value is a pointer the value is copied inside the buffer
 */
int vbuff_put(struct vbuff_buff* buffs, struct vbuff_data *buff_data, void** data, size_t size){
        int i = 0;

        while(i < PAGE_COUNT){
                printk("%d\n", i);
                if(_vbuff_put(buffs + i++, buff_data, data, size) == 0)
                        return 0;
        }

        return -1;
}