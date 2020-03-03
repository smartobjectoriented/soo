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

#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/bug.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include <soo/uapi/dcm.h>

#include <soo/dcm/datacomm.h>
#include <soo/dcm/compressor.h>

#include <soo/soolink/soolink.h>

#include <soo/core/device_access.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

/* The main requester descriptor managed by Soolink */
static sl_desc_t *datacomm_sl_desc = NULL;

static bool datacomm_initialized = false;

static struct task_struct *recv_thread = NULL;

extern bool sl_ready_to_send(sl_desc_t *sl_desc);

bool datacomm_ready_to_send(void) {
	return sl_ready_to_send(datacomm_sl_desc);
}

/**
 * At the moment, we experiment a broadcast (no known recipient) and
 * a fixed prio got from the DCM Core.
 */
void datacomm_send(void *ME_buffer, size_t size, uint32_t prio) {
	if (unlikely(!datacomm_initialized))
		BUG();

	sl_send(datacomm_sl_desc, ME_buffer, size, get_null_agencyUID(), prio);
}

/**
 * This function is synchronous and blocks until an incoming ME is available.
 */
static void datacomm_recv(void **ME_buffer, size_t *size_p) {
	size_t size;
	void *priv_buffer = NULL;

	if (unlikely(!datacomm_initialized))
		BUG();

	size = sl_recv(datacomm_sl_desc, &priv_buffer);

	*ME_buffer = priv_buffer;
	*size_p = size;

}

static int recv_thread_task_fn(void *data) {
	void *ME_compressed_buffer, *ME_decompressed_buffer;
	size_t compressed_size, decompressed_size;
	int ret;

	while (1) {
		/* Receive data from Soolink */
		datacomm_recv(&ME_compressed_buffer, &compressed_size);

		/* If the decoder has nothing for us... */
		if (!compressed_size)
			continue;

		if ((ret = decompress_data(&ME_decompressed_buffer, ME_compressed_buffer, compressed_size)) < 0) {
			/*
			 * If dcm_decompress_ME returns -EIO, this means that the decompressor could not
			 * decompress the ME. We have to discard it.
			 */

			vfree((void *) ME_decompressed_buffer);
			vfree((void *) ME_compressed_buffer);

			continue;
		}
		decompressed_size = ret;

		/* Release the original compressed buffer */
		vfree((void *) ME_compressed_buffer);

		ret = dcm_ME_rx(ME_decompressed_buffer, compressed_size);

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

long datacomm_init(void) {
	DBG("Registering the DCM with Soolink\n");

	/* At this point, we can start the Discovery process */
	sl_discovery_start();

	lprintk("%s: my agency UID is: ", __func__); lprintk_buffer(get_my_agencyUID(), SOO_AGENCY_UID_SIZE);

	/*
	 * By default, we are using the WLAN plugin on the MERIDA board, or the
	 * Ethernet otherwise. This behaviour might be changed later.
	 */
#if defined(CONFIG_SOOLINK_PLUGIN_WLAN)
	datacomm_sl_desc = sl_register(SL_REQ_DCM, SL_IF_WLAN, SL_MODE_UNIBROAD);
#else /* CONFIG_SOOLINK_PLUGIN_WLAN */
	datacomm_sl_desc = sl_register(SL_REQ_DCM, SL_IF_ETH, SL_MODE_UNIBROAD);
#endif /* !CONFIG_SOOLINK_PLUGIN_WLAN */

	recv_thread = kthread_run(recv_thread_task_fn, NULL, "datacomm_recv");

	datacomm_initialized = true;

	return 0;
}
