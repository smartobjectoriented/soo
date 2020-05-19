/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2019 Baptiste Delporte <bonel@bonel.net>
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

#include <soo/hypervisor.h>
#include <soo/uapi/console.h>

extern int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

void (*__printch)(char c);

void __lprintk(const char *format, va_list va) {
	char buf[CONSOLEIO_BUFFER_SIZE];
	char *__start;
	int i;

	vsnprintf(buf, CONSOLEIO_BUFFER_SIZE, format, va);

	__start = buf;

	/* Skip printk prefix issued by the standard Linux printk if any */
	if ((*__start != 0) && (*__start < 10))
		__start += 2;

	for (i = 0; i < strlen(__start); i++)
		if (likely(__printch))
			__printch(__start[i]);

}

void lprintch(char c) {
	if (likely(__printch))
		__printch(c);
}

void lprintk(char *format, ...) {

	va_list va;
	va_start(va, format);

	__lprintk(format, va);

	va_end(va);
}

/**
 * Print the contents of a buffer.
 */
void lprintk_buffer(void *buffer, uint32_t n) {
	uint32_t i;

	for (i = 0 ; i < n ; i++)
		lprintk("%02x ", ((char *) buffer)[i]);
}

/**
 * Print the contents of a buffer. Each element is separated using a given character.
 */
void lprintk_buffer_separator(void *buffer, uint32_t n, char separator) {
	uint32_t i;

	for (i = 0 ; i < n ; i++)
		lprintk("%02x%c", ((char *) buffer)[i], separator);
	lprintk("\n");
}

/**
 * Print an uint64_t number and concatenate a string.
 */
void lprintk_int64_post(s64 number, char *post) {
	uint32_t msb = number >> 32;
	uint32_t lsb = number & 0xffffffff;

	lprintk("%08x %08x%s", msb, lsb, post);
}

/**
 * Print an uint64_t number.
 */
void lprintk_int64(s64 number) {
	lprintk_int64_post(number, "\n");
}
