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

#ifndef _LCEC_AX5100_H_
#define _LCEC_AX5100_H_

/** \brief Vendor ID */
#define LCEC_AX5100_VID LCEC_BECKHOFF_VID

/** \brief Product Code */
#define LCEC_AX5100_PID 0x13f26012

/** \brief Number of axes */
#define LCEC_AX5100_AXIS  1

/** \brief Number of PDO */
#define LCEC_AX5100_PDOS (4 * LCEC_AX5100_AXIS)

int lcec_ax5100_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif /* _LCEC_AX5100_H_ */
