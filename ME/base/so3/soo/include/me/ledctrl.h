/*
 * Copyright (C) 2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef LEDCTRL_H
#define LEDCTRL_H

#include <completion.h>

typedef struct {

	struct completion upd_lock;

	int local_nr;
	int incoming_nr;

	/* To determine if the ME needs to be propagated.
	 * If it is the same state, no need to be propagated.
	 */
	bool need_propagate;

} sh_ledctrl_t;

/* Export the reference to the shared content structure */
extern sh_ledctrl_t *sh_ledctrl;

#endif /* LEDCTRL_H */


