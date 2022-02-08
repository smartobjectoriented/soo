//
//    Copyright (C) 2015 Claudio lorini <claudio.lorini@iit.it>
//    Copyright (C) 2011 Sascha Ittner <sascha.ittner@modusoft.de>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
/********************************************************************
 *  Copyright (C) 2012 Sascha Ittner <sascha.ittner@modusoft.de>
 *  Copyright (C) 2015 Claudio lorini <claudio.lorini@iit.it>
 *  Copyright (C) 2019 Jean-Pierre Miceli Miceli <jean-pierre.miceli@heig-vd.ch>
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

#ifndef _LCEC_TSD80E_H_
#define _LCEC_TSD80E_H_

/** \brief Vendor ID */
#define LCEC_TSD80E_VID 0x0000abba

/** \brief Product Code */
#define LCEC_TSD80E_PID 0x00000171

/** \brief Number of axes */
#define LCEC_TSD80E_AXIS 2

/** \brief Number of PDO */
#define LCEC_TSD80E_PDOS (16 * LCEC_TSD80E_AXIS)

int lcec_tsd80e_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#endif /* _LCEC_TSD80E_H_ */
