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

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/delay.h>

#include <asm/memory.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/io.h>

#include <soo/vbus.h>
#include <soo/gnttab.h>
#include <soo/paging.h>

#include <soo/uapi/debug.h>
#include <soo/hypervisor.h>	/* hypercall defs */
#include <soo/vbstore.h>
#include <soo/uapi/console.h>

extern void unmap_kernel_range(unsigned long addr, unsigned long size);

#ifdef DEBUG
void gnttab_dump(void);
#endif

static grant_entry_t *gnttab_ME[MAX_DOMAINS];

static struct vbus_watch gt_watch[MAX_DOMAINS];

/* External tools reserve first few grant table entries. -> TO BE REMOVED IN A NEAR FUTURE ! */
#define NR_RESERVED_ENTRIES 	8
#define GNTTAB_LIST_END 	(NR_GRANT_ENTRIES + 1)

static DEFINE_SPINLOCK(handle_list_lock);

static inline int get_order_from_pages(unsigned long nr_pages)
{
	int order;
	nr_pages--;
	for (order = 0; nr_pages; order++)
		nr_pages >>= 1;

	return order;
}

struct handle_grant {
	grant_ref_t gref;
	unsigned int domid;
	unsigned int handle;
	uint64_t host_addr;

	/* Sub-page mapping...*/
	unsigned int offset;
	unsigned int size;

	struct list_head list;
};

LIST_HEAD(list_handle_grant);

/*
 * Number of associations pfn<->frame table entry
 * Basically, number of foreign pages + number of pages necessary for the foreign grant table
 */
#define NR_FRAME_TABLE_ENTRIES	(NR_GRANT_FRAMES + 2)

static inline u32 atomic_cmpxchg_u16(volatile u16 *v, u16 old, u16 new)
{
	u16 ret;
	unsigned long flags;

	raw_local_irq_save(flags);

	ret = *v;
	if (likely(ret == old))
		*v = new;

	raw_local_irq_restore(flags);

	return ret;
}

/*** handle management ***/

static struct handle_grant *new_handle(unsigned int domid, unsigned int gref,  uint64_t host_addr, unsigned int offset, unsigned int size) {
	unsigned long flags;
	struct handle_grant *entry;
	unsigned int cur_handleID = 0;

	spin_lock_irqsave(&handle_list_lock, flags);

	/* Get the max handleID */
	again:
	list_for_each_entry(entry, &list_handle_grant, list)
	if (entry->handle >= cur_handleID) {
		cur_handleID = entry->handle + 1;
		if (cur_handleID == 0)
			goto again;
	}

	entry = kmalloc(sizeof(struct handle_grant), GFP_ATOMIC);

	entry->domid = domid;
	entry->gref = gref;
	entry->handle = cur_handleID;
	entry->host_addr = host_addr;
	entry->offset = offset;
	entry->size = size;

	list_add_tail(&entry->list, &list_handle_grant);
	spin_unlock_irqrestore(&handle_list_lock, flags);

	return entry;
}

/*
 * Retrieve the handle descriptor for a specific handle ID.
 */
static struct handle_grant *get_handle(unsigned int handle) {
	struct handle_grant *entry;

	list_for_each_entry(entry, &list_handle_grant, list)
	if (entry->handle == handle)
		return entry;

	return NULL;
}

static void free_handle(unsigned int handle) {
	struct handle_grant *entry;
	unsigned long flags;

	spin_lock_irqsave(&handle_list_lock, flags);

	list_for_each_entry(entry, &list_handle_grant, list)
	if (entry->handle == handle) {
		list_del(&entry->list);
		kfree(entry);
		spin_unlock_irqrestore(&handle_list_lock, flags);
		return ;
	}
	panic("should not be here...\n");
}

/*
 * Update the reference to the ME grant table.
 * Returns non-zero if a foreign table is available for the domain domID.
 */
static bool gnttab_update_peer(unsigned int domID) {
	unsigned int ret, pfn;
	char path[VBS_KEY_LENGTH];

	sprintf(path, "domain/gnttab/%d", domID);

	/* Check for a previous entry for this ME and remove it. */
	if (gnttab_ME[domID] != NULL) {
		iounmap(gnttab_ME[domID]);
		gnttab_ME[domID] = NULL;
	}

	ret = vbus_scanf(VBT_NIL, path, "pfn", "%x", &pfn);

	/* ret with value greater than 0 means an entry found in vbstore */

	if (ret) /* If such entries exist */
	{
		gnttab_ME[domID] = paging_remap(pfn << PAGE_SHIFT, NR_GRANT_FRAMES * PAGE_SIZE);
		 
		DBG("gnttab_ME[%d]=%lx\n", domID, (unsigned long) gnttab_ME[domID]);

		return true;
	} else
		return false;

}

static void gnttab_register_peer(struct vbus_watch *watch)
{
	unsigned int domID;

	/*
	 * Foreign grant table belongs to another ME domain than ourself.
	 *
	 * We assume that the pfn referring this table is locally NOT a valid pfn since it is ouside
	 * any RAM memblock that we own. Therefore, a mapping of this page is required using ioremap().
	 * Note that this function does not support remapping RAM, that's good for us since the pfn
	 * is outside our RAM region.
	 */

	sscanf(watch->node, "domain/gnttab/%d", &domID);

	gnttab_update_peer(domID);
}

void gnttab_map(struct gnttab_map_grant_ref *op) {
	unsigned long addr;
	bool gnttab_ME_ok;
	struct handle_grant *handle;

	/*
	 * It is possible that the watch did not have time to triggered after the ME
	 * has registered its grant table.
	 */
	if (gnttab_ME[op->dom] == NULL) {
		gnttab_ME_ok = gnttab_update_peer(op->dom);
		BUG_ON(!gnttab_ME_ok);
	}

	/*
	 * WARNING!!
	 *
	 * The virtual address which needs to be mapped must NOT be a kernel (linear) address, but ONLY a vmalloc address.
	 * Re-mapping kernel address (kaddr) will lead to misbehaviour of linear conversion functions such as virt_to_phys(), etc.
	 */

	addr = op->host_addr;

	DBG("%s mapping domain %d pfn: 0x%X to addr: 0x%08lX\n", __FUNCTION__, op->dom, gnttab_ME[op->dom][op->ref].frame, addr);
	DBG("remapping pfn 0x%X for ref: %d\n", gnttab_ME[op->dom][op->ref].frame, op->ref);

#ifdef DEBUG
	gnttab_dump();
#endif

	paging_remap_page_range(addr, addr + PAGE_SIZE, __pfn_to_phys(gnttab_ME[op->dom][op->ref].frame));

	op->status = 0;

	op->dev_bus_addr = gnttab_ME[op->dom][op->ref].frame;

	handle = new_handle(op->dom, op->ref, op->host_addr, op->offset, op->size);
	op->handle = handle->handle;
}

void gnttab_unmap(struct gnttab_unmap_grant_ref *op) {
	struct handle_grant *handle;

	handle = get_handle(op->handle);
	if (handle == NULL) {
		printk("%s: no handle found, strange... should not be the case...\n", __func__);
		BUG();
	}

	free_handle(op->handle);
	op->handle = 0;

	/* Invalidate the mapping */
	unmap_kernel_range(handle->host_addr, PAGE_SIZE);

	op->dev_bus_addr = 0;
}

void gnttab_copy(struct gnttab_copy *op) {

	unsigned char *vaddr_foreign;
	unsigned char *src, *dst;
	unsigned int pfn_to_map;
	grant_ref_t gref;

	/* Mapping the foreign page on our virtual address space */
	if (op->flags & GNTCOPY_source_gref) {
		gref = op->source.u.ref;
		pfn_to_map = gnttab_ME[op->source.domid][op->source.u.ref].frame;
		DBG("Processing COPY_source_gref: %lx handle: %lx pfn_to_map: %lx\n", __func__, gref, op->handle, pfn_to_map);
	} else {
		gref = op->dest.u.ref;
		pfn_to_map = gnttab_ME[op->dest.domid][op->dest.u.ref].frame;
		DBG("Processing COPY_dest_gref: %lx handle: %lx pfn_to_map: %lx\n", __func__, gref, op->handle, pfn_to_map);
	}

	vaddr_foreign = paging_remap(pfn_to_map << PAGE_SHIFT, PAGE_SIZE);

	if (op->flags & GNTCOPY_source_gref) {

		dst = (unsigned char *) phys_to_virt(__pfn_to_phys(op->dest.u.gmfn));

		memcpy(dst + op->dest.offset, vaddr_foreign + op->source.offset, op->len);

	} else { /* GNTCOPY_dest_gref */

		src = (unsigned char *) phys_to_virt(__pfn_to_phys(op->source.u.gmfn));

		memcpy(vaddr_foreign + op->dest.offset, src + op->source.offset, op->len);
	}

	/* No do not need any more */
	iounmap(vaddr_foreign);

	op->status = 0;
}

void gnttab_map_sync_copy(unsigned int h, unsigned int flags, unsigned int offset, unsigned size) {
	struct gnttab_copy op_copy;
	struct handle_grant *handle;

	op_copy.flags = flags;
	op_copy.handle = h;

	handle = get_handle(h);
	if (handle == NULL) {
		printk("%s: handle retrieval failed.\n", __func__);
		BUG();
	}

	if (size > 0) {
		/* New offset & size given in parameters */
		handle->offset = offset;
		handle->size = size;
	}

	switch (flags) {

	case GNTCOPY_source_gref:
		op_copy.source.u.ref = handle->gref;
		op_copy.source.domid = handle->domid;

		op_copy.dest.u.gmfn = virt_to_pfn(handle->host_addr);

		op_copy.len = handle->size;
		op_copy.source.offset = handle->offset;
		op_copy.dest.offset = handle->offset;
		break;

	case GNTCOPY_dest_gref:

		op_copy.dest.u.ref = handle->gref;
		op_copy.dest.domid = handle->domid;

		op_copy.source.u.gmfn = virt_to_pfn(handle->host_addr);

		op_copy.len = handle->size;
		op_copy.source.offset = handle->offset;
		op_copy.dest.offset = handle->offset;
		break;
	}

	gnttab_copy(&op_copy);
}


void gnttab_map_with_copy(struct gnttab_map_grant_ref *op) {

	struct gnttab_copy op_copy;
	struct handle_grant *handle;

	BUG_ON(gnttab_ME[(op->dom)] == NULL);

	op_copy.flags = GNTCOPY_source_gref;

	op_copy.source.u.ref = op->ref;
	op_copy.source.domid = op->dom;

	op_copy.dest.u.gmfn = virt_to_pfn(op->host_addr);

	op_copy.len = op->size;
	op_copy.source.offset = op->offset;
	op_copy.dest.offset = op->offset;

	handle = new_handle(op->dom, op->ref, op->host_addr, op->offset, op->size);
	if (handle == NULL) {
		printk("%s: handle creation failed.\n", __func__);
		BUG();
	}
	op->handle = handle->handle;
	op_copy.handle = op->handle;

	gnttab_copy(&op_copy);

	op->status = 0;
}

void gnttab_unmap_with_copy(struct gnttab_unmap_grant_ref *op) {

	struct gnttab_copy op_copy;
	struct handle_grant *handle;

	handle = get_handle(op->handle);
	if (handle == NULL) {
		printk("%s: handle not found; unmapping failed.\n", __func__);
		BUG();
	}

	op_copy.flags = GNTCOPY_dest_gref;

	op_copy.dest.u.ref = handle->gref;
	op_copy.dest.domid = handle->domid;

	op_copy.source.u.gmfn = virt_to_pfn(handle->host_addr);

	op_copy.len = handle->size;
	op_copy.source.offset = handle->offset;
	op_copy.dest.offset = handle->offset;

	op_copy.handle = op->handle;

	gnttab_copy(&op_copy);

	free_handle(op->handle);
	op->handle = 0;

	op->status = 0;
}



/* This Hypercall take the following as input parameters:
 * on a MAP operation:
 *   op->host_addr    : The virtual address were to map (if vmalloc'ed, otherwise unmodified)
 *   op->ref          : A grant table reference in the given domain table
 *   op->dom          : The exporting domain id
 *   op->flags		  : Mapping flags (ignored)
 * on a UNMAP operation :
 *   op->dev_bus_addr : The virtual address of the granted page (optional, unused)
 *   op->host_addr    : The virtual address of the granted page (optional, unused)
 *
 * This hypercall set the following as OUT parameters:
 * on a MAP operation:
 *   op->status = 0 on success
 *   op->dev_bus_addr : The virtual address of the granted page (this is used during an unmap operation)
 * on a UNMAP operation :
 *   op->status = 0 on success
 */
void grant_table_op(unsigned int cmd, void *uop, unsigned int count)
{
	int i;

	switch (cmd)
	{
	case GNTTABOP_map_grant_ref:
	{
		struct gnttab_map_grant_ref *op = (struct gnttab_map_grant_ref *) uop;

		for (i = 0; i < count; i++) {

			if (op->flags & GNTMAP_with_copy)
				gnttab_map_with_copy(op);
			else
				gnttab_map(op);

			op++;
		}

		wmb();
		break;
	}
	case GNTTABOP_unmap_grant_ref:
	{

		/* The area might be freed using free_vm_area or vunmap by the caller */
		struct gnttab_unmap_grant_ref *op = (struct gnttab_unmap_grant_ref *) uop;

		for (i = 0; i < count; i++) {

			if (op->flags & GNTMAP_with_copy)
				gnttab_unmap_with_copy(op);
			else
				gnttab_unmap(op);

			op++;
		}

		wmb();
		break;
	}
	case GNTTABOP_copy:
	{
		int i = 0;
		struct gnttab_copy *op = (struct gnttab_copy *) uop;

		for (i = 0; i < count; i++) {

			if ( ((op->source.offset + op->len) > PAGE_SIZE) || ((op->dest.offset + op->len) > PAGE_SIZE) )
			{
				printk("%s/%d GNTST_bad_copy_arg.\n", __FUNCTION__, __LINE__);
				BUG();
			}

			gnttab_copy(op);
			op++;
		}

		wmb();
		break;
	}
	}
}

/*
 * Register a set of watches for each domain which handles a grant table.
 */
void register_watches(void) {
	int domID;

	for (domID = 1; domID < MAX_DOMAINS; domID++) {

		gt_watch[domID].node = kzalloc(VBS_KEY_LENGTH, GFP_ATOMIC);
		gt_watch[domID].callback = gnttab_register_peer;

		sprintf(gt_watch[domID].node, "domain/gnttab/%d", domID);
		register_vbus_watch(&gt_watch[domID]);
	}
}

/*
 * Grant table initialization function.
 */
int gnttab_init(void)
{
	int i;

	DBG("%s: setting up...\n", __func__);

	for (i = 0; i < MAX_DOMAINS; i++)
		gnttab_ME[i] = NULL;

	/* The agency is monitoring all grant table references */
	DBG0("Registering all grant watches for each domain ...\n");
	register_watches();

	DBG0("End of gnttab_init. Well done!\n");

	return 0;
}

/* Dump contents of grant table */
void gnttab_dump(void)
{
	int i, j;
	for (i = 0; i < 50; i++) {
		for (j = 0 ; j < MAX_DOMAINS ; j++) {
			if (gnttab_ME[j])
				printk("gnttab_ME[%d][%d] = { flags = %X, domid = %u, frame = 0x%X }\n",
					j,
					i,
					gnttab_ME[j][i].flags,
					gnttab_ME[j][i].domid,
					gnttab_ME[j][i].frame);
		}
	}
}

arch_initcall(gnttab_init);
