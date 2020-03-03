/*
 * Copyright (C) 2016 Daniel Rossier <daniel.rossier@soo.tech>

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
#include <linux/vmalloc.h>
#include <linux/semaphore.h>
#include <linux/io.h>


#ifdef CONFIG_ARM
#include <asm/mach/map.h>
#endif

#include <asm/memory.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/console.h>
#include <soo/imec.h>
#include <soo/gnttab.h>
#include <soo/evtchn.h>
#include <soo/imec.h>
#include <soo/guest_api.h>

/*
 * No more one imec channel can be initiated at the same time (until the other peer ME has been fully set up).
 */

static irqreturn_t imec_thr_isr(int irq, void *dev_id)
{
	int res;
	imec_channel_t *__imec_channel;

	__imec_channel = (imec_channel_t *) dev_id;

	DBG("%s %d: __imec_channel->peer_handler=0x%08x\n", __func__, ME_domID(), __imec_channel->peer_handler);

	/* Put the real handler */
	res = request_irq(irq, __imec_channel->peer_handler, IRQF_SHARED, "imec_interrupt", dev_id);
	if (res < 0)
		lprintk("%s: request_irq failed.\n", __func__);

	imec_peer_setup(__imec_channel);

	return IRQ_HANDLED;
}

static irqreturn_t imec_isr(int irq, void *dev_id)
{
	imec_channel_t *__imec_channel = (imec_channel_t *) dev_id;

	DBG("%s %d(0x%08x), __imec_channel->initiator_handler=0x%08x\n", __func__, ME_domID(), __imec_channel, __imec_channel->initiator_handler);
	DBG("%s %d(0x%08x), __imec_channel->peer_handler=0x%08x\n", __func__, ME_domID(), __imec_channel, __imec_channel->peer_handler);

	if (__imec_channel->ready)
		return IRQ_HANDLED;
	else
		return IRQ_WAKE_THREAD;
}

/*
 * Prepare a IMEC channel to be used between two MEs (called initiator and peer ME).
 * Configure the forward shared ring (meaning that we are playing the frontend role)
 * The event handler must take care about forward and backward shared rings (two different event channels).
 *
 * This function is executed by the initiator.
 */
int imec_init_channel(imec_channel_t *imec_channel, irq_handler_t event_handler)
{
	int res;
	struct evtchn_alloc_unbound alloc_unbound;

	DBG("%s(0x%08x)\n", __func__, imec_channel);

	imec_channel->ready = false;

	/* Pre-allocate an event channel associated to this imec channel */

	/* Initiator (frontend) side */
	alloc_unbound.dom = ME_domID();
	alloc_unbound.remote_dom = DOMID_SELF;

	res = hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_alloc_unbound, (long) &alloc_unbound, 0, 0);

	if (res) {
		lprintk("%s: vbus_alloc_evtchn failed!\n", __func__);
		return -1;
	}

	imec_channel->levtchn = alloc_unbound.evtchn;

	res = bind_evtchn_to_virq_handler(imec_channel->levtchn, imec_isr, imec_thr_isr, IRQF_SHARED, "imec_interrupt", imec_channel);
	if (res <= 0) {
		lprintk("%s: binding event channel failed!\n", __func__);
		return -1;
	}

	imec_channel->lirq = res;
	imec_channel->initiator_handler = event_handler;

  DBG("%s %d: imec_channel->initiator_handler=0x%08x\n", __func__, ME_domID(), imec_channel->initiator_handler);

	/* Create a thread IRQ which will be used to set up the bindings */

	return 0;
}
/*
 * Finalize initialization of the initiator ME (frontend side)
 */
int imec_initiator_setup(imec_channel_t *imec_channel)
{
	int res;

	DBG("%s(0x%08x)\n", __func__, imec_channel);

	DBG("bind_existing_interdomain_evtchn(%d, %d, %d)\n", imec_channel->levtchn, imec_channel->peer_slotID-1, imec_channel->revtchn);

	/* Realize the binding between the two event channels */
	res = bind_existing_interdomain_evtchn(imec_channel->levtchn, imec_channel->peer_slotID-1, imec_channel->revtchn);
	if (res < 0) {
		lprintk("%s: Bind existing interdomain evtchn failed!\n", __func__);
		return -1;
	}

	/* Allocate a shared page for the forward ring */
	imec_channel->initiator.sring = (imec_ring_sring_t *) __get_free_page(GFP_NOIO | __GFP_HIGH);
	if (!imec_channel->initiator.sring) {
		lprintk("%s: Getting a free page for sring failed!\n", __func__);
		return -1;
	}

	SHARED_RING_INIT(imec_channel->initiator.sring);
	FRONT_RING_INIT(&imec_channel->initiator, imec_channel->initiator.sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	imec_channel->ring_pfn = virt_to_pfn((unsigned long) imec_channel->initiator.sring);

	DBG("%s %d: imec_channel->initiator_handler=0x%08x\n", __func__, ME_domID(), imec_channel->initiator_handler);

	/* Reconfigure the IRQ handler */
	res = request_irq(imec_channel->lirq, imec_channel->initiator_handler, IRQF_SHARED, "imec_interrupt", imec_channel);
	if (res < 0)
		lprintk("%s: request_irq failed.\n", __func__);

	/* Send the notification to the peer so that it can configure itself. */
	imec_notify(imec_channel);

	return 0;
}

bool imec_ready(imec_channel_t *imec_channel)
{
	DBG("%s(0x%08x): %d\n", __func__, imec_channel, imec_channel->ready);

	return imec_channel->ready;
}

bool imec_initiator(imec_channel_t *imec_channel)
{
	return (ME_domID() == imec_channel->initiator_slotID-1);
}

bool imec_peer(imec_channel_t *imec_channel)
{
	return (ME_domID() == imec_channel->peer_slotID-1);
}

int imec_peer_setup(imec_channel_t *imec_channel)
{
	imec_ring_sring_t *sring;
	unsigned int rc;

	DBG("%s(0x%08x)\n", __func__, imec_channel);

	/* Map the shared page*/

	sring = NULL;

	imec_channel->area = alloc_vm_area(PAGE_SIZE, NULL);
	if (!imec_channel->area) {
		lprintk("%s: mapping the shared page (shared ring) failed!\n", __func__);
		return -1;
	}

	sring = imec_channel->area->addr;

#ifdef CONFIG_ARM
	rc = ioremap_page((unsigned long) sring, __pfn_to_phys(imec_channel->ring_pfn), get_mem_type(MT_MEMORY_RWX_NONCACHED));
#else
	rc = ioremap_page_range((unsigned long) sring, (unsigned long) sring + PAGE_SIZE - 1, __pfn_to_phys(imec_channel->ring_pfn), cachemode2pgprot(_PAGE_CACHE_MODE_UC));
#endif

	if (rc) {
		lprintk("%s failed with rc = %d\n", __func__, rc);
		return -1;
	}

	cache_flush_all();

	sring = imec_channel->area->addr;

	SHARED_RING_INIT(sring);
	BACK_RING_INIT(&imec_channel->peer, sring, PAGE_SIZE);

	/* Now we are ready to use the IMEC channel fully. */
	imec_channel->ready = true;

	DBG("IMEC channel 0x%08x now ready\n", imec_channel);

	return 0;
}

/*
 * Close the allocated resources for this IMEC channel.
 */
void imec_close_channel(imec_channel_t *imec_channel)
{

	if (!imec_channel->ready)
		return ;

	imec_channel->ready = false;
	mb();

	if (imec_initiator(imec_channel)) {

		unbind_from_virqhandler(imec_channel->lirq, imec_channel);

		free_page((unsigned long) imec_channel->initiator.sring);

	} else {

		vunmap_page_range((unsigned long) imec_channel->peer.sring, ((unsigned long) imec_channel->peer.sring) + PAGE_SIZE);
		free_vm_area(imec_channel->area);
		unbind_from_virqhandler(imec_channel->rirq, imec_channel);

	}

	cache_flush_all();
}


void imec_notify(imec_channel_t *imec_channel)
{
	DBG("%s(0x%08x)\n", __func__, imec_channel);

	if (imec_initiator(imec_channel)) {
	  DBG("l evtchn=%d\n", evtchn_from_irq(imec_channel->lirq));
		notify_remote_via_virq(imec_channel->lirq);
	}
	else {
	  DBG("r evtchn=%d\n", evtchn_from_irq(imec_channel->rirq));
		notify_remote_via_virq(imec_channel->rirq);
	}
}

void *imec_prod_request(imec_channel_t *imec_channel)
{
	void *req;

	req = RING_GET_REQUEST(&imec_channel->initiator, imec_channel->initiator.sring->req_prod);
	mb();
	imec_channel->initiator.sring->req_prod++;

	return req;
}

void *imec_cons_request(imec_channel_t *imec_channel)
{
	void *req;

#if 0
	if (imec_channel->peer.sring->req_cons == imec_channel->peer.sring->req_prod)
		return NULL;
#endif
	if (!imec_available_request(imec_channel))
		return NULL;

	req = RING_GET_REQUEST(&imec_channel->peer, imec_channel->peer.sring->req_cons);
	mb();
	imec_channel->peer.sring->req_cons++;

	return req;
}

bool imec_available_request(imec_channel_t *imec_channel) {
	return !(imec_channel->peer.sring->req_cons == imec_channel->peer.sring->req_prod);
}

void *imec_prod_response(imec_channel_t *imec_channel)
{
	void *rsp;

	rsp = RING_GET_RESPONSE(&imec_channel->peer, imec_channel->peer.sring->rsp_prod);
	mb();
	imec_channel->peer.sring->rsp_prod++;

	return rsp;
}

void *imec_cons_response(imec_channel_t *imec_channel)
{
	void *rsp;

#if 0
	if (imec_channel->initiator.sring->rsp_cons == imec_channel->initiator.sring->rsp_prod)
		return NULL;
#endif
	if (!imec_available_response(imec_channel))
		return NULL;

	rsp = RING_GET_RESPONSE(&imec_channel->initiator, imec_channel->initiator.sring->rsp_cons);
	mb();
	imec_channel->initiator.sring->rsp_cons++;

	return rsp;
}

bool imec_available_response(imec_channel_t *imec_channel) {
	return !(imec_channel->initiator.sring->rsp_cons == imec_channel->initiator.sring->rsp_prod);
}
