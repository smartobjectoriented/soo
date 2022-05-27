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

#include <linux/random.h>

#include <soo/core/device_access.h>
#include <soo/core/sysfs.h>

#include <soo/guest_api.h>
#include <soo/avz.h>

#include <soo/uapi/soo.h>
#include <soo/uapi/avz.h>
#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

#include <soo/soolink/discovery.h>

#include <soo/sooenv.h>

#include <asm/io.h>

/* Device capabilities bitmap */
uint32_t __devcaps[DEVCAPS_CLASS_NR];

/**
 * Check if a devcaps class is supported or not.
 * The class is considered as "supported" if there is at least one active attribute belonging to the class.
 */

/**
 * Check if a class is present in the devcaps table.
 *
 * @param class
 * @return true if the class is present
 */
bool devaccess_devcaps_class_supported(uint32_t class) {
	int i;

	/* It returns true when the class is present in the table of devcaps */
	for (i = 0; i < DEVCAPS_CLASS_NR; i++)
		if ((__devcaps[i] >> 24) == class)
			return true;

	return false;
}

/**
 * Look at the devcaps table and check if the devcaps attribute are supported or not.
 */
bool devaccess_devcaps_supported(uint32_t class, uint8_t devcaps) {
	int i;

	for (i = 0; i < DEVCAPS_CLASS_NR; i++)
		if ((__devcaps[i] >> 24) == class) {
			if ((__devcaps[i] & 0xffffff) & devcaps)
				return true;
			else
				return false;
		}

	return false;
}

/**
 * Set or clear the flag associated to a device capability.
 */
void devaccess_set_devcaps(uint32_t class, uint8_t devcaps, bool available) {
	int i;

	for (i = 0; i < DEVCAPS_CLASS_NR; i++)
		if ((__devcaps[i] >> 24) == class) {
			if (available)
				__devcaps[i] |= devcaps;
			else
				__devcaps[i] &= ~devcaps;
		}
}

void devaccess_dump_devcaps(void) {
	int i;

	pr_cont("[soo:core:device_access] devcaps: ");
	for (i = 0; i < DEVCAPS_CLASS_NR; i++)
		lprintk(" 0x08x ", __devcaps[i]);

	lprintk("\n");
}

/**
 * Set the Smart Object name.
 */
void devaccess_set_soo_name(char *name) {

#ifndef CONFIG_SOOLINK_PLUGIN_SIMULATION
	strcpy(current_soo->name, name);
#endif
}


/**
 * Get the Smart Object name.
 */
void devaccess_get_soo_name(char *ptr) {

	/* Check if we are on the agency CPU in which we can deal with netsimul env. */
	/* Otherwise we consider the *standard* SOO name */

	strcpy(ptr, current_soo->name);
}

void devaccess_dump_soo_name(void) {
	lprintk("SOO name: %s\n", current_soo->name);
}

void agencyUID_read(char *str) {
	sprintf(str, "%16llx", AVZ_shared->dom_desc.u.agency.agencyUID);
}

/*
 * Used for debugging purposes. We assign a specific agency UID / the last byte is significant, the others are set to 0.
 */
void set_agencyUID(uint64_t val) {

	current_soo->agencyUID = val;

	discovery_update_ourself(current_soo->agencyUID);

	soo_log("[soo:core:device_access] New SOO Agency UID: ");
	soo_log_printlnUID(current_soo->agencyUID);
}

void name_read(char *str) {
	devaccess_get_soo_name(str);
}

void name_write(char *str) {
	char tmp_buf[SOO_NAME_SIZE + 1];

	/* Is the name too long? */
	if (strlen(str) >= SOO_NAME_SIZE) {
		strcpy(str, "(soo_name too long)");
		return ;
	}

	strcpy(tmp_buf, str);

	/* If the last character is a '\n', delete it */
	if (tmp_buf[strlen(tmp_buf) - 1] == '\n')
		tmp_buf[strlen(tmp_buf) - 1] = '\0';

	devaccess_set_soo_name((char *) tmp_buf);
}

void devaccess_init(void) {
	int i;

	/* Initialize the device capabilities bitmap */
	for (i = 0; i < DEVCAPS_CLASS_NR; i++)
		__devcaps[i] = 0;

	soo_sysfs_register(agencyUID, agencyUID_read, NULL);
	soo_sysfs_register(name, name_read, name_write);
}
