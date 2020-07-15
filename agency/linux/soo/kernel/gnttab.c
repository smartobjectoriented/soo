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

#ifdef CONFIG_ARM
#include <asm/mach/map.h>
#endif

#include <asm/memory.h>
#include <asm/cacheflush.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/memory.h>

#include <soo/vbus.h>
#include <soo/gnttab.h>
#include <soo/uapi/debug.h>
#include <soo/hypervisor.h>	/* hypercall defs */
#include <soo/vbstore.h>
#include <soo/uapi/console.h>

extern const struct mem_type *get_mem_type(unsigned int type);
extern int remap_area_pages(unsigned long start, unsigned long pfn, size_t size, const struct mem_type *type);
extern void unmap_kernel_range(unsigned long addr, unsigned long size);

#ifdef DEBUG
void gnttab_dump(void);
#endif

static grant_entry_t *gnttab_ME[MAX_DOMAINS];

static grant_entry_t *gnttab;

static struct vbus_watch gt_watch[MAX_DOMAINS];

/* External tools reserve first few grant table entries. -> TO BE REMOVED IN A NEAR FUTURE ! */
#define NR_RESERVED_ENTRIES 	8
#define GNTTAB_LIST_END 	(NR_GRANT_ENTRIES + 1)

static grant_ref_t gnttab_list[NR_GRANT_ENTRIES];
static int gnttab_free_count;
static grant_ref_t gnttab_free_head;
static DEFINE_SPINLOCK(gnttab_list_lock);
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

static struct gnttab_free_callback *gnttab_free_callback_list = NULL;

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

struct handle_grant *new_handle(unsigned int domid, unsigned int gref,  uint64_t host_addr, unsigned int offset, unsigned int size) {
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
struct handle_grant *get_handle(unsigned int handle) {
	struct handle_grant *entry;

	list_for_each_entry(entry, &list_handle_grant, list)
	if (entry->handle == handle)
		return entry;

	return NULL;
}

void free_handle(unsigned int handle) {
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

static int get_free_entries(int count)
{
	unsigned long flags;
	int ref;
	grant_ref_t head;

	spin_lock_irqsave(&gnttab_list_lock, flags);

	if (gnttab_free_count < count) {
		spin_unlock_irqrestore(&gnttab_list_lock, flags);
		return -1;
	}

	ref = head = gnttab_free_head;
	gnttab_free_count -= count;

	while (count-- > 1)
		head = gnttab_list[head];

	gnttab_free_head = gnttab_list[head];
	gnttab_list[head] = GNTTAB_LIST_END;

	spin_unlock_irqrestore(&gnttab_list_lock, flags);
	return ref;
}

#define get_free_entry() get_free_entries(1)

static void do_free_callbacks(void)
{
	struct gnttab_free_callback *callback, *next;

	callback = gnttab_free_callback_list;
	gnttab_free_callback_list = NULL;

	while (callback != NULL) {
		next = callback->next;
		if (gnttab_free_count >= callback->count) {
			callback->next = NULL;
			callback->fn(callback->arg);
		} else {
			callback->next = gnttab_free_callback_list;
			gnttab_free_callback_list = callback;
		}
		callback = next;
	}
}

static inline void check_free_callbacks(void)
{
	if (unlikely((long) gnttab_free_callback_list))
		do_free_callbacks();
}

static void put_free_entry(grant_ref_t ref)
{
	unsigned long flags;
	spin_lock_irqsave(&gnttab_list_lock, flags);
	gnttab_list[ref] = gnttab_free_head;
	gnttab_free_head = ref;
	gnttab_free_count++;
	check_free_callbacks();
	spin_unlock_irqrestore(&gnttab_list_lock, flags);
}

int gnttab_map_refs(struct gnttab_map_grant_ref *map_ops, struct gnttab_map_grant_ref *kmap_ops, struct page **pages, unsigned int count)
{
	int ret;

	ret = grant_table_op(GNTTABOP_map_grant_ref, map_ops, count);

	if (ret)
		return ret;

	return 0;
}

int gnttab_unmap_refs(struct gnttab_unmap_grant_ref *unmap_ops, struct gnttab_map_grant_ref *kmap_ops, struct page **pages, unsigned int count)
{
	int ret;

	ret = grant_table_op(GNTTABOP_unmap_grant_ref, unmap_ops, count);
	if (ret)
		return ret;

	return 0;
}

/*
 * Public grant-issuing interface functions
 */

int gnttab_grant_foreign_access(domid_t domid, unsigned long frame, int readonly)
{
	int ref;

	if (unlikely((ref = get_free_entry()) == -1))
		return -ENOSPC;

	gnttab[ref].frame = frame;
	gnttab[ref].domid = domid;
	wmb();
	gnttab[ref].flags = GTF_permit_access | (readonly ? GTF_readonly : 0);

#ifdef DEBUG
	lprintk("%s(%d, %08x), ref=%u\n", __func__, domid, frame, ref);
#endif

	return ref;
}
EXPORT_SYMBOL_GPL(gnttab_grant_foreign_access);

void gnttab_grant_foreign_access_ref(grant_ref_t ref, domid_t domid, unsigned long frame, int readonly)
{
	gnttab[ref].frame = frame;
	gnttab[ref].domid = domid;
	wmb();
	gnttab[ref].flags = GTF_permit_access | (readonly ? GTF_readonly : 0);

}

int gnttab_query_foreign_access(grant_ref_t ref)
{
	u16 nflags;

	nflags = gnttab[ref].flags;

	return (nflags & (GTF_reading|GTF_writing));
}

void gnttab_end_foreign_access_ref(grant_ref_t ref)
{
	u16 flags, nflags;

	nflags = gnttab[ref].flags;
	do {
		if ((flags = nflags) & (GTF_reading|GTF_writing)) {
			printk(KERN_ALERT "WARNING: g.e. still in use!\n");
			BUG();
		}
	}
	while ((nflags = atomic_cmpxchg_u16(&gnttab[ref].flags, flags, 0)) != flags) ;;
}

void gnttab_end_foreign_access(grant_ref_t ref)
{
	gnttab_end_foreign_access_ref(ref);
	put_free_entry(ref);
}

void gnttab_free_grant_reference(grant_ref_t ref)
{
	put_free_entry(ref);
}

void gnttab_free_grant_references(grant_ref_t head)
{
	grant_ref_t ref;
	unsigned long flags;
	int count = 1;
	if (head == GNTTAB_LIST_END)
		return;
	spin_lock_irqsave(&gnttab_list_lock, flags);
	ref = head;
	while (gnttab_list[ref] != GNTTAB_LIST_END) {
		ref = gnttab_list[ref];
		count++;
	}
	gnttab_list[ref] = gnttab_free_head;
	gnttab_free_head = head;
	gnttab_free_count += count;
	check_free_callbacks();
	spin_unlock_irqrestore(&gnttab_list_lock, flags);
}

int gnttab_alloc_grant_references(u16 count, grant_ref_t *head)
{
	int h = get_free_entries(count);

	if (h == -1)
		return -ENOSPC;

	*head = h;

	return 0;
}

int gnttab_claim_grant_reference(grant_ref_t *private_head)
{
	grant_ref_t g = *private_head;

	if (unlikely(g == GNTTAB_LIST_END))
		return -ENOSPC;
	*private_head = gnttab_list[g];
	return g;
}

void gnttab_release_grant_reference(grant_ref_t *private_head, grant_ref_t  release)
{
	gnttab_list[release] = *private_head;
	*private_head = release;
}

void gnttab_request_free_callback(struct gnttab_free_callback *callback, void (*fn)(void *), void *arg, u16 count)
{
	unsigned long flags;
	spin_lock_irqsave(&gnttab_list_lock, flags);
	if (callback->next)
		goto out;
	callback->fn = fn;
	callback->arg = arg;
	callback->count = count;
	callback->next = gnttab_free_callback_list;
	gnttab_free_callback_list = callback;
	check_free_callbacks();

out:
	spin_unlock_irqrestore(&gnttab_list_lock, flags);
}

void gnttab_cancel_free_callback(struct gnttab_free_callback *callback)
{
	struct gnttab_free_callback **pcb;
	unsigned long flags;

	spin_lock_irqsave(&gnttab_list_lock, flags);
	for (pcb = &gnttab_free_callback_list; *pcb; pcb = &(*pcb)->next) {
		if (*pcb == callback) {
			*pcb = callback->next;
			break;
		}
	}
	spin_unlock_irqrestore(&gnttab_list_lock, flags);
}
EXPORT_SYMBOL_GPL(gnttab_cancel_free_callback);

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

		cache_flush_all();
	}

	ret = vbus_scanf(VBT_NIL, path, "pfn", "%x", &pfn);

	/* ret with value greater than 0 means an entry found in vbstore */

	if (ret) /* If such entries exist */
	{
#ifdef CONFIG_ARM
		gnttab_ME[domID] = __arm_ioremap(pfn << PAGE_SHIFT, NR_GRANT_FRAMES * PAGE_SIZE, MT_MEMORY_RWX_NONCACHED);
#else
		gnttab_ME[domID] = ioremap_nocache(pfn << PAGE_SHIFT, NR_GRANT_FRAMES * PAGE_SIZE);
#endif
		if (!gnttab_ME[domID]) {
			lprintk("%s - line %d: Re-mapping forein page for domain %d failed.\n", __func__, __LINE__, domID);
			BUG();
		}
		 
		DBG("gnttab_ME[%d]=%08x\n", domID, gnttab_ME[domID]);

		cache_flush_all();

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

int gnttab_map(struct gnttab_map_grant_ref *op) {
	unsigned long addr;
	int err = 0;
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

#ifdef CONFIG_ARM
	if(op->size <= PAGE_SIZE)
		err = ioremap_page(addr, __pfn_to_phys(gnttab_ME[op->dom][op->ref].frame), get_mem_type(MT_MEMORY_RWX_NONCACHED));
	else
		err = ioremap_size(addr, __pfn_to_phys(gnttab_ME[op->dom][op->ref].frame), get_mem_type(MT_MEMORY_RWX_NONCACHED), op->size);
#else
	err = ioremap_page_range((addr, addr + PAGE_SIZE - 1, __pfn_to_phys(gnttab_ME[op->dom][op->ref].frame), cachemode2pgprot(_PAGE_CACHE_MODE_UC));
#endif
	if (err) {
		printk("%s failed\n", __func__);
		return -1;
	}

	/* Flush all cache */
	cache_flush_all();

	op->status = 0;

	op->dev_bus_addr = gnttab_ME[op->dom][op->ref].frame;

	handle = new_handle(op->dom, op->ref, op->host_addr, op->offset, op->size);
	op->handle = handle->handle;

	return 0;
}

int gnttab_unmap(struct gnttab_unmap_grant_ref *op) {
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

	return 0;
}

int gnttab_copy(struct gnttab_copy *op) {

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

#ifdef CONFIG_ARM
	vaddr_foreign = __arm_ioremap(pfn_to_map << PAGE_SHIFT, PAGE_SIZE, MT_MEMORY_RWX_NONCACHED);
#else
	vaddr_foreign  = ioremap_nocache(pfn_to_map << PAGE_SHIFT,  PAGE_SIZE);
#endif
	if (vaddr_foreign == NULL) {
		printk("_arm_ioremap() in %s failed\n", __func__);
		return -1;
	}

	/* Flush all cache */
	cache_flush_all();

	if (op->flags & GNTCOPY_source_gref) {
		//printk("### %s: copy from %lx to %lx  dest.offset = %d  src.offset = %d  len = %d\n", __func__, pfn_to_map, op->dest.u.gmfn,  op->dest.offset, op->source.offset, op->len);

		dst = (unsigned char *)((size_t)(pfn_to_virt(op->dest.u.gmfn)));
		memcpy(dst + op->dest.offset, vaddr_foreign + op->source.offset, op->len);

	} else { /* GNTCOPY_dest_gref */

		//printk("### %s: copy to %lx from %lx  dest.offset = %d  src.offset = %d  len = %d\n", __func__, pfn_to_map, op->source.u.gmfn,  op->dest.offset, op->source.offset, op->len);

		src = (unsigned char *)((size_t)(pfn_to_virt(op->source.u.gmfn)));
		memcpy(vaddr_foreign + op->dest.offset, src + op->source.offset, op->len);
	}

	/* No do not need any more */
	iounmap(vaddr_foreign);

	op->status = 0;

	return 0;
}

int gnttab_map_sync_copy(unsigned int h, unsigned int flags, unsigned int offset, unsigned size) {
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

	return 0;
}


int gnttab_map_with_copy(struct gnttab_map_grant_ref *op) {

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

	return 0;
}

int gnttab_unmap_with_copy(struct gnttab_unmap_grant_ref *op) {

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

	return 0;
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
int grant_table_op(unsigned int cmd, void *uop, unsigned int count)
{
	int i, ret;

	switch (cmd)
	{
	case GNTTABOP_map_grant_ref:
	{

		struct gnttab_map_grant_ref *op = (struct gnttab_map_grant_ref *) uop;

		for (i = 0; i < count; i++) {

			if (op->flags & GNTMAP_with_copy)
				ret = gnttab_map_with_copy(op);
			else
				ret = gnttab_map(op);

			op++;

			if (ret)
				printk("%s failed\n", __func__);
		}

		wmb();

		return 0;
	}
	case GNTTABOP_unmap_grant_ref:
	{

		/* The area might be freed using free_vm_area or vunmap by the caller */
		struct gnttab_unmap_grant_ref *op = (struct gnttab_unmap_grant_ref *) uop;

		for (i = 0; i < count; i++) {

			if (op->flags & GNTMAP_with_copy)
				ret = gnttab_unmap_with_copy(op);
			else
				ret = gnttab_unmap(op);

			op++;
			if (ret)
				printk("%s failed\n", __func__);

		}

		wmb();

		return 0;
	}
	case GNTTABOP_copy:
	{

		int i = 0;
		struct gnttab_copy *op = (struct gnttab_copy *) uop;

		int ret;

		for (i = 0; i < count; i++) {

			if ( ((op->source.offset + op->len) > PAGE_SIZE) || ((op->dest.offset + op->len) > PAGE_SIZE) )
			{
				printk("%s/%d GNTST_bad_copy_arg.\n", __FUNCTION__, __LINE__);
				goto error_out;
			}

			ret = gnttab_copy(op);

			if (ret)
				printk("%s failed.\n", __func__);

			op++;
		}

		wmb();

		return 0;

	}
	default:
		return -1;
	}

error_out:
	printk("%s/%d BUG !!!!\n", __FUNCTION__, __LINE__);
	BUG();
}

/*
 * Remove the grant table and all foreign entries.
 */
int gnttab_remove(bool with_vbus) {
	char path[20];

	DBG("Removing grant table ...\n");

	/* Free previous entries */
	free_pages((unsigned long) gnttab, get_order_from_pages(NR_GRANT_FRAMES));

	if (with_vbus) {
		sprintf(path, "domain/gnttab/%i", ME_domID());
		vbus_rm(VBT_NIL, path, "pfn");
	}

	return 0;
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

	/* First allocate the current domain grant table. */
	gnttab = (void *) __get_free_pages(GFP_KERNEL | __GFP_ZERO, get_order_from_pages(NR_GRANT_FRAMES));

	if (gnttab == NULL)
	{
		printk("%s/%d: Failed to alloc grant table\n", __FILE__, __LINE__);
		BUG();
	}

	DBG("Exporting grant_ref table pfn %05lx virt %p \n", virt_to_pfn((unsigned int) gnttab), gnttab);

	for (i = NR_RESERVED_ENTRIES; i < NR_GRANT_ENTRIES; i++)
		gnttab_list[i] = i + 1;

	gnttab_free_count = NR_GRANT_ENTRIES - NR_RESERVED_ENTRIES;
	gnttab_free_head  = NR_RESERVED_ENTRIES;


	/* The agency is monitoring all grant table references */
	DBG0("Registering all grant watches for each domain ...\n");
	register_watches();

	DBG0("End of gnttab_init. Well done!\n");
	return 0;
}

/*
 * Update the grant table for the post-migrated domain.
 * Update the watches as well.
 */
void postmig_gnttab_update(void) {

	gnttab_remove(false);

	/* At the moment, perform a full rebuild of grant table */
	gnttab_init();

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
