/*
 *
 * ----- SO3 Smart Object Oriented (SOO) Operating System -----
 *
 * Copyright (c) 2014-2019 REDS Institute, HEIG-VD, Switzerland
 *
 * This software is released under the MIT License whose terms are defined hereafter.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Contributors:
 *
 * - August 2017: Daniel Rossier
 * - February 2019: Daniel Rossier
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
		lprintk("%c", c);
	else
		vuart_write(&c, 1);

	return 1;
}

static int so3virt_serial_init(dev_t *dev) {

	/* Init so3virt serial */

	memcpy(&so3virt_serial_dev, dev, sizeof(dev_t));

	serial_ops.put_byte = so3virt_put_byte;

	/* At the moment, there is no way to get input from SO3 directly. */

	serial_ops.dev = dev;

	return 0;
}

REGISTER_DRIVER_POSTCORE("serial,so3virt", so3virt_serial_init);
