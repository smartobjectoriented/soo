/*
 * Copyright (C) 2017-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <device/device.h>
#include <device/driver.h>
#include <device/serial.h>

#include <soo/dev/vuart.h>

static dev_t so3virt_serial_dev;

void __ll_put_byte(char c) {
	lprintk("%c", c);
}

static int so3virt_put_byte(char c) {

	if (!vuart_ready())
		__ll_put_byte(c);
	else
		vuart_write(&c, 1);

	return 1;
}

/*
 * Read bytes coming from the backend.
 */
static char so3virt_get_byte(bool polling) {

	if (!vuart_ready())
		DBG("## %s: failed to read (vuart not ready yet)\n", __func__);

	return vuart_read_char();
}


static int so3virt_serial_init(dev_t *dev) {

	/* Init so3virt serial */

	memcpy(&so3virt_serial_dev, dev, sizeof(dev_t));

	serial_ops.put_byte = so3virt_put_byte;
	serial_ops.get_byte = so3virt_get_byte;

	serial_ops.dev = dev;

	return 0;
}

REGISTER_DRIVER_POSTCORE("serial,so3virt", so3virt_serial_init);
