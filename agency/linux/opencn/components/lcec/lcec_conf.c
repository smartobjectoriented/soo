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

#include <linux/slab.h>

#include <opencn/uapi/lcec_conf.h>

#include "lcec_priv.h"
#include "lcec_ek1100.h"
#include "lcec_el1252.h"
#include "lcec_el1xxx.h"
#include "lcec_el2252.h"
#include "lcec_el2xxx.h"
#include "lcec_tsd80e.h"
#include "lcec_ax5100.h"
#include "lcec_ax5200.h"
#include "lcec_el7411.h"
#include "lcec_companion.h"

typedef struct lcec_typelist {
	lcec_slave_type_t type;
	uint32_t          vid;
	uint32_t          pid;
	int               pdo_entry_count;
	lcec_slave_init_t proc_init;
} lcec_typelist_t;

static const lcec_typelist_t types[] = {
	/* bus coupler */
	{lcecSlaveTypeEK1100, LCEC_EK1100_VID, LCEC_EK1100_PID, LCEC_EK1100_PDOS, NULL},

	/* digital in */
	{lcecSlaveTypeEL1252, LCEC_EL1252_VID, LCEC_EL1252_PID, LCEC_EL1252_PDOS, lcec_el1252_init}, // 2 fast channels with timestamp

	{lcecSlaveTypeEL1002, LCEC_EL1xxx_VID, LCEC_EL1002_PID, LCEC_EL1002_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1004, LCEC_EL1xxx_VID, LCEC_EL1004_PID, LCEC_EL1004_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1008, LCEC_EL1xxx_VID, LCEC_EL1008_PID, LCEC_EL1008_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1012, LCEC_EL1xxx_VID, LCEC_EL1012_PID, LCEC_EL1012_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1014, LCEC_EL1xxx_VID, LCEC_EL1014_PID, LCEC_EL1014_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1018, LCEC_EL1xxx_VID, LCEC_EL1018_PID, LCEC_EL1018_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1024, LCEC_EL1xxx_VID, LCEC_EL1024_PID, LCEC_EL1024_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1034, LCEC_EL1xxx_VID, LCEC_EL1034_PID, LCEC_EL1034_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1084, LCEC_EL1xxx_VID, LCEC_EL1084_PID, LCEC_EL1084_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1088, LCEC_EL1xxx_VID, LCEC_EL1088_PID, LCEC_EL1088_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1094, LCEC_EL1xxx_VID, LCEC_EL1094_PID, LCEC_EL1094_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1098, LCEC_EL1xxx_VID, LCEC_EL1098_PID, LCEC_EL1098_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1104, LCEC_EL1xxx_VID, LCEC_EL1104_PID, LCEC_EL1104_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1114, LCEC_EL1xxx_VID, LCEC_EL1114_PID, LCEC_EL1114_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1124, LCEC_EL1xxx_VID, LCEC_EL1124_PID, LCEC_EL1124_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1134, LCEC_EL1xxx_VID, LCEC_EL1134_PID, LCEC_EL1134_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1144, LCEC_EL1xxx_VID, LCEC_EL1144_PID, LCEC_EL1144_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1808, LCEC_EL1xxx_VID, LCEC_EL1808_PID, LCEC_EL1808_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1809, LCEC_EL1xxx_VID, LCEC_EL1809_PID, LCEC_EL1809_PDOS, lcec_el1xxx_init},
	{lcecSlaveTypeEL1819, LCEC_EL1xxx_VID, LCEC_EL1819_PID, LCEC_EL1819_PDOS, lcec_el1xxx_init},


	/* digital out */
	{lcecSlaveTypeEL2252, LCEC_EL2252_VID, LCEC_EL2252_PID, LCEC_EL2252_PDOS, lcec_el2252_init},

	{lcecSlaveTypeEL2002, LCEC_EL2xxx_VID, LCEC_EL2002_PID, LCEC_EL2002_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2004, LCEC_EL2xxx_VID, LCEC_EL2004_PID, LCEC_EL2004_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2008, LCEC_EL2xxx_VID, LCEC_EL2008_PID, LCEC_EL2008_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2022, LCEC_EL2xxx_VID, LCEC_EL2022_PID, LCEC_EL2022_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2024, LCEC_EL2xxx_VID, LCEC_EL2024_PID, LCEC_EL2024_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2032, LCEC_EL2xxx_VID, LCEC_EL2032_PID, LCEC_EL2032_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2034, LCEC_EL2xxx_VID, LCEC_EL2034_PID, LCEC_EL2034_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2042, LCEC_EL2xxx_VID, LCEC_EL2042_PID, LCEC_EL2042_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2084, LCEC_EL2xxx_VID, LCEC_EL2084_PID, LCEC_EL2084_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2088, LCEC_EL2xxx_VID, LCEC_EL2088_PID, LCEC_EL2088_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2124, LCEC_EL2xxx_VID, LCEC_EL2124_PID, LCEC_EL2124_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2612, LCEC_EL2xxx_VID, LCEC_EL2612_PID, LCEC_EL2612_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2622, LCEC_EL2xxx_VID, LCEC_EL2622_PID, LCEC_EL2622_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2634, LCEC_EL2xxx_VID, LCEC_EL2634_PID, LCEC_EL2634_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2808, LCEC_EL2xxx_VID, LCEC_EL2808_PID, LCEC_EL2808_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2798, LCEC_EL2xxx_VID, LCEC_EL2798_PID, LCEC_EL2798_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEL2809, LCEC_EL2xxx_VID, LCEC_EL2809_PID, LCEC_EL2809_PDOS, lcec_el2xxx_init},

	{lcecSlaveTypeEP2008, LCEC_EL2xxx_VID, LCEC_EP2008_PID, LCEC_EP2008_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEP2028, LCEC_EL2xxx_VID, LCEC_EP2028_PID, LCEC_EP2028_PDOS, lcec_el2xxx_init},
	{lcecSlaveTypeEP2809, LCEC_EL2xxx_VID, LCEC_EP2809_PID, LCEC_EP2809_PDOS, lcec_el2xxx_init},

	/* Triamec drives */
	{lcecSlaveTypeTSD80E, LCEC_TSD80E_VID, LCEC_TSD80E_PID, LCEC_TSD80E_PDOS, lcec_tsd80e_init},

	/* Companion slave */
	{lcecSlaveTypeCompanion, LCEC_COMPANION_VID, LCEC_COMPANION_PID, LCEC_COMPANION_PDOS, lcec_companion_init},

	/* Beckhoff drives */
	{lcecSlaveTypeAX5100, LCEC_AX5100_VID, LCEC_AX5100_PID, LCEC_AX5100_PDOS, lcec_ax5100_init},
	{lcecSlaveTypeAX5200, LCEC_AX5200_VID, LCEC_AX5200_PID, LCEC_AX5200_PDOS, lcec_ax5200_init},
	{lcecSlaveTypeEL7411, LCEC_EL7411_VID, LCEC_EL7411_PID, LCEC_EL7411_PDOS, lcec_el7411_init},

	{lcecSlaveTypeInvalid}
};

lcec_master_t *first_master = NULL;
static lcec_master_t *last_master  = NULL;

void lcec_clear_config(void)
{
	lcec_master_t *master;
	lcec_slave_t *slave;
	lcec_slave_sdoconf_t *sdo;

	for (master = first_master; master != NULL; master = master->next) {
		for (slave = master->first_slave; slave != NULL; slave = slave->next) {
			if (slave->dc_conf) {
				kfree(slave->dc_conf);
			}

			if (slave->sdo_config) {
				for (sdo = slave->sdo_config; sdo != NULL; sdo = sdo->next) {
					kfree(sdo->data);
					kfree(sdo);
				}
			}
			kfree(slave);
		}

		kfree(master->pdo_entry_regs);
		kfree(master);
	}
}

int lcec_build_config(lcec_master_t *usr_cfg)
{
	int                    slave_count;
	const lcec_typelist_t *type;
	lcec_master_t         *usr_master;
	lcec_master_t         *master;
	lcec_slave_t          *usr_slave;
	lcec_slave_t          *slave;
	lcec_slave_dc_t       *dc;
	lcec_slave_sdoconf_t  *first_sdo;
	lcec_slave_sdoconf_t  *last_sdo;
	lcec_slave_sdoconf_t  *sdo;
	lcec_slave_sdoconf_t  *usr_sdo;
	lcec_slave_idnconf_t  *first_idn;
	lcec_slave_idnconf_t  *last_idn;
	lcec_slave_idnconf_t  *idn;
	lcec_slave_idnconf_t  *usr_idn;
	ec_pdo_entry_reg_t    *pdo_entry_regs;

	/* initialize list */
	first_master = NULL;
	last_master  = NULL;

	/* process config items */
	slave_count = 0;
	master      = NULL;
	slave       = NULL;

	for (usr_master = usr_cfg; usr_master != NULL; usr_master = usr_master->next) {

		/* Create a "kernel" master */
		master = kzalloc(sizeof(lcec_master_t), GFP_ATOMIC);
		if (master == NULL) {
			rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate master %d structure memory\n", usr_master->index);
			goto fail;
		}

		/* Copy info received from user space */
		master->index           = usr_master->index;
		master->app_time_period = usr_master->app_time_period;
		strncpy(master->name, usr_master->name, LCEC_CONF_STR_MAXLEN);
		master->name[LCEC_CONF_STR_MAXLEN - 1] = 0;

		/* add master to list */
		LCEC_LIST_APPEND(first_master, last_master, master);

		for (usr_slave = usr_master->first_slave; usr_slave != NULL; usr_slave = usr_slave->next) {

			/* create new slave */
			slave = kzalloc(sizeof(lcec_slave_t), GFP_ATOMIC);
			if (slave == NULL) {
				rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate slave %s.%s structure memory\n", master->name,
								usr_slave->name);
				goto fail;
			}

			/* Copy info received from user space */
			slave->index = usr_slave->index;
			slave->type = usr_slave->type;
			strncpy(slave->type_name, usr_slave->type_name, LCEC_CONF_STR_MAXLEN);
			slave->type_name[LCEC_CONF_STR_MAXLEN - 1] = 0;
			strncpy(slave->name, usr_slave->name, LCEC_CONF_STR_MAXLEN);
			slave->name[LCEC_CONF_STR_MAXLEN - 1] = 0;

			slave->master = master;

			for (type = types; type->type != lcecSlaveTypeInvalid; type++) {
				if (type->type == slave->type) {
					slave->vid = type->vid;
					slave->pid = type->pid;

					slave->pdo_entry_count = type->pdo_entry_count;
					slave->proc_init       = type->proc_init;

					master->pdo_entry_count += slave->pdo_entry_count;
					break;
				}
			}

			slave->dc_conf    = NULL;
			slave->sdo_config = NULL;

			/* add slave to list */
			LCEC_LIST_APPEND(master->first_slave, master->last_slave, slave);

			slave_count++;

			if (usr_slave->dc_conf) {

				/* create new dc config */
				dc = kzalloc(sizeof(lcec_slave_dc_t), GFP_ATOMIC);
				if (dc == NULL) {
					rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate slave %s.%s dc config memory\n", master->name, slave->name);
					goto fail;
				}

				/* initialize dc conf */
				dc->assignActivate = usr_slave->dc_conf->assignActivate;
				dc->sync0Cycle     = usr_slave->dc_conf->sync0Cycle;
				dc->sync0Shift     = usr_slave->dc_conf->sync0Shift;
				dc->sync1Cycle     = usr_slave->dc_conf->sync1Cycle;
				dc->sync1Shift     = usr_slave->dc_conf->sync1Shift;

				/* add to slave */
				slave->dc_conf = dc;
			}

			if (usr_slave->sdo_config) {
				first_sdo = NULL;
				last_sdo  = NULL;

				for (usr_sdo = usr_slave->sdo_config; usr_sdo != NULL; usr_sdo = usr_sdo->next) {

					sdo = kzalloc(sizeof(lcec_slave_sdoconf_t), GFP_ATOMIC);
					if (sdo == NULL) {
						rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate slave %s.%s sdo config memory\n", master->name, slave->name);
						goto fail;
					}

					/* initialize sdo conf */
					sdo->index    = usr_sdo->index;
					sdo->subindex = usr_sdo->subindex;
					sdo->length   = usr_sdo->length;

#if 0 /* debug message */
					rtapi_print_msg(RTAPI_MSG_DBG, "[LCEC] SDO: index = %x, subindex = %d, length = %d\n",
							sdo->index, sdo->subindex, sdo->length);
#endif

					sdo->data = kzalloc(sizeof(uint8_t)*sdo->length, GFP_ATOMIC);
					memcpy(sdo->data, usr_sdo->data, sdo->length);

					/* add sdo to list */
					LCEC_LIST_APPEND(first_sdo, last_sdo, sdo);
				}

				/* add to slave */
				slave->sdo_config = first_sdo;
			}

			if (usr_slave->idn_config) {
				first_idn = NULL;
				last_idn  = NULL;

				for (usr_idn = usr_slave->idn_config; usr_idn != NULL; usr_idn = usr_idn->next) {

					idn = kzalloc(sizeof(lcec_slave_idnconf_t), GFP_ATOMIC);
					if (idn == NULL) {
						rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate slave %s.%s idn config memory\n", master->name, slave->name);
						goto fail;
					}

					/* initialize idn conf */
					idn->drive  = usr_idn->drive;
					idn->idn    = usr_idn->idn;
					idn->state  = usr_idn->state;
					idn->length = usr_idn->length;

					idn->data = kzalloc(sizeof(uint8_t)*idn->length, GFP_ATOMIC);
					memcpy(idn->data, usr_idn->data, idn->length);

					/* add sdo to list */
					LCEC_LIST_APPEND(first_idn, last_idn, idn);
				}

				/* add to slave */
				slave->idn_config = first_idn;
			}
		}

		/* allocate PDO entity memory */
		pdo_entry_regs = kzalloc(sizeof(ec_pdo_entry_reg_t)*(master->pdo_entry_count + 1), GFP_ATOMIC);
		if (pdo_entry_regs == NULL) {
			rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "Unable to allocate master %s PDO entry memory\n", master->name);
			goto fail;
		}
		master->pdo_entry_regs = pdo_entry_regs;
	}

	return slave_count;

fail:
	lcec_clear_config();
	return -1;
}

void lcec_print_cfg(void)
{
	lcec_master_t *master;
	lcec_slave_t *slave;
	int i;
	lcec_slave_sdoconf_t *sdo;
	lcec_slave_idnconf_t *idn;

	printk("== EtherCAT configuration:\n");
	for (master = first_master; master != NULL; master = master->next) {
		printk("Master #%d\n", master->index);
		printk("  name: %s\n", master->name);
		printk("  app_time_period: %d\n", master->app_time_period);
		for (slave = master->first_slave; slave != NULL; slave = slave->next) {
			printk("  slave #%d\n", slave->index);
			printk("    type: %d\n", slave->type);
			printk("    type name: %s\n", slave->type_name);
			printk("    name: %s\n", slave->name);
			if (slave->dc_conf) {
				printk("    dc, assignActivate: %d\n", slave->dc_conf->assignActivate);
				printk("    dc, sync0Cycle: %d\n", slave->dc_conf->sync0Cycle);
				printk("    dc, sync0Shift: %d\n", slave->dc_conf->sync0Shift);
				printk("    dc, sync1Cycle: %d\n", slave->dc_conf->sync1Cycle);
				printk("    dc, sync1Shift: %d\n", slave->dc_conf->sync1Shift);
			} else {
				printk("    no dc config\n");
			}

			if (slave->sdo_config) {
				for (sdo = slave->sdo_config; sdo != NULL; sdo = sdo->next) {
					printk("    sdo, index: %d\n", sdo->index);
					printk("    sdo, subindex: %d\n", sdo->subindex);
					printk("    sdo, length: %ld\n", (long int)sdo->length);
					printk("    sdo, data: ");
					for (i=0; i<sdo->length; i++)
						printk(KERN_CONT "%02x", sdo->data[i]);
					printk("\n\n");
				}
			} else {
				printk("    no sdo config\n");
			}

			if (slave->idn_config) {
				for (idn = slave->idn_config; idn != NULL; idn = idn->next) {
					printk("    idn,  drive: %d\n", idn->drive);
					printk("    idn,  idn: %d\n", idn->idn);
					printk("    idn,  state: %d\n", idn->state);
					printk("    idn,  length: %ld\n", (long int) idn->length);
					printk("    idn,  data: ");
					for (i=0; i<idn->length; i++)
						printk(KERN_CONT "%02x", idn->data[i]);
					printk("\n\n");
				}
			} else {
				printk("    no idn config\n");
			}
		}
	}
}
