/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef IMEC_H
#define IMEC_H

#include <device/irq.h>

#include <soo/ring.h>
#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/soo.h>

typedef enum {
	IMEC_MSG_T,
	IMEC_VAL_T,
	IMEC_PFN_T,
	IMEC_IOCTL_T,
	IMEC_IOCTL_DATA_T,
	IMEC_EVENT_T,
} imec_req_type_t;

typedef struct {
	imec_req_type_t	type;
	unsigned char	spid[SOO_AGENCY_UID_SIZE];
	union {
		struct {
			unsigned int	cmd;
			unsigned long	arg;
			int		conf_id;
		} ioctl;
		struct {
			unsigned int	cmd;
			unsigned long	arg;
			int		conf_id;
			char		data[64];
		} ioctl_data;
		char data[64];
	} content;
} imec_content_t;

DEFINE_RING_TYPES(imec_ring, imec_content_t, imec_content_t);

struct imec_channel;

typedef struct imec_channel {

	imec_ring_front_ring_t	initiator;
	imec_ring_back_ring_t	peer;

	unsigned int		ring_pfn;

	unsigned int		levtchn, revtchn;  /* local and remote (peer) event channel */
	unsigned int		lirq, rirq; 	   /* local and remote IRQ */

	unsigned int		initiator_slotID, peer_slotID;

	uint32_t		vaddr;	 /* Used by the peer to handle a virtual address */

	bool			ready;

	irq_handler_t		initiator_handler;
	irq_handler_t		peer_handler;
} imec_channel_t;


void imec_close_channel(imec_channel_t *imec_channel);

int imec_initiator_setup(imec_channel_t *imec_channel);
int imec_init_channel(imec_channel_t *imec_channel, irq_handler_t event_handler);
void imec_peer_setup(imec_channel_t *imec_channel);

void imec_notify(imec_channel_t *imec_channel);

bool imec_ready(imec_channel_t *imec_channel);

bool imec_initiator(imec_channel_t *imec_channel);
bool imec_peer(imec_channel_t *imec_channel);

/* Helper functions */
void *imec_prod_request(imec_channel_t *imec_channel);
void *imec_cons_request(imec_channel_t *imec_channel);
bool imec_available_request(imec_channel_t *imec_channel);

void *imec_prod_response(imec_channel_t *imec_channel);
void *imec_cons_response(imec_channel_t *imec_channel);
bool imec_available_response(imec_channel_t *imec_channel);

#endif /* IMEC_H */
