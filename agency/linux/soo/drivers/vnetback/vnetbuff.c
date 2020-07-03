


#include <soo/dev/vnetbuff.h>


int vbuff_put(struct vbuff_data *buff_data, void* data, size_t size){

	return 0;
}



void* vbuff_get(struct vbuff_buff* buffs, struct vbuff_data *buff_data){
	printk("[Get packet] index: %d offset: %d length: %d", buff_data->index, buff_data->offset, buff_data->size);
	return buffs[buff_data->index].data + buff_data->offset;
}

void vbuff_remove(struct vbuff_buff* buffs, struct vbuff_data *buff_data){

}