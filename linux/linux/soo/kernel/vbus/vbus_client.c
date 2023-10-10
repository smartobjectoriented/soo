/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2018 Baptiste Delporte <bonel@bonel.net>
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
#include <linux/types.h>
#include <linux/vmalloc.h>

#include <soo/gnttab.h>		 
#include <soo/grant_table.h>

#include <soo/hypervisor.h>
#include <soo/uapi/avz.h>
#include <soo/uapi/event_channel.h>

#include <soo/evtchn.h>
#include <soo/uapi/console.h>
#include <soo/vbus.h>

#include <linux/ipipe_base.h>

const char *vbus_strstate(enum vbus_state state)
{
	static const char *const name[] = {
		[ VbusStateUnknown      ] = "Unknown",
		[ VbusStateInitialising ] = "Initialising",
		[ VbusStateInitWait     ] = "InitWait",
		[ VbusStateInitialised  ] = "Initialised",
		[ VbusStateConnected    ] = "Connected",
		[ VbusStateClosing      ] = "Closing",
		[ VbusStateClosed	      ] = "Closed",
		[ VbusStateReconfiguring] = "Reconfiguring",
		[ VbusStateReconfigured ] = "Reconfigured",
		[ VbusStateSuspending   ] = "Suspending",
		[ VbusStateSuspended    ] = "Suspended",
		[ VbusStateResuming     ] = "Resuming",
	};
	return (state < ARRAY_SIZE(name)) ? name[state] : "INVALID";
}

/**
 * vbus_watch_path - register a watch
 * @dev: vbus device
 * @path: path to watch
 * @watch: watch to register
 * @callback: callback to register
 *
 * Register a @watch on the given path, using the given vbus_watch structure
 * for storage, and the given @callback function as the callback.  Return 0 on
 * success, or -errno on error.  On success, the given @path will be saved as
 * @watch->node, and remains the caller's to free.  On error, @watch->node will
 * be NULL, the device will switch to %VbusStateClosing, and the error will
 * be saved in the store.
 */
void vbus_watch_path(char *path, struct vbus_watch *watch, void (*callback)(struct vbus_watch *))
{
	watch->node = path;
	watch->callback = callback;

	register_vbus_watch(watch);
}


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
void vbus_watch_pathfmt(struct vbus_device *dev, struct vbus_watch *watch, void (*callback)(struct vbus_watch *), const char *pathfmt, ...)
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
 * Allocate an event channel for the given vbus_device, assigning the newly
 * created local evtchn to *evtchn.  Return 0 on success, or -errno on error.  On
 * error, the device will switch to VbusStateClosing, and the error will be
 * saved in the store.
 */
void vbus_alloc_evtchn(struct vbus_device *dev, uint32_t *evtchn)
{
	struct evtchn_alloc_unbound alloc_unbound;

	alloc_unbound.dom = DOMID_SELF;

	alloc_unbound.remote_dom = dev->otherend_id;

	hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_alloc_unbound, (long) &alloc_unbound, 0, 0);
	*evtchn = alloc_unbound.evtchn;
}


/**
 * Bind to an existing interdomain event channel in another domain. Returns 0
 * on success and stores the local evtchn in *evtchn. On error, returns -errno,
 * switches the device to VbusStateClosing, and saves the error in VBstore.
 */
void vbus_bind_evtchn(struct vbus_device *dev, uint32_t remote_evtchn, uint32_t *evtchn)
{
	struct evtchn_bind_interdomain bind_interdomain;

	bind_interdomain.remote_dom = dev->otherend_id;
	bind_interdomain.remote_evtchn = remote_evtchn;

	hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_bind_interdomain, (long) &bind_interdomain, 0, 0);

	*evtchn = bind_interdomain.local_evtchn;
	DBG("%s: got local evtchn: %d for remote evtchn: %d\n", __func__, *evtchn, remote_evtchn);
}

/**
 * Free an existing event channel. Returns 0 on success or -errno on error.
 */
void vbus_free_evtchn(struct vbus_device *dev, uint32_t evtchn)
{
	struct evtchn_close close;

	close.evtchn = evtchn;

	hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_close, (long) &close, 0, 0);
}

/**
 * vbus_map_ring_valloc
 * @dev: vbus device
 * @gnt_ref: grant reference
 * @vaddr: pointer to address to be filled out by mapping
 *
 * Based on Rusty Russell's skeleton driver's map_page.
 * Map a page of memory into this domain from another domain's grant table.
 * vbus_map_ring_valloc allocates a page of virtual address space, maps the
 * page to that address, and sets *vaddr to that address.
 * Returns 0 on success, and GNTST_* (see include/interface/grant_table.h)
 * or -ENOMEM on error. If an error is returned, device will switch to
 * VbusStateClosing and the error message will be saved in VBstore.
 */
void vbus_map_ring_valloc(struct vbus_device *dev, int gnt_ref, void **vaddr)
{
	struct gnttab_map_grant_ref op = {
		.flags = GNTMAP_host_map,
		.ref   = gnt_ref,
		.dom   = dev->otherend_id,
	};
	struct vm_struct *area;

	DBG("%u\n", gnt_ref);

	*vaddr = NULL;

	area = get_vm_area(PAGE_SIZE, VM_IOREMAP);
	if (!area)
		BUG();

	op.host_addr = (unsigned long) area->addr;

	grant_table_op(GNTTABOP_map_grant_ref, &op, 1);

#ifdef DEBUG
	lprintk("op: ");
	lprintk_buffer(&op, sizeof(struct gnttab_map_grant_ref));
#endif

	if (op.status != GNTST_okay) {
		free_vm_area(area);
		lprintk("%s - line %d: Mapping in shared page %d from domain %d failed for device %s\n", __func__, __LINE__, gnt_ref, dev->otherend_id, dev->nodename);
		BUG();
	}

	/* Stuff the handle in an unused field */
	area->phys_addr = (unsigned long) op.handle;

	*vaddr = area->addr;
}

/**
 * vbus_map_ring
 * @dev: vbus device
 * @gnt_ref: grant reference
 * @handle: pointer to grant handle to be filled
 * @vaddr: address to be mapped to
 *
 * Map a page of memory into this domain from another domain's grant table.
 * vbus_map_ring does not allocate the virtual address space (you must do
 * this yourself!). It only maps in the page to the specified address.
 * Returns 0 on success, and GNTST_* (see include/interface/grant_table.h)
 * or -ENOMEM on error. If an error is returned, device will switch to
 * VbusStateClosing and the error message will be saved in VBStore.
 */
void vbus_map_ring(struct vbus_device *dev, int gnt_ref, grant_handle_t *handle, void *vaddr)
{
	struct gnttab_map_grant_ref op = {
		.host_addr = (unsigned long)vaddr,
		.flags     = GNTMAP_host_map,
		.ref       = gnt_ref,
		.dom       = dev->otherend_id,
	};

	grant_table_op(GNTTABOP_map_grant_ref, &op, 1);

	if (op.status != GNTST_okay) {
		lprintk("%s - line %d: Mapping in shared page %d from domain %d failed for device %s\n", __func__, __LINE__, gnt_ref, dev->otherend_id, dev->nodename);
		BUG();
	} else
		*handle = op.handle;
}

/**
 * vbus_unmap_ring_vfree
 * @dev: vbus device
 * @vaddr: addr to unmap
 *
 * Based on Rusty Russell's skeleton driver's unmap_page.
 * Unmap a page of memory in this domain that was imported from another domain.
 * Use vbus_unmap_ring_vfree if you mapped in your memory with
 * vbus_map_ring_valloc (it will free the virtual address space).
 * Returns 0 on success and returns GNTST_* on error
 * (see include/interface/grant_table.h).
 */
void vbus_unmap_ring_vfree(struct vbus_device *dev, void *vaddr)
{
	struct vm_struct *area;
	struct gnttab_unmap_grant_ref op = {
		.host_addr = (unsigned long)vaddr,
	};
	
	area = find_vm_area(vaddr);

	if (!area) {
		lprintk("%s - line %d: can't find mapped virtual address %p for device %s\n", __func__, __LINE__, vaddr, dev->nodename);
		BUG();
	}

	op.handle = (grant_handle_t)area->phys_addr;

	grant_table_op(GNTTABOP_unmap_grant_ref, &op, 1);

	if (op.status == GNTST_okay)
		free_vm_area(area);
	else {
		lprintk("%s - line %d: Unmapping page at handle %d error %d for device %s\n", __func__, __LINE__,  (int16_t) area->phys_addr, op.status, dev->nodename);
		BUG();
	}
}

/**
 * vbus_unmap_ring
 * @dev: vbus device
 * @handle: grant handle
 * @vaddr: addr to unmap
 *
 * Unmap a page of memory in this domain that was imported from another domain.
 * Returns 0 on success and returns GNTST_* on error
 * (see include/interface/grant_table.h).
 */
void vbus_unmap_ring(struct vbus_device *dev, grant_handle_t handle, void *vaddr)
{
	struct gnttab_unmap_grant_ref op = {
		.host_addr = (unsigned long)vaddr,
		.handle    = handle,
	};

	grant_table_op(GNTTABOP_unmap_grant_ref, &op, 1);

	if (op.status != GNTST_okay) {
		lprintk("%s - line %d: Unmapping page at handle %d error %d for device %s\n", __func__, __LINE__,  handle, op.status, dev->nodename);
		BUG();
	}
}

/**
 * vbus_read_driver_state
 * @path: path for driver
 *
 * Return the state of the driver rooted at the given store path, or
 * VbusStateUnknown if no state can be read.
 */
enum vbus_state vbus_read_driver_state(const char *path)
{
	enum vbus_state result;
	bool found;

	if (smp_processor_id() == AGENCY_RT_CPU)
		found = rtdm_vbus_gather(VBT_NIL, path, "state", "%d", &result, NULL);
	else
		found = vbus_gather(VBT_NIL, path, "state", "%d", &result, NULL);

	if (!found)
		result = VbusStateUnknown;

	return result;
}
