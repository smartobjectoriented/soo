/*
 * Copyright (c) 2012 Linaro Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef VIRT_H
#define VIRT_H

#include <asm/processor.h>

/*
 * Flag indicating that the kernel was not entered in the same mode on every
 * CPU.  The zImage loader stashes this value in an SPSR, so we need an
 * architecturally defined flag bit here.
 */
#define BOOT_CPU_MODE_MISMATCH	PSR_STATUS_N

#define HVC_SET_VECTORS 0
#define HVC_SOFT_RESTART 1
#define HVC_RESET_VECTORS 2

#define HVC_STUB_HCALL_NR 3

#define HVC_STUB_ERR	0xbadca11

#endif /* ! VIRT_H */
