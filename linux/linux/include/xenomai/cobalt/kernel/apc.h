/*
 * Copyright (C) 2012 Philippe Gerum <rpm@xenomai.org>.
 *
 * Xenomai is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Xenomai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xenomai; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#ifndef _COBALT_KERNEL_APC_H
#define _COBALT_KERNEL_APC_H

#include <linux/ipipe.h>

/**
 * @addtogroup cobalt_core_apc
 * @{
 */

int xnapc_alloc(const char *name,
		void (*handler)(void *cookie),
		void *cookie);

void xnapc_free(int apc);

void apc_dispatch(unsigned int virq, void *arg);

/** @} */

#endif /* !_COBALT_KERNEL_APC_H */
