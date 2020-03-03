/*
 * Copyright (C) 2017-2019 Baptiste Delporte <bonel@bonel.net>
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

#include <soo/soolink/discovery.h>

#include <soo/core/device_access.h>

#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/soo.h>
#include <soo/guest_api.h>
#include <soo/uapi/console.h>

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
