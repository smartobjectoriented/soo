/*
 * Copyright (C) 2020 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
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

#ifndef ASF_PRIV_H
#define ASF_PRIV_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdarg.h>

#include <soo/uapi/asf.h>

#define ASF_MAX_PRINT_SIZE	256

#define ASF_IMSG(fmt, ...)   asf_print(__func__, __LINE__, "ASF", "INFO", fmt, ##__VA_ARGS__)
#define ASF_EMSG(fmt, ...)   asf_print(__func__, __LINE__, "ASF", "ERROR", fmt, ##__VA_ARGS__)

/* It should not be used directly. Use ASF_EMSG or ASF_IMSG macro */
void asf_print(const char *function, int line, const char *prefix, char *level, const char *fmt, ...);


void asf_ta_installation(void);

#endif /* ASF_PRIV_H */
