/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
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
#include <linux/string.h>

#include <soo/gnttab.h>
#include <soo/grant_table.h>

#include <soo/hypervisor.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/console.h>
#include <soo/uapi/event_channel.h>

#include <soo/vbus_me.h>

/**
 * vbus_watch_pathfmt - register a watch on a sprintf-formatted path
 * @dev: vbus device
 * @watch: watch to register
 * @callback: callback to register
 * @pathfmt: format of path to watch
 *
 * Register a watch on the given @path, using the given vbus_watch
 * structure for storage, and the given @callback function as the callback.
 * Return 0 on success, or -errno on error.  On success, the watched path
 * (@path/@path2) will be saved as @watch->node, and becomes the caller's to
 * kfree().  On error, watch->node will be NULL, so the caller has nothing to
 * free, the device will switch to %VbusStateClosing, and the error will be
 * saved in the store.
 */
void vbus_me_watch_pathfmt(struct vbus_me_device *dev, struct vbus_watch *watch, void (*callback)(struct vbus_watch *), const char *pathfmt, ...)
{
	va_list ap;
	char *path;

	va_start(ap, pathfmt);
	path = kvasprintf(GFP_ATOMIC, pathfmt, ap);
	va_end(ap);

	if (!path) {
		lprintk("%s - line %d: Allocating path for watch failed for device %s\n", __func__, __LINE__, dev->nodename);
		BUG();
	}
	vbus_watch_path(path, watch, callback);

}

/**
 * vbus_grant_ring
 * @dev: vbus device
 * @ring_mfn: mfn of ring to grant

 * Grant access to the given @ring_mfn to the peer of the given device.  Return
 * 0 on success, or -errno on error.  On error, the device will switch to
 * VbusStateClosing, and the error will be saved in the store.
 */
int vbus_me_grant_ring(struct vbus_me_device *dev, unsigned long ring_pfn)
{
	int err = gnttab_me_grant_foreign_access(dev->otherend_id, ring_pfn, 0);

	if (err < 0) {
		lprintk("%s - line %d: granting access to ring page failed / dev name: %s ring_pfn: %u\n", __func__, __LINE__, dev->nodename, ring_pfn);
		BUG();
	}

	return err;
}


/**
 * Allocate an event channel for the given vbus_me_device, assigning the newly
 * created local evtchn to *evtchn.  Return 0 on success, or -errno on error.  On
 * error, the device will switch to VbusStateClosing, and the error will be
 * saved in the store.
 */
int vbus_me_alloc_evtchn(struct vbus_me_device *dev, uint32_t *evtchn)
{
	struct evtchn_alloc_unbound alloc_unbound;
	int err;

	alloc_unbound.dom = DOMID_SELF;
	alloc_unbound.remote_dom = dev->otherend_id;

	err = hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_alloc_unbound, (long) &alloc_unbound, 0, 0);
	if (err) {
	  	lprintk("%s - line %d: allocating event channel failed / dev name: %s evtchn: %d\n", __func__, __LINE__, dev->nodename, evtchn);
		BUG();
	} else
		*evtchn = alloc_unbound.evtchn;

	return err;
}


/**
 * Bind to an existing interdomain event channel in another domain. Returns 0
 * on success and stores the local evtchn in *evtchn. On error, returns -errno,
 * switches the device to VbusStateClosing, and saves the error in VBstore.
 */
int vbus_me_bind_evtchn(struct vbus_me_device *dev, uint32_t remote_evtchn, uint32_t *evtchn)
{
	struct evtchn_bind_interdomain bind_interdomain;
	int err;

	bind_interdomain.remote_dom = dev->otherend_id;
	bind_interdomain.remote_evtchn = remote_evtchn;

	err = hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_bind_interdomain, (long) &bind_interdomain, 0, 0);
	if (err) {
		lprintk("%s - line %d: Binding to event channel %d from domain %d failed for device %s\n", __func__, __LINE__, remote_evtchn, dev->otherend_id, dev->nodename);
		BUG();
	} else {
		*evtchn = bind_interdomain.local_evtchn;
		DBG("%s: got local evtchn: %d for remote evtchn: %d\n", __func__, *evtchn, remote_evtchn);
	}

	return err;
}

/**
 * Free an existing event channel. Returns 0 on success or -errno on error.
 */
int vbus_me_free_evtchn(struct vbus_me_device *dev, uint32_t evtchn)
{
	struct evtchn_close close;
	int err;

	close.evtchn = evtchn;

	err = hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_close, (long) &close, 0, 0);
	if (err) {
		lprintk("%s - line %d: Freeing event channel %d failed for device %s\n", __func__, __LINE__, evtchn, dev->otherend_id, dev->nodename);
		BUG();
	}

	return err;
}


