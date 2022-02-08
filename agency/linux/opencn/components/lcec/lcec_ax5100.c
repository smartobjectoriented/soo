/********************************************************************
 *  Copyright (C) 2021 Jean-Pierre Miceli Miceli <jean-pierre.miceli@heig-vd.ch>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 ********************************************************************/

#include "lcec_priv.h"
#include "lcec_ax5100.h"

extern int lcec_ax5x00_init(struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs, unsigned axis_nr);

/* Master 0, Slave 0, "AX5106-0000-0012"
 * Vendor ID:       0x00000002
 * Product code:    0x13f26012
 * Revision number: 0x000c0000
 */

static ec_pdo_entry_info_t ax5100_pdo_entries[] = {
	{0x0086, 0x00, 16}, /* Master control word */
	{0x002F, 0x00, 32}, /* Position command value */

	{0x0087, 0x00, 16}, /* Drive status word */
	{0x0033, 0x00, 32}, /* Position feedback 1 value */
};

static ec_pdo_info_t ax5100_pdos[] = {
	{0x0018, 2, ax5100_pdo_entries + 0}, /* MDT */
	{0x0010, 2, ax5100_pdo_entries + 2}, /* AT */
};

static ec_sync_info_t ax5100_syncs[] = {
	{0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
	{1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
	{2, EC_DIR_OUTPUT, 1, ax5100_pdos + 0, EC_WD_DISABLE},
	{3, EC_DIR_INPUT, 1, ax5100_pdos + 1, EC_WD_DISABLE},
	{0xff}
};

int lcec_ax5100_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs)
{
	int err;

	/* initializer sync info */
	slave->sync_info = ax5100_syncs;

	err = lcec_ax5x00_init(slave, pdo_entry_regs, LCEC_AX5100_AXIS);

	return err;
}
