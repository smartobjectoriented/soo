/*
 * Copyright (C) 2016 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2017,2018 Baptiste Delporte <bonel@bonel.net>
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

#ifndef AVZ_SOO_H
#define AVZ_SOO_H

#include <avz/types.h>

/* Device tree features */
#define ME_FEAT_ROOT		"/me_features"

#define	ME_FEAT_REALTIME	"realtime" /* 0 means non-realtime / 1 means realtime */

typedef struct {
	bool realtime;
} dtb_feat_t;

int soo_pre_activate(unsigned int slotID);
int soo_cooperate(unsigned int slotID);
void shutdown_ME(unsigned int ME_slotID);

ME_state_t get_ME_state(unsigned int ME_slotID);
void set_ME_state(unsigned int ME_slotID, ME_state_t state);

void set_dom_realtime(struct domain *d, bool realtime);
bool is_dom_realtime(struct domain *d);

#endif /* SOO_H */
