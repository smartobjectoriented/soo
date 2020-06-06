/*
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

#include <linux/random.h>

#include <soo/core/device_access.h>

#include <soo/uapi/soo.h>
#include <soo/guest_api.h>
#include <soo/uapi/avz.h>
#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>


#include <asm/io.h>

#ifdef CONFIG_ARM
#include <asm/mach/map.h>
#endif

/* For the upgrade */
uint32_t upgrade_buffer_pfn = 0;
uint32_t upgrade_buffer_size = 0;
unsigned int upgrade_ME_slotID = 5;

/* Device capabilities bitmap */
uint8_t devcaps_class[DEVCAPS_CLASS_NR];

/* Name of the Smart Object */
char devaccess_soo_name[SOO_NAME_SIZE] = { 0 };

/* Null agency UID */
static agencyUID_t null_agencyUID = {
	.id = { 0 }
};

/* SPID of the SOO.blind ME */
uint8_t SOO_blind_spid[SPID_SIZE] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x11, 0x8d };

/* SPID of the SOO.outdoor ME */
uint8_t SOO_outdoor_spid[SPID_SIZE] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0xd0, 0x08 };

/**
 * Return a NULL agency UID.
 */
agencyUID_t *get_null_agencyUID(void) {
	return &null_agencyUID;
}

/**
 * Return the agency UID of this Smart Object.
 */
agencyUID_t *get_my_agencyUID(void) {
	return (agencyUID_t *) &HYPERVISOR_shared_info->dom_desc.u.agency.agencyUID;
}

void devaccess_dump_agencyUID(void) {
	lprintk("Agency UID: ");
	lprintk_buffer((void *) &HYPERVISOR_shared_info->dom_desc.u.agency.agencyUID.id, SOO_AGENCY_UID_SIZE);
}

uint32_t devaccess_get_upgrade_pfn(void) {
    return upgrade_buffer_pfn;
}

uint32_t devaccess_get_upgrade_size(void) {
    return upgrade_buffer_size;
}

unsigned int devaccess_get_upgrade_ME_slotID(void) {
    return upgrade_ME_slotID;
}

void devaccess_store_upgrade_addr(uint32_t buffer_pfn, uint32_t buffer_size) {

    upgrade_buffer_pfn = buffer_pfn;
    upgrade_buffer_size = buffer_size;
}

void devaccess_store_upgrade(uint32_t buffer_pfn, uint32_t buffer_size, unsigned int ME_slotID) {

    printk("Storing upgrade image: pfn %u, size %u, slot %d\n", buffer_pfn, buffer_size, ME_slotID);

    upgrade_buffer_pfn = buffer_pfn;
    upgrade_buffer_size = buffer_size;
    upgrade_ME_slotID = ME_slotID;
}


/**
 * Check if a devcaps class is supported or not.
 * The class is considered as "supported" if there is at least one active attribute belonging to the class.
 * This function accepts a bit map as argument to validate several devcaps classes at once (all classes must be supported).
 * Return true if the requested devcaps is supported, false otherwise.
 */
bool devaccess_is_devcaps_class_supported(uint32_t class) {
	int i;

	/* If at least one class is not present as requested, it returns false */
	for (i = 0; i < DEVCAPS_CLASS_NR; i++)
		if (((i << 8) & class) && (devcaps_class[i] == 0))
			return false;

	return true;
}
/**
 * Parse the device capabilities bitmap and find out if the requested device capability is supported.
 * Return true if the requested dev cap is supported, false otherwise.
 */
bool devaccess_is_devcaps_supported(uint32_t class, uint8_t devcaps) {
	if (unlikely(((class >> 8) > DEVCAPS_CLASS_NR) || (devcaps > 0xff)))
		return false;

	return (devcaps_class[class >> 8] & devcaps);
}

/**
 * Set or clear the flag associated to a device capability.
 */
void devaccess_set_devcaps(uint32_t class, uint8_t devcaps, bool available) {
	if (unlikely(((class >> 8) > DEVCAPS_CLASS_NR) || (devcaps > 0xff)))
		return;

	if (available)
		devcaps_class[class >> 8] |= devcaps;
	else
		devcaps_class[class >> 8] &= ~devcaps;
}

void devaccess_dump_devcaps(void) {
	lprintk("devcaps: ");
	lprintk_buffer(devcaps_class, DEVCAPS_CLASS_NR);
}

/**
 * Set the Smart Object name.
 */
void devaccess_set_soo_name(char *name) {
	strcpy(devaccess_soo_name, name);
}

/**
 * Get the Smart Object name.
 */
void devaccess_get_soo_name(char *ptr) {
	strcpy(ptr, devaccess_soo_name);
}

void devaccess_dump_soo_name(void) {
	lprintk("SOO name: %s\n", devaccess_soo_name);
}

/**
 * Initialize the agency UID.
 */
static void init_agencyUID(void) {

	/* Generate a random UID */
	get_random_bytes((void *) &HYPERVISOR_shared_info->dom_desc.u.agency.agencyUID, SOO_AGENCY_UID_SIZE);

	lprintk("SOO Agency UID: ");
	lprintk_buffer((uint8_t *) &HYPERVISOR_shared_info->dom_desc.u.agency.agencyUID, SOO_AGENCY_UID_SIZE);

	lprintk("cpu=%d\n", smp_processor_id());
}

ssize_t agencyUID_show(struct device *dev, struct device_attribute *attr, char *buf) {
	char agencyUID_str[SOO_AGENCY_UID_SIZE * 3];
	char agencyUID_digit[3];
	uint32_t i;

	agencyUID_digit[2] = '\0';

	for (i = 0; i < SOO_AGENCY_UID_SIZE; i++) {
		sprintf(agencyUID_digit, "%02x", HYPERVISOR_shared_info->dom_desc.u.agency.agencyUID.id[i]);

		memcpy(&agencyUID_str[3 * i], agencyUID_digit, 2);
		agencyUID_str[3 * i + 2] = ':';
	}
	agencyUID_str[SOO_AGENCY_UID_SIZE * 3 - 1] = '\0';

	memcpy(buf, agencyUID_str, 3 * SOO_AGENCY_UID_SIZE);

	return 3 * SOO_AGENCY_UID_SIZE;
}

ssize_t agencyUID_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {
	/* Writing into the agency UID is not allowed */

	return size;
}

/*
 * Used for debugging purposes. We assign a specific agency UID / the last byte is significant, the others are set to 0.
 */
void set_agencyUID(uint8_t val) {
	int i;

	for (i = 0; i < 15; i++)
		HYPERVISOR_shared_info->dom_desc.u.agency.agencyUID.id[i] = 0;

	HYPERVISOR_shared_info->dom_desc.u.agency.agencyUID.id[15] = val;
	lprintk("** New SOO Agency UID: ");
	lprintk_buffer((uint8_t *) &HYPERVISOR_shared_info->dom_desc.u.agency.agencyUID, SOO_AGENCY_UID_SIZE);
	lprintk("\n");
}

void devaccess_init(void) {
	init_agencyUID();

	upgrade_buffer_pfn = 0;
	upgrade_buffer_size = 0;

	/* Initialize the device capabilities bitmap */
	memset(devcaps_class, 0, DEVCAPS_CLASS_NR);
}
