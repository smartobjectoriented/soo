/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
 *
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

#include <string.h>
#include <common.h>
#include <memory.h>

#include <device/device.h>
#include <device/fdt.h>

#include <device/arch/gic.h>

#include <libfdt/libfdt_env.h>

/* Virtual address of the device tree */
addr_t *fdt_vaddr;

const struct fdt_property *fdt_find_property(void *fdt_addr, int offset, const char *propname) {
	const struct fdt_property *prop;

	prop = fdt_get_property(fdt_addr, offset, propname, NULL);
	if (prop)
		return prop;
	else
		return NULL;
}

int fdt_property_read_string(void *fdt_addr, int offset, const char *propname, const char **out_string) {
	const struct fdt_property *prop;

	prop = fdt_find_property(fdt_addr, offset, propname);
	if (prop) {
		*out_string = prop->data;
		return 0;
	}

	return -1;
}

int fdt_property_read_u32(void *fdt_addr, int offset, const char *propname, u32 *out_value) {
	const fdt32_t *val;

	val = fdt_getprop(fdt_addr, offset, propname, NULL);

	if (val) {
		*out_value = fdt32_to_cpu(val[0]);
		return 0;
	}

	return -1;
}

int fdt_find_node_by_name(void *fdt_addr, int parent, const char *nodename) {
	int node;
	const char *__nodename, *node_name;
	int len;

	fdt_for_each_subnode(node, fdt_addr, parent) {
		__nodename = fdt_get_name(fdt_addr, node, &len);

		node_name = kbasename(__nodename);
		len = strchrnul(node_name, '@') - node_name;

		if ((strlen(nodename) == len) && (strncmp(node_name, nodename, len) == 0))
			return node;

	}

	return -1;
}

/*
 * Retrieve a node matching with a specific compat string.
 * Returns -1 if no node is present.
 */
int fdt_find_compatible_node(void *fdt_addr, char *compat) {
	int offset;

	offset = fdt_node_offset_by_compatible(fdt_addr, 0, compat);

	return offset;
}

/*
 * fdt_pack_reg - pack address and size array into the "reg"-suitable stream
 */
int fdt_pack_reg(const void *fdt, void *buf, u64 *address, u64 *size)
{
	int address_cells = fdt_address_cells(fdt, 0);
	int size_cells = fdt_size_cells(fdt, 0);
	char *p = buf;

	if (address_cells == 2)
		*(fdt64_t *)p = cpu_to_fdt64(*address);
	else
		*(fdt32_t *)p = cpu_to_fdt32(*address);

	p += 4 * address_cells;

	if (size_cells == 2)
		*(fdt64_t *)p = cpu_to_fdt64(*size);
	else
		*(fdt32_t *)p = cpu_to_fdt32(*size);

	p += 4 * size_cells;

	return p - (char *)buf;
}

/**
 * fdt_find_or_add_subnode() - find or possibly add a subnode of a given node
 *
 * @fdt: pointer to the device tree blob
 * @parentoffset: structure block offset of a node
 * @name: name of the subnode to locate
 *
 * fdt_subnode_offset() finds a subnode of the node with a given name.
 * If the subnode does not exist, it will be created.
 */
int fdt_find_or_add_subnode(void *fdt, int parentoffset, const char *name)
{
	int offset;

	offset = fdt_subnode_offset(fdt, parentoffset, name);

	if (offset == -FDT_ERR_NOTFOUND)
		offset = fdt_add_subnode(fdt, parentoffset, name);

	if (offset < 0)
		printk("%s: %s: %s\n", __func__, name, fdt_strerror(offset));

	return offset;
}




