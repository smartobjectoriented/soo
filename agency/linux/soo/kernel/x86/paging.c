/*
 * Copyright (C) 2016-2021 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <linux/io.h>

#include <asm/pgtable.h>

#include <soo/paging.h>

/*
 * Enable paging of pages outside the RAM, but not belonging to I/O devices.
 */
void *paging_remap(unsigned long paddr, size_t size) {
	void *vaddr;

	vaddr = ioremap_nocache(paddr, size);

	return vaddr;
}
void paging_remap_page_range(unsigned long addr, unsigned long end, phys_addr_t physaddr) {
	int ret;

	ret = ioremap_page_range(addr, end, physaddr, cachemode2pgprot(_PAGE_CACHE_MODE_UC));
	BUG_ON(!ret);

}

