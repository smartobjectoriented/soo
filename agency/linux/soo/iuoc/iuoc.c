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

#include <stdarg.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h> /* register_chrdev, unregister_chrdev */
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/sched.h>

#include <linux/delay.h> /* usleep_range */

#include <soo/iuoc/iuoc.h>
#include <soo/uapi/iuoc.h>
#include <soo/dev/viuoc.h>


static int major = -1;
static struct cdev iuoc_cdev;
static struct class *iuoc_class = NULL;

struct completion data_wait_lock;

struct iuoc_me_data_list {
    iuoc_data_t me_data;
    struct list_head list;
};

static LIST_HEAD(iuoc_me_data_head);

iuoc_data_t *me_received;

static struct task_struct *debug_thread;
int debug_count = 0;
iuoc_data_t data_debug;
field_data_t field_debug;

static void forward_data(iuoc_data_t iuoc_data);

/**
 * @brief This function is called, when the device file is opened
 */
static int iuoc_open(struct inode *device_file, struct file *instance) 
{
	printk("close /dev/soo/iuoc\n");
	return 0;
} 

/**
 * @brief This function is called, when the device file is opened
 */
static int iuoc_close(struct inode *device_file, struct file *instance) 
{
	printk("open /dev/soo/iuoc\n");
	return 0;
}

static int debug_thread_fn(void *data) 
{
	while (1) {
	    usleep_range(3000000, 3000001);

		data_debug.me_type = IUOC_ME_BLIND;
		data_debug.timestamp = 20 * debug_count;
		strcpy(field_debug.name, "action");  
		strcpy(field_debug.type, "int");  
		field_debug.value = 3;
		data_debug.data_array[0] = field_debug;
		data_debug.data_array_size = 1;
		add_iuoc_element_to_queue(data_debug);
		debug_count++;
		complete(&data_wait_lock);
		printk("GOWAIT\n");
	}
	
	return 0;
}

void add_iuoc_element_to_queue(iuoc_data_t data)
{
	struct iuoc_me_data_list *entry;
	entry = kmalloc(sizeof(struct iuoc_me_data_list), GFP_KERNEL);
    entry->me_data = data;
    list_add_tail(&entry->list, &iuoc_me_data_head);
	printk("[IUOC driver] New data put in queue, timestamp=%d\n", data.timestamp);
	complete(&data_wait_lock);
}


/* Global Variable for reading and writing */
static long int iuoc_ioctl(struct file *file, unsigned cmd, unsigned long arg) 
{ 
	iuoc_data_t iuoc_data;
	field_data_t field_data;
    struct iuoc_me_data_list *tmp;
	printk("[IUOC driver] Entering in iuoc_ioctl\n");
	switch(cmd) {
	case UIOC_IOCTL_SEND_DATA:
		if(copy_from_user(&iuoc_data, (iuoc_data_t *) arg, sizeof(iuoc_data))) {
			printk("[IUOC driver] Driver IOCTL_SEND_DATA forwarding to FE!\n");
		}
		//forward_data(iuoc_data);
		viuoc_send_data_to_fe(iuoc_data);

		break;

	case UIOC_IOCTL_RECV_DATA:
		printk("[IUOC driver] Receive data before wait\n");

		wait_for_completion(&data_wait_lock);

		tmp = list_first_entry(&iuoc_me_data_head, struct iuoc_me_data_list, list);

		printk("[IUOC driver] Data in me : timestamp = %d, data = %s \n",  tmp->me_data.timestamp, tmp->me_data.data_array[0].name);

		if(copy_to_user((iuoc_data_t *) arg, &(tmp->me_data), sizeof(iuoc_data))) 
			printk("ioctl_example - Error copying data to user!\n");

		list_del(&tmp->list);

		 break;

	case UIOC_IOCTL_TEST:
		if(copy_from_user(&iuoc_data, (iuoc_data_t *) arg, sizeof(iuoc_data))) 
			printk("ioctl_example - Error copying data from user!\n");
		else
			printk("Greetings from kernel !\n");
		break;
	}
	return 0;
}

struct file_operations iuoc_fops = {
	.owner = THIS_MODULE,
	.open = iuoc_open,
	.release = iuoc_close,
	.unlocked_ioctl = iuoc_ioctl
};
 
/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int iuoc_init(void) 
{

    int device_created = 0;

    if (alloc_chrdev_region(&major, 0, 1, NAME "_proc") < 0) {
		printk("[IUOC] alloc_chrdev_region failed\n");
		BUG();
	}

    if ((iuoc_class = class_create(THIS_MODULE, NAME "_sys")) == NULL) {
		printk("[IUOC] class_create failed\n");
		BUG();
	}

    if (device_create(iuoc_class, NULL, major, NULL, NAME) == NULL) {
		printk("[IUOC] device_create failed\n");
		BUG();	
	}

    device_created = 1;
    cdev_init(&iuoc_cdev, &iuoc_fops);
    if (cdev_add(&iuoc_cdev, major, 1) == -1) {
		printk("[IUOC] cdev_add failed\n");
		BUG();		
	}

	me_received = kmalloc(sizeof(iuoc_data_t), GFP_KERNEL);
	if(!me_received) {
		printk("[IUOC] kmalloc me_received failed\n");
		BUG();		
	}

	init_completion(&data_wait_lock);

    // debug_thread = kthread_create(debug_thread_fn, NULL, "debug_thread");

	// if (IS_ERR(debug_thread)) {
    //     printk(KERN_ERR "Failed to create thread\n");
    //     return PTR_ERR(debug_thread);
    // }
    // // Start the thread
    // wake_up_process(debug_thread);

	return 0;
}

void forward_data(iuoc_data_t iuoc_data) 
{
	int i;
	printk("[IUOC] Data sent IUOC_ME_SWITCH -> SOO : me_type=%d, timestamp=%d, nb_data=%d \ndatas:", 
			iuoc_data.me_type, iuoc_data.timestamp, iuoc_data.data_array_size);
	for (i = 0; i < iuoc_data.data_array_size; i++) {
		printk("data nb %d : name=%s, type=%s, value=%d\n", i, iuoc_data.data_array[i].name,
						iuoc_data.data_array[i].type, iuoc_data.data_array[i].value);
	}		
}


late_initcall(iuoc_init);
