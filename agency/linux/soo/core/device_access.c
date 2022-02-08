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

#include <soo/guest_api.h>

#include <soo/uapi/soo.h>
#include <soo/uapi/avz.h>
#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

#include <soo/soolink/discovery.h>

#include <soo/sooenv.h>

#include <asm/io.h>

/* Null agency UID */
static agencyUID_t null_agencyUID = {
	.id = { 0 }
};

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
	return &current_soo->agencyUID;
}

void devaccess_dump_agencyUID(void) {
	soo_log("[soo:core:device_access] Agency UID: ");
	soo_log_printlnUID(&current_soo->agencyUID);
}

/**
 * Set the Smart Object name.
 */
void devaccess_set_soo_name(char *name) {
	strcpy(current_soo->name, name);
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
		current_soo->agencyUID.id[i] = 0;

	current_soo->agencyUID.id[15] = val;

#ifndef CONFIG_X86
	discovery_update_ourself(&current_soo->agencyUID);
#endif

	soo_log("[soo:core:device_access] New SOO Agency UID: ");
	soo_log_printlnUID(&current_soo->agencyUID);
}

ssize_t soo_name_show(struct device *dev, struct device_attribute *attr, char *buf) {
	devaccess_get_soo_name(buf);

	return strlen(buf);
}

ssize_t soo_name_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {
	char tmp_buf[SOO_NAME_SIZE + 1];

	/* Is the name too long? */
	if (strlen(buf) >= SOO_NAME_SIZE)
		return size;

	strcpy(tmp_buf, buf);

	/* If the last character is a '\n', delete it */
	if (tmp_buf[strlen(tmp_buf) - 1] == '\n')
		tmp_buf[strlen(tmp_buf) - 1] = '\0';

	devaccess_set_soo_name((char *) tmp_buf);

	return size;
}

void devaccess_init(void) {

}
