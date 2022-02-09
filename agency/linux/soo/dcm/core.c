/*
 * Copyright (C) 2017 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2017,2018 Baptiste Delporte <bonel@bonel.net>
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

#if 0
#define DEBUG
#endif

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/mutex.h>

#include <soo/dcm/datacomm.h>
#include <soo/dcm/compressor.h>
#include <soo/dcm/security.h>

#include <soo/uapi/dcm.h>

#include <soo/core/device_access.h>

#include <soo/soolink/soolink.h>
#include <soo/soolink/datalink.h>
#include <soo/soolink/discovery.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

/* Protection of the buffers */
static struct mutex recv_lock;

/**
 * Receival buffers.
 */
static dcm_buffer_desc_t recv_buffers[DCM_N_RECV_BUFFERS];

/* Buffer management. The following functions must be called in a context protected by locks. */

/**
 * This function looks for a free buffer, whose status is FREE.
 * If a free buffer is found, the function returns the index of the found buffer (1 or 2).
 * If there is no free buffer, the function returns -ENOMEM.
 */
static int find_free_buffer(void) {
	uint32_t i;

	for (i = 0; i < DCM_N_RECV_BUFFERS; i++) {
		if (recv_buffers[i].status == DCM_BUFFER_FREE)
			return i;
	}

	return -ENOMEM;
}

/**
 * Perform the sending of ME.
 * Since the Agency Core did a test before sending in order to make sure it was possible to send a ME, at this point,
 * the situation did not change.
 */
static void dcm_send_ME(unsigned long arg) {
	dcm_ioctl_send_args_t args;
	void *ME_data;
	size_t size = 0;
	int ret;
#ifdef CONFIG_SOO_CORE_ASF
	void *ME_crypt;
#endif

	if ((ret = copy_from_user(&args, (void *) arg, sizeof(dcm_ioctl_send_args_t))) < 0) {
		lprintk("Error when retrieving args (%d)...\n", ret);
		BUG();
	}

	if (args.ME_data == NULL) {
		datacomm_send(NULL, 0, 0);

		return ;
	}

	/* Check for end of transmission. */
	size = compress_data(COMPRESSOR_LZ4, &ME_data, args.ME_data, args.size);

#ifdef CONFIG_SOO_CORE_ASF
	/* ME encryption */
	size = security_encrypt(ME_data, size, &ME_crypt);

	/* Prio is not supported yet. Default value: 0. */
	datacomm_send(ME_crypt, size, 0);
#else
	/* Prio is not supported yet. Default value: 0. */
	soo_log("[soo:dcm] Sending to datacomm...\n");

	datacomm_send(ME_data, size, 0);

	soo_log("[soo:dcm] After sending to datacomm...\n");
#endif

	/* Free the compressed ME area. */
	vfree((void *) ME_data);

	vfree((void *) args.ME_data);

#ifdef CONFIG_SOO_CORE_ASF
	/* Free the encrypted buffer */
	kfree(ME_crypt);
#endif

}

/* ME receival */

/**
 * Retrieve a ME to process by the Agency Core subsystem (Migration Manager)
 */
static long dcm_recv_ME(unsigned long arg) {
	dcm_buffer_desc_t *buffer_desc;
	dcm_ioctl_recv_args_t args;
	ME_info_transfer_t * info;
	static int cur_buffer_idx = 0; /* Current buffer index */
	int i, ret;

	mutex_lock(&recv_lock);

	args.ME_size = 0;
	args.buffer_size = 0;
	args.ME_data = NULL;

	/* Look for an available ME */

	for (i = 0; i < DCM_N_RECV_BUFFERS; i++) {

		buffer_desc = &recv_buffers[cur_buffer_idx];
		if (buffer_desc->status == DCM_BUFFER_BUSY) {

			DBG("Buffer %d: > DCM_BUFFER_BUSY\n", cur_buffer_idx);

			args.ME_data = buffer_desc->ME_data;
			info = (ME_info_transfer_t *) args.ME_data;

			args.buffer_size = buffer_desc->size;
			args.ME_size = info->ME_size;

			/* Go out of this loop */
			break;
		}

		cur_buffer_idx = (cur_buffer_idx + 1) % DCM_N_RECV_BUFFERS;
	}


	if ((ret = copy_to_user((void *) arg, &args, sizeof(dcm_ioctl_recv_args_t))) < 0) {
		lprintk("Error when sending args (%d)\n", ret);
		BUG();
	}

	mutex_unlock(&recv_lock);

	/* No buffer available args.ME_data = NULL */

	return 0;
}

/**
 * This function looks for a free buffer to process the new incoming ME (issued from Datacomm)
 * If a buffer is found, the function returns the index of the newly reserved buffer (1 or 2).
 * The buffer buffer will be freed by the dcm_release_ME function.
 * If there is no free buffer, the function returns -ENOMEM. The caller must free
 * the ME buffer itself.
 */
int dcm_ME_rx(void *ME_buffer, size_t size) {
	int buffer_idx;
	dcm_buffer_desc_t *buffer_desc;

	mutex_lock(&recv_lock);

	soo_log("[soo:dcm] Got a ME rx size: %x bytes\n", size);

	buffer_idx = find_free_buffer();
	if (buffer_idx < 0) {
		mutex_unlock(&recv_lock);
		return buffer_idx;
	}

	buffer_desc = &recv_buffers[buffer_idx];

	buffer_desc->ME_data = ME_buffer;
	buffer_desc->size = size;
	buffer_desc->status = DCM_BUFFER_BUSY;

	DBG("Buffer %d\n", buffer_idx);

	mutex_unlock(&recv_lock);

	return 0;
}

/* ME release */

/**
 * Release the buffer of the received and processed ME.
 */
static long dcm_release_ME(unsigned long arg) {
	void *ME_addr;
	int i;

	/* Obviously, the address placed in arg is in the kernel space (vmalloc'd by the decompressor) */
	ME_addr = (void *) arg;

	mutex_lock(&recv_lock);

	for (i = 0; i < DCM_N_RECV_BUFFERS; i++)
		if (recv_buffers[i].ME_data == ME_addr) {

			/* We can release the memory of the compressed ME. */
			vfree(ME_addr);

			recv_buffers[i].status = DCM_BUFFER_FREE;
			mutex_unlock(&recv_lock);

			return 0;
		}

	mutex_unlock(&recv_lock);

	return 0;
}

/* /dev interface */

static int dcm_open(struct inode *inode, struct file *file) {
	return 0;
}

static int soolink_dump_neighbourhood(unsigned long arg) {
	uint32_t neigh_bitmap;
	int ret;

	if ((ret = copy_from_user(&neigh_bitmap, (void *) arg, sizeof(uint32_t))) < 0) {
		lprintk("Error when retrieving args (%d)\n", ret);
		BUG();
	}

	/* If prio is to -1, we simply dump the list of neighbours */
	if (neigh_bitmap == -1) {
		soo_log("[soo:dcm] Agency UID: ");
		printk_buffer(get_my_agencyUID(), SOO_AGENCY_UID_SIZE);
		soo_log("\n");

		discovery_dump_neighbours();
	}

	return 0;
}

static long dcm_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {

	switch (cmd) {

	case DCM_IOCTL_NEIGHBOUR_COUNT:
		return sl_neighbour_count();

	case DCM_IOCTL_SEND:
		dcm_send_ME(arg);
		return 0;

	case DCM_IOCTL_RECV:
		return dcm_recv_ME(arg);

	case DCM_IOCTL_RELEASE:
		return dcm_release_ME(arg);

	case DCM_IOCTL_DUMP_NEIGHBOURHOOD:
		return soolink_dump_neighbourhood(arg);

	case DCM_IOCTL_SET_AGENCY_UID:
		set_agencyUID((uint8_t) arg);
		return 0;

	default:
		lprintk("%s: DCM ioctl %d unavailable...\n", __func__, cmd);
		panic("DCM core");
	}

	return -EINVAL;
}

static struct file_operations dcm_fops = {
	.owner          = THIS_MODULE,
	.open           = dcm_open,
	.unlocked_ioctl = dcm_ioctl,
};

/**
 * DCM initialization function.
 */
int dcm_init(void) {
	int rc, i;

	DBG("DCM subsys initializing ...\n");

	compressor_init();

	mutex_init(&recv_lock);

	for (i = 0; i < DCM_N_RECV_BUFFERS; i++)
		recv_buffers[i].status = DCM_BUFFER_FREE;

	/* Registering device */
	rc = register_chrdev(DCM_MAJOR, DCM_DEV_NAME, &dcm_fops);
	if (rc < 0) {
		printk("Cannot obtain the major number %d\n", DCM_MAJOR);
		BUG();
	}

	datacomm_init();

	return 0;
}

