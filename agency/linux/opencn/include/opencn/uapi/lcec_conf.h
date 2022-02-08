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

#ifndef UAPI_LCEC_CONF_H_
#define UAPI_LCEC_CONF_H_

#ifdef __KERNEL__
#include <linux/types.h>
#include <opencn/hal/hal.h>
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#include <opencn/uapi/hal.h>
#include <opencn/uapi/ecrt.h>

#define LCEC_CONF_STR_MAXLEN 48

/* list macros */
#define LCEC_LIST_APPEND(first, last, item)  \
	do {                                     \
        (item)->prev = (last);               \
		if ((item)->prev != NULL) {          \
			(item)->prev->next = (item);     \
		} else {                             \
			(first) = (item);                \
		}                                    \
		(last) = (item);                     \
		(item)->next = NULL;                 \
	} while (0);


struct lcec_master;
struct lcec_slave;

typedef void (*lcec_slave_rw_t)(struct lcec_slave *slave, long period);
typedef void (*lcec_slave_cleanup_t)(struct lcec_slave *slave);
typedef int (*lcec_slave_init_t)(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

typedef enum {
	lcecConfTypeNone = 0,
	lcecConfTypeMasters,
	lcecConfTypeMaster,
	lcecConfTypeSlave,
	lcecConfTypeDcConf,
	lcecConfTypeSdoConfig,
	lcecConfTypeIdnConfig,
	lcecConfTypeInitCmds
} lcec_conf_type_t;

typedef enum {
	lcecSlaveTypeInvalid,
	lcecSlaveTypeEK1100,
	lcecSlaveTypeEL1252,
	lcecSlaveTypeEL1002,
	lcecSlaveTypeEL1004,
	lcecSlaveTypeEL1008,
	lcecSlaveTypeEL1012,
	lcecSlaveTypeEL1014,
	lcecSlaveTypeEL1018,
	lcecSlaveTypeEL1024,
	lcecSlaveTypeEL1034,
	lcecSlaveTypeEL1084,
	lcecSlaveTypeEL1088,
	lcecSlaveTypeEL1094,
	lcecSlaveTypeEL1098,
	lcecSlaveTypeEL1104,
	lcecSlaveTypeEL1114,
	lcecSlaveTypeEL1124,
	lcecSlaveTypeEL1134,
	lcecSlaveTypeEL1144,
	lcecSlaveTypeEL1808,
	lcecSlaveTypeEL1809,
	lcecSlaveTypeEL1819,
	lcecSlaveTypeEL2252,
	lcecSlaveTypeTSD80E,
	lcecSlaveTypeAX5100,
	lcecSlaveTypeAX5200,
	lcecSlaveTypeCompanion,
	lcecSlaveTypeEL7411,
	lcecSlaveTypeEL2002,
	lcecSlaveTypeEL2004,
	lcecSlaveTypeEL2008,
	lcecSlaveTypeEL2022,
	lcecSlaveTypeEL2024,
	lcecSlaveTypeEL2032,
	lcecSlaveTypeEL2034,
	lcecSlaveTypeEL2042,
	lcecSlaveTypeEL2084,
	lcecSlaveTypeEL2088,
	lcecSlaveTypeEL2124,
	lcecSlaveTypeEL2612,
	lcecSlaveTypeEL2622,
	lcecSlaveTypeEL2634,
	lcecSlaveTypeEL2808,
	lcecSlaveTypeEL2798,
	lcecSlaveTypeEL2809,
	lcecSlaveTypeEP2008,
	lcecSlaveTypeEP2028,
	lcecSlaveTypeEP2809,
} lcec_slave_type_t;

typedef struct {
	uint16_t assignActivate;
	uint32_t sync0Cycle;
	int32_t  sync0Shift;
	uint32_t sync1Cycle;
	int32_t  sync1Shift;
} lcec_slave_dc_t;

typedef struct lcec_slave_state {
	hal_bit_t *online;
	hal_bit_t *operational;
	hal_bit_t *state_init;
	hal_bit_t *state_preop;
	hal_bit_t *state_safeop;
	hal_bit_t *state_op;
} lcec_slave_state_t;

typedef struct lcec_slave_sdoconf {
	struct lcec_slave_sdoconf *prev;
	struct lcec_slave_sdoconf *next;
	uint16_t index;
	int16_t  subindex;
	size_t   length;
	uint8_t *data;
} lcec_slave_sdoconf_t;

typedef struct lcec_slave_idnconf {
	struct lcec_slave_idnconf *prev;
	struct lcec_slave_idnconf *next;
	uint8_t drive;
	uint16_t idn;
	ec_al_state_t state;
	size_t length;
	uint8_t *data;
} lcec_slave_idnconf_t;

typedef struct lcec_slave {
	lcec_slave_type_t       type;
	char                    type_name[LCEC_CONF_STR_MAXLEN];
	struct lcec_slave 	*prev;
	struct lcec_slave 	*next;
	struct lcec_master 	*master;
	int                     index;
	char                    name[LCEC_CONF_STR_MAXLEN];
	uint32_t                vid;
	uint32_t                pid;
	int                     pdo_entry_count;
	ec_sync_info_t 		*sync_info;
	ec_slave_config_t 	*config;
	ec_slave_config_state_t state;
	lcec_slave_dc_t 	*dc_conf;
	lcec_slave_cleanup_t    proc_cleanup;
	lcec_slave_init_t       proc_init;
	lcec_slave_rw_t         proc_read;
	lcec_slave_rw_t         proc_write;
	lcec_slave_state_t 	*hal_state_data;
	void 			*hal_data;
	lcec_slave_sdoconf_t   *sdo_config;
	lcec_slave_idnconf_t   *idn_config;
} lcec_slave_t;

typedef struct lcec_master_data {
	hal_u32_t *slaves_responding;
	hal_bit_t *state_init;
	hal_bit_t *state_preop;
	hal_bit_t *state_safeop;
	hal_bit_t *state_op;
	hal_bit_t *link_up;
	hal_bit_t *all_op;
	hal_s32_t * pll_err;
	hal_s32_t * pll_out;
	hal_float_t pll_p;
	hal_float_t pll_i;
} lcec_master_data_t;

typedef struct lcec_master {
	struct lcec_master *prev;
	struct lcec_master *next;
	int                 index;
	char                name[LCEC_CONF_STR_MAXLEN];
	ec_master_t 	    *master;
	unsigned long       mutex;
	int                 pdo_entry_count;
	ec_pdo_entry_reg_t  *pdo_entry_regs;
	ec_domain_t 	    *domain;
	uint8_t 	    *process_data;
	int                 process_data_len;
	struct lcec_slave   *first_slave;
	struct lcec_slave   *last_slave;
	lcec_master_data_t  *hal_data;
	uint64_t            app_time_base;
	uint32_t            app_time_period;
	long                period_last;
	long long           state_update_timer;
	ec_master_state_t   ms;
	double   periodfp;
	uint64_t dc_ref;
	uint32_t dc_time_last;
	uint32_t app_time_last;
	int32_t  pll_limit;
	double   pll_isum;
	int debug;
} lcec_master_t;

#endif /* UAPI_LCEC_CONF_H_ */
