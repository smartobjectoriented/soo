/*
 * Copyright (C) 2014-2021 Daniel Rossier <daniel.rossier@soo.tech>
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

extern int do_sync_vbstore(void *arg);

extern int do_post_migration_sync_ctrl(void *arg);
extern int do_domcall_evtchn_from_irq(void *arg);
extern int do_soo_activity(void *arg);
extern int do_sync_directcomm(void *arg);

/*
 * Used to track activities induced by a ME such as agency_ctl commands.
 * The current_soo() function can check whether it is possible to use the
 * "current" define accordingly.
 */
bool __domcall_in_progress = false;


/**
 * Domcall routines - Called by the hypervisor to run some domain routines
 */

int domcall(int cmd, void *arg)
{
	int rc = 0;

	/* No concurrency here */
	__domcall_in_progress = true;

	switch (cmd) {
	case DOMCALL_sync_vbstore:
		rc = do_sync_vbstore(arg);
		break;
	case DOMCALL_sync_directcomm:
		rc = do_sync_directcomm(arg);
		break;

	/* SOO Activity control */
	case DOMCALL_soo:
		rc = do_soo_activity(arg);
		break;

	default:
		printk("Unknowmn cmd %#x\n", cmd);
		rc = -1;
		break;
	}

	__domcall_in_progress = false;

	return rc;
}

