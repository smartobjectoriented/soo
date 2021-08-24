/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2017-2019 Baptiste Delporte <bonel@bonel.net>
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

#include <linux/types.h>
#include <linux/init.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <soo/soolink/plugin.h>
#include <soo/uapi/debug.h>
#include <soo/soolink/plugin/ethernet.h>
#include <soo/soolink/lib/socketmgr.h>
#include <soo/soolink/lib/tcpclient.h>
#include <soo/dcm/datacomm.h>
#include <linux/vmalloc.h>
#include <soo/dcm/compressor.h>
#include <soo/uapi/dcm.h>

#define SERVER_CMD 0x4
#define WAKE_UP 0

static struct socket *sockClient;


typedef struct {
    unsigned char *ip;
    unsigned port;
} init_param_t;

static unsigned nb_try = 100000;
static struct task_struct *__ts_wait_eth;
int soo_space_server = 0;
init_param_t param;

int connect_server(unsigned char *ip,unsigned port){

   int ret;

   sockClient = do_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

   if(!sockClient){
       return -1;
   }

   ret = do_connect(sockClient,ip,port);

   return ret;

}

int TCP_send_ME_to_server(void* ME, size_t me_size){

    int ret;
    unsigned size = me_size;

    //send the size of ME 
    ret = server_sendmsg(sockClient, &size, sizeof(unsigned));
    if (ret < 0){
        return ret;
    } 

    //send ME 
    ret = server_sendmsg(sockClient, ME, me_size);
   
    return ret;
    
}
/**
 * This function is synchronous and blocks until an incoming ME is available.
 */
int TCP_rcv_ME_from_server(void** new_ME,size_t* size){
    unsigned size_of_incoming_me;
    int ret;

    //get size of new ME
    ret = server_rcvmsg(sockClient,&size_of_incoming_me,sizeof(unsigned));
    if (ret < 0){
        return ret;
    }
    *size  = size_of_incoming_me;

    //alloc new ME buffer
    *new_ME = vmalloc(*size);

    //get ME from server
    ret = server_rcvmsg(sockClient,*new_ME,*size);

    return ret;

}

int sendTimeout_rsp(void){
    int ret;
    int size = 0x4;
    
     //send the size of ME 
    ret = server_sendmsg(sockClient, &size, sizeof(unsigned));
   
    return ret;
    
}

int recv_from_server(void) {

	void *ME_compressed_buffer, *ME_decompressed_buffer;
	size_t compressed_size, decompressed_size;
	int ret;
	
    #ifdef CONFIG_SOO_CORE_ASF
	int size;
	void *ME_decrypt;
    #endif


	while (1) {
		/* Receive data from Soospace*/
         
        printk("[%s]: wait data from SOO.space server", __func__);
		ret =  TCP_rcv_ME_from_server(&ME_compressed_buffer, &compressed_size);

        if(ret == 0){
            if(compressed_size == SERVER_CMD){
                printk("[%s]: server check timeout every %u ms", __func__,*((unsigned*)ME_compressed_buffer));
                vfree((void *) ME_compressed_buffer);
                continue;

            }else{
                printk("[%s]: new data compressed_size : %u KB ", __func__,compressed_size/1024);
            }
             
        }else{
            printk("[%s]: error rcv : %d ", __func__,ret);
            continue;
        }

        

       
		/* If the decoder has nothing for us... */
		if (!compressed_size)
			continue;

#ifdef CONFIG_SOO_CORE_ASF
		size = security_decrypt(ME_compressed_buffer, compressed_size, &ME_decrypt);
		if (size <= 0)
			continue;

		if ((ret = decompress_data(&ME_decompressed_buffer, ME_decrypt, size)) < 0) {
			/*
			 * If dcm_decompress_ME returns -EIO, this means that the decompressor could not
			 * decompress the ME. We have to discard it.
			 */

			vfree((void *) ME_decompressed_buffer);
			vfree((void *) ME_compressed_buffer);
			kfree(ME_decrypt);
			continue;
		}

		decompressed_size = ret;

		/* Release the original compressed buffer */
		kfree(ME_decrypt);

#else /* !CONFIG_SOO_CORE_ASF */

		if ((ret = decompress_data(&ME_decompressed_buffer, ME_compressed_buffer, compressed_size)) < 0) {
			/*
			 * If dcm_decompress_ME returns -EIO, this means that the decompressor could not
			 * decompress the ME. We have to discard it.
			 */

			vfree((void *) ME_decompressed_buffer);
			vfree((void *) ME_compressed_buffer);
			continue;
		}
#endif /* !CONFIG_SOO_CORE_ASF */

		decompressed_size = ret;

		/* Release the original compressed buffer */
		vfree((void *) ME_compressed_buffer);
        printk("[%s]: start new ME from sooSpace server decompressed size: %u KB ", __func__,decompressed_size/1024);
		ret = dcm_ME_rx(ME_decompressed_buffer, decompressed_size);

		/*
		 * If dc_recv_ME returns -ENOMEM, this means that there is no free buffer.
		 * We have to discard the ME and free its buffer ourselves.
		 * Otherwise, the ME buffer will be freed by the dcm_release_ME function.
		 */
		if (ret < 0)
			vfree((void *) ME_decompressed_buffer);
	}


	return 0;
}

static int wait_plugin_eth_thread_task_fn(void *data) {

     int ret = -1;
    struct net_device *net_dev;

    net_dev = NULL;
   
    /*while (!net_dev || (net_dev && net_dev->state)) {
		msleep(1000);
		net_dev = dev_get_by_name(&init_net, "eth0");
	}*/
     
  
    
     while(ret < 0 && nb_try--){
          ret = connect_server(param.ip,param.port);
          msleep(1000);
     }

     if(ret < 0){
         printk("[%s]: SOO.space server unreachable", __func__);
         return 0;
     }


     soo_space_server = 1;

     recv_from_server();

    return 0;

}

void init_client_soo_space(unsigned char *ip,unsigned port){
   
    param.ip = ip;
    param.port = port;


      //start  thread for recev me
	__ts_wait_eth = kthread_create(wait_plugin_eth_thread_task_fn, NULL, "wait_eth0");
	 add_thread(current_soo, __ts_wait_eth->pid);
	 wake_up_process(__ts_wait_eth);

}
