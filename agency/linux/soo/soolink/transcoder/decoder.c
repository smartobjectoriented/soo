/*
 * Copyright (C) 2017-2019 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2017-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/slab.h>

#include <soo/soolink/transcoder.h>
#include <soo/soolink/decoder.h>
#include <soo/soolink/coder.h>

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/soo.h>
#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

/*
 * The decoder is currently able to manage only one block at a time, i.e. it can deserve one call to decoder_recv() on a specific incoming block,
 * but not more at the moment.
 */

rtdm_task_t decoder_process_task;

static struct list_head block_list;
static rtdm_mutex_t decoder_lock;
static rtdm_task_t decoder_watchdog;
static bool decoder_recv_requested = false;
static decoder_block_t *last_block = NULL;

uint32_t transcoder_discarded_packets = 0;

static rtdm_mutex_t decoder_stream_lock;
static rtdm_event_t decoder_stream_event;

/* Pointer to received data in netstream mode. Used for the sync between the requester and Datalink. */
static void *decoder_stream_data;

/*
 * Create a new block based on the received data.
 */
static decoder_block_t *create_block(sl_desc_t *sl_desc) {
	decoder_block_t *block;

	DBG("Agency UID: ");
	DBG_BUFFER(&sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

	block = (decoder_block_t *) kmalloc(sizeof(decoder_block_t), GFP_ATOMIC);
	if (!block) {
		lprintk("Cannot allocate memory for a new block!");
		BUG();
	}

	block->sl_desc = sl_desc;

	block->incoming_block = NULL;
	block->size = 0;
	block->cur_pos = NULL;
	block->block_ext_in_progress = false;
	block->discarded_block = false;
	block->complete = false;
	block->last_timestamp = ktime_to_ms(ktime_get());

	list_add_tail(&block->list, &block_list);

	return block;
}

static decoder_block_t *get_block_by_sl_desc(sl_desc_t *sl_desc) {
	struct list_head *cur;
	decoder_block_t *block;

	DBG("Agency UID: ");
	DBG_BUFFER(&sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

	list_for_each(cur, &block_list) {
		block = list_entry(cur, decoder_block_t, list);
			if (!memcmp(&block->sl_desc->agencyUID_from, &sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE)) {
				DBG("Slot found\n");
				return block;
			}
	}

	DBG("Block not found\n");
	return NULL;
}

/*
 * Receive data over the interface which is specified in the sl_desc descriptor
 */
int decoder_recv(sl_desc_t *sl_desc, void **data) {
	size_t size;

	rtdm_mutex_lock(&decoder_lock);
	decoder_recv_requested = true;
	rtdm_mutex_unlock(&decoder_lock);

	/* We still need to manage a timeout according to the specification */
	rtdm_event_wait(&sl_desc->recv_event);

	size = sl_desc->incoming_block_size;

	/*
	 * Copy received data so that the Decoder cannot free a buffer being used.
	 * The buffer will be freed by the consumer after use.
	 */
	if (likely(size)) {
		*data = xnheap_vmalloc(size);
		memcpy(*data, sl_desc->incoming_block, size);

		rtdm_mutex_lock(&decoder_lock);

		/* Free the buffer by the Decoder's side */
		xnheap_vfree(sl_desc->incoming_block);

	}
	else {
		rtdm_mutex_lock(&decoder_lock);
		*data = NULL;
	}

	decoder_recv_requested = false;

	rtdm_mutex_unlock(&decoder_lock);

	return size;
}

int decoder_rx(sl_desc_t *sl_desc, void *data, size_t size) {
	transcoder_packet_t *pkt;
	decoder_block_t *block;

	DBG("Size: %d\n", size);

	/* Bypass the Decoder if the requester is of Bluetooth or TCP type */
	if ((sl_desc->if_type == SL_IF_BT) || (sl_desc->if_type == SL_IF_TCP)) {
		pkt = (transcoder_packet_t *) data;

		/* Allocate the memory for this new (simple) block */
		sl_desc->incoming_block = xnheap_vmalloc(size - sizeof(transcoder_packet_format_t));

		/* Transfer the block frame */
		memcpy(sl_desc->incoming_block, pkt->payload, size - sizeof(transcoder_packet_format_t));
		sl_desc->incoming_block_size = size - sizeof(transcoder_packet_format_t);

		rtdm_event_signal(&sl_desc->recv_event);

		return 0;
	}

	rtdm_mutex_lock(&decoder_lock);

	/*
	 * Look for the block associated to this agency UID.
	 */
	if (!(block = get_block_by_sl_desc(sl_desc))) {
		/* If the block does not exist, create it */
		block = create_block(sl_desc);
	}

	if (!block)
		BUG();

	/* Update the slot timestamp */
	block->last_timestamp = ktime_to_ms(ktime_get());

	/* Check the kind of packet we have */
	pkt = (transcoder_packet_t *) data;

	if (pkt->u.simple.consistency_type == CODER_CONSISTENCY_SIMPLE) {

		/* Allocate the memory for this new (simple) block */
		block->incoming_block = xnheap_vmalloc(size - sizeof(transcoder_packet_format_t));

		/* Transfer the block frame */
		memcpy(block->incoming_block, pkt->payload, size - sizeof(transcoder_packet_format_t));
		block->size = size - sizeof(transcoder_packet_format_t);

		sl_desc->incoming_block_size = size - sizeof(transcoder_packet_format_t);
		sl_desc->incoming_block = block->incoming_block;

		/* If a complete block has been received with no request coming from the requester, free it */
		if (!decoder_recv_requested) {
			xnheap_vfree(block->incoming_block);
			block->incoming_block = NULL;

			/* Release the lock on the block processing */
			rtdm_mutex_unlock(&decoder_lock);

			return 0;
		}

		/*
		 * So, we are ready to forward the block to the Soolink core.
		 * Either we are in a blocking call (via decoder_recv()), or
		 * the sl_desc descriptor has a receiver callback, and we can use it.
		 */

		/*
		 * Just in case we also have a registered receiver callback in the soolink descriptor,
		 * although it would be rather a special case.
		 * At this point, we potentially lost the lock on the block.
		 * The RT callback is executed before the non RT part. The non RT part will free
		 * the buffer.
		 */
		if (sl_desc->rtdm_recv_callback)
			sl_desc->rtdm_recv_callback(sl_desc, block->incoming_block, block->size);

		rtdm_event_signal(&sl_desc->recv_event);

	} else {

		/* At the moment... */
		BUG_ON(size > sizeof(transcoder_packet_format_t) + SL_CODER_PACKET_MAX_SIZE);

		/* If we missed a packet for this block, we simply discard all subsequent packets */
		if (block->discarded_block) {
			DBG("Ignore: %d\n", pkt->u.ext.packetID);

			block->n_recv_packets++;

			/*
			 * Wait for the next packet whose packet ID is 1, meaning that this is the first
			 * packet of the next block
			 */
			if (pkt->u.ext.packetID == 1) {
				block->discarded_block = false;
				block->block_ext_in_progress = false;
			}
			else {
				/* Release the lock on the block processing */
				rtdm_mutex_unlock(&decoder_lock);

				return 0;
			}
		}

		if ((block->block_ext_in_progress) && (pkt->u.ext.packetID != block->cur_packetID + 1)) {
			DBG("Discard: %d-%d", block->cur_packetID, pkt->u.ext.packetID);

			transcoder_discarded_packets++;

			block->discarded_block = true;
			if (block->incoming_block) {
				xnheap_vfree(block->incoming_block);
				block->incoming_block = NULL;
			}

			sl_desc->incoming_block = NULL;
			sl_desc->incoming_block_size = 0;

			/* Wake up potential requester which performs a synchonous call */
			rtdm_event_signal(&sl_desc->recv_event);

			/*
			 * Release the lock on the block processing.
			 * The caller will see that the block has been discarded because incoming_block is NULL
			 * and incoming_block_size is 0.
			 */
			rtdm_mutex_unlock(&decoder_lock);

			return 0;
		}

		if (!block->block_ext_in_progress) {
			if (pkt->u.ext.packetID != 1) {
				/*
				 * We have missed some packets of a new block when processing the current block.
				 * Wait for the next packet ID = 1 to arrive.
				 */
				rtdm_mutex_unlock(&decoder_lock);
				return 0;
			}

			block->discarded_block = false;
			block->total_size = pkt->u.ext.nr_packets * SL_CODER_PACKET_MAX_SIZE;
			block->real_size = 0;
			block->n_total_packets = pkt->u.ext.nr_packets;
			block->n_recv_packets = 0;
			block->cur_packetID = -1;

			block->incoming_block = xnheap_vmalloc(block->total_size);
			BUG_ON(block->incoming_block == NULL);

			block->cur_pos = block->incoming_block;
			block->block_ext_in_progress = true;
		}

		block->n_recv_packets++;

		memcpy(block->cur_pos, pkt->payload, pkt->u.ext.payload_length);
		block->cur_pos += pkt->u.ext.payload_length;

		block->real_size += pkt->u.ext.payload_length;
		block->cur_packetID = pkt->u.ext.packetID;

		if (block->cur_packetID == pkt->u.ext.nr_packets) {
			block->size = block->real_size;

			sl_desc->incoming_block_size = block->real_size;
			sl_desc->incoming_block = block->incoming_block;

			last_block = block;

			/* If a complete block has been received with no request coming from the requester, free it */
			if (!decoder_recv_requested) {
				xnheap_vfree(block->incoming_block);
				block->incoming_block = NULL;

				/* Release the lock on the block processing */
				rtdm_mutex_unlock(&decoder_lock);

				return 0;
			}

			/*
			 * So, we are ready to forward the block to the Soolink core.
			 * Either we are in a blocking call (via decoder_recv()), or
			 * the sl_desc descriptor has a receiver callback, and we can use it.
			 */

			/*
			 * Just in case we also have a registered receiver callback in the soolink descriptor,
			 * although it would be rather a special case.
			 * At this point, we potentially lost the lock on the block.
			 * The RT callback is executed before the non RT part. The non RT part will free
			 * the buffer.
			 */
			if (sl_desc->rtdm_recv_callback)
				sl_desc->rtdm_recv_callback(sl_desc, block->incoming_block, block->size);

			block->block_ext_in_progress = false;

			/* Inform the synchronous waiter about available data */
			rtdm_event_signal(&sl_desc->recv_event);
		}

	}

	/* Release the lock on the block processing */
	rtdm_mutex_unlock(&decoder_lock);

	return 0;
}

/**
 * A recv request is being performed by the requester.
 * The pointer targeted by data points to the whole netstream transceiver packet.
 */
int decoder_stream_recv(sl_desc_t *sl_desc, void **data) {
	/* Only one RX request can be processed at a time */
	rtdm_mutex_lock(&decoder_stream_lock);

	/* Wait for a packet to come */
	rtdm_event_wait(&decoder_stream_event);

	*data = decoder_stream_data;

	rtdm_mutex_unlock(&decoder_stream_lock);

	return 0;
}

/**
 * A packet is incoming from the plugin.
 * data points to the whole netstream transceiver packet.
 */
int decoder_stream_rx(sl_desc_t *sl_desc, void *data) {
	decoder_stream_data = data;

	/* Unblock the requester RX */
	rtdm_event_signal(&decoder_stream_event);

	return 0;
}

/*
 * The following task is used to monitor the activities of allocated blocks.
 * Typically, it might happen that an extended block waits for its subsequent packets and they never arrive.
 * In this case, all received packets are discarded and the blocks are released.
 */
static void decoder_watchdog_task_fn(void *arg)  {
	struct list_head *cur, *tmp;
	decoder_block_t *block;
	s64 current_time;

	while (1) {
		rtdm_task_wait_period(NULL);

		current_time = ktime_to_ms(ktime_get());

		rtdm_mutex_lock(&decoder_lock);

		/* Look for the blocks that exceed the timeout */
		list_for_each_safe(cur, tmp, &block_list) {
			block = list_entry(cur, decoder_block_t, list);

			/*
			 * If the packet is complete and it is waiting to be consumed, do not do anything.
			 * The consumer will free the buffer.
			 */
			if ((!block->block_ext_in_progress) || (block->complete))
				continue;

			if (current_time - block->last_timestamp >= SOOLINK_DECODE_BLOCK_TIMEOUT) {
				DBG("Discard the block: ");
				DBG_BUFFER(&block->sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

				list_del(cur);

				/*
				 * Free the memory allocated for this block.
				 * The buffer should not be freed if the packet has been discarded and it has already been freed.
				 */
				if (block->incoming_block) {
					xnheap_vfree(block->incoming_block);
					block->incoming_block = NULL;
				}

				kfree(block);
			}
		}

		rtdm_mutex_unlock(&decoder_lock);
	}
}

/*
 * Main initialization function of the Decoder functional block.
 */
void decoder_init(void) {
	INIT_LIST_HEAD(&block_list);

	rtdm_mutex_init(&decoder_lock);
	rtdm_mutex_init(&decoder_stream_lock);

	rtdm_event_init(&decoder_stream_event, 0);

	/* Watchdog */
	rtdm_task_init(&decoder_watchdog, "Decoder_watchdog", decoder_watchdog_task_fn, NULL, DECODER_WATCHDOG_TASK_PRIO, DECODER_WATCHDOG_TASK_PERIOD);
}
