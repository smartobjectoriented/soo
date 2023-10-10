/*
 * Copyright (C) 2017-2019 Baptiste Delporte <bonel@bonel.net>
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

#ifndef DEVICE_ACCESS_H
#define DEVICE_ACCESS_H

#include <linux/device.h>

#include <soo/uapi/soo.h>
#include <soo/uapi/console.h>

extern uint32_t devcaps[DEVCAPS_CLASS_NR];

/* Helper function to display agencyUID or any 64-bit identifier */
static inline void soo_log_printUID(uint64_t uid) {
	int i;
	uint8_t *c = (uint8_t *) &uid;

	if (!uid)
		soo_log("n/a");
	else
		for (i = 0; i < 8; i++ ) {
			soo_log("%02x ", *(c+7-i)); /* Display byte per byte arranged from little-endian  */
		}
}

/* Helper function to display agencyUID or any 64-bit identifier */
static inline void soo_log_printlnUID(uint64_t uid) {
	soo_log_printUID(uid);
	soo_log("\n");
}

void set_agencyUID(uint64_t val);

bool devaccess_devcaps_class_supported(uint32_t class);
bool devaccess_devcaps_supported(uint32_t class, uint8_t devcaps);

void devaccess_set_devcaps(uint32_t class, uint8_t devcaps, bool available);
void devaccess_dump_devcaps(void);

void devaccess_set_soo_name(char *name);
void devaccess_get_soo_name(char *ptr);
void devaccess_dump_soo_name(void);

void devaccess_store_upgrade_addr(uint32_t update_buffer_pfn, uint32_t buffer_size);
uint32_t devaccess_get_upgrade_img(void **upgrade_img);

uint32_t devaccess_get_upgrade_size(void);
addr_t devaccess_get_upgrade_pfn(void);
unsigned int devaccess_get_upgrade_ME_slotID(void);

void devaccess_store_upgrade(addr_t update_buffer_pfn, uint32_t buffer_size, unsigned int ME_slotID);

void devaccess_init(void);

/* sysfs handlers */
ssize_t agencyUID_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t agencyUID_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
ssize_t soo_name_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t soo_name_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);


#endif /* DEVICE_ACCESS_H */
