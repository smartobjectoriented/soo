/********************************************************************
 *  Copyright (C) 2012 Sascha Ittner <sascha.ittner@modusoft.de>
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

#ifndef _LCEC_PRIV_H_
#define _LCEC_PRIV_H_

/* LinuxCNC */
#include <opencn/hal/hal.h>

#include <opencn/uapi/lcec_conf.h>

#define LCEC_MSG_PFX "LCEC: "

/* vendor ids */
#define LCEC_BECKHOFF_VID 0x00000002

/* IDN builder */
#define LCEC_IDN_TYPE_P 0x8000
#define LCEC_IDN_TYPE_S 0x0000

#define LCEC_IDN(type, set, block) (type | ((set & 0x07) << 12) | (block & 0x0fff))

struct lcec_master;
struct lcec_slave;

/* pdo macros */
#define LCEC_PDO_INIT(pdo, pos, vid, pid, idx, sidx, off, bpos)  \
	do {                                                         \
		pdo->position     = pos;                                 \
		pdo->vendor_id    = vid;                                 \
		pdo->product_code = pid;                                 \
		pdo->index        = idx;                                 \
		pdo->subindex     = sidx;                                \
		pdo->offset       = off;                                 \
		pdo->bit_position = bpos;                                \
		pdo++;                                                   \
	} while (0);

typedef struct {
	hal_type_t    type;
	hal_pin_dir_t dir;
	int           offset;
	const char *  fmt;
} lcec_pindesc_t;

struct clock_correction {
	int64_t limit_ns;
	uint64_t period_ns;
	double isum_ns;
	int64_t out_ns;

	double pll_p;
 	double pll_i;
	double periodfp;
};

extern char lcec_module_name[];
extern lcec_master_t *first_master;

extern int lcec_read_idn(struct lcec_slave *slave, uint8_t drive_no, uint16_t idn, uint8_t *target, size_t size);

extern int lcec_pin_newf_list(void *base, const lcec_pindesc_t *list, ...);
extern int lcec_pin_newf(hal_type_t type, hal_pin_dir_t dir, void **data_ptr_addr, const char *fmt, ...);

extern int lcec_param_newf_list(void *base, const lcec_pindesc_t *list, ...);

extern int lcec_build_config(lcec_master_t *usr_cfg);
extern void lcec_clear_config(void);
extern void lcec_print_cfg(void);

#endif /* _LCEC_PRIV_H_ */
