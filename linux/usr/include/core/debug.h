/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
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

#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG
#undef DBG

#define DBG(fmt, ...) \
	do { \
		printf("(user) %s:%d > "fmt, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define BUG() do { printf("BUG\n"); fflush(stdout); exit(-1); } while (0);

#define DBG0(...) DBG("%s", ##__VA_ARGS__)

#else

#define DBG(fmt, ...)
#define RTDBG(fmt, ...)
#define DBG0(...)
#define DBG_BUFFER(buffer, ...)
#define DBG_ON__
#define DBG_OFF__

#endif

#endif /* DEBUG_H */
