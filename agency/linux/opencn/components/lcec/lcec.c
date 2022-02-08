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

#include <linux/fs.h>
#include <linux/slab.h>

#ifdef CONFIG_ARM
#include <asm/neon.h>
#endif

/* Xenomai */
#include <xenomai/rtdm/driver.h>

#include <soo/uapi/avz.h>

#include <opencn/rtapi/rtapi_mutex.h>

#include <opencn/uapi/lcec.h>
#include <opencn/uapi/lcec_conf.h>

#include <soo/uapi/console.h>

#include "lcec_priv.h"

/* Enable this macro to print EtherCAT configuration */
#if 0
#define PRINT_CONFIG
#endif

#define LCEC_CONF_SDO_COMPLETE_SUBIDX     -1


void ecrt_ioctl_soe_rt_read(dc_event_t dc_event);

typedef enum {
	LCEC_STARTUP_ERR_NO_ERROR = 0,
	LCEC_STARTUP_ERR_MASTER_REQ,
	LCEC_STARTUP_ERR_DOMAIN_CREATION,
	LCEC_STARTUP_ERR_SLAVE_CFG,
	LCEC_STARTUP_ERR_SDO_CFG,
	LCEC_STARTUP_ERR_PROC_INIT,
	LCEC_STARTUP_ERR_PDO_CFG,
	LCEC_STARTUP_ERR_PIN_EXPORT,
	LCEC_STARTUP_ERR_REG_PDO,
	LCEC_STARTUP_ERR_MASTER_ACTIVATE,
	LCEC_STARTUP_ERR_INIT_MASTER_HAL,
} lcec_startup_error_t;

static volatile lcec_startup_error_t lcec_startup_error;

static int comp_id;						/* component ID */
char lcec_module_name[LCEC_CONF_STR_MAXLEN];

static const lcec_pindesc_t master_global_pins[] = {
	{HAL_U32, HAL_OUT, offsetof(lcec_master_data_t, slaves_responding), "%s.slaves-responding"},
	{HAL_BIT, HAL_OUT, offsetof(lcec_master_data_t, state_init), "%s.state-init"},
	{HAL_BIT, HAL_OUT, offsetof(lcec_master_data_t, state_preop), "%s.state-preop"},
	{HAL_BIT, HAL_OUT, offsetof(lcec_master_data_t, state_safeop), "%s.state-safeop"},
	{HAL_BIT, HAL_OUT, offsetof(lcec_master_data_t, state_op), "%s.state-op"},
	{HAL_BIT, HAL_OUT, offsetof(lcec_master_data_t, link_up), "%s.link-up"},
	{HAL_BIT, HAL_OUT, offsetof(lcec_master_data_t, all_op), "%s.all-op"},
	{HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL}
};

static const lcec_pindesc_t master_pins[] = {
	{HAL_S32, HAL_OUT, offsetof(lcec_master_data_t, pll_err), "%s.pll-err"},
	{HAL_S32, HAL_OUT, offsetof(lcec_master_data_t, pll_out), "%s.pll-out"},
	{HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL}
};

static const lcec_pindesc_t master_params[] = {
	{HAL_FLOAT, HAL_RW, offsetof(lcec_master_data_t, pll_p), "%s.pll-p"},
	{HAL_FLOAT, HAL_RW, offsetof(lcec_master_data_t, pll_i), "%s.pll-i"},
	{HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL}
};

static const lcec_pindesc_t slave_pins[] = {
	{HAL_BIT, HAL_OUT, offsetof(lcec_slave_state_t, online), "%s.%s.%s.online"},
	{HAL_BIT, HAL_OUT, offsetof(lcec_slave_state_t, operational), "%s.%s.%s.operational"},
	{HAL_BIT, HAL_OUT, offsetof(lcec_slave_state_t, state_init), "%s.%s.%s.state-init"},
	{HAL_BIT, HAL_OUT, offsetof(lcec_slave_state_t, state_preop), "%s.%s.%s.state-preop"},
	{HAL_BIT, HAL_OUT, offsetof(lcec_slave_state_t, state_safeop), "%s.%s.%s.state-safeop"},
	{HAL_BIT, HAL_OUT, offsetof(lcec_slave_state_t, state_op), "%s.%s.%s.state-op"},
	{HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL}
};

static int            comp_id      = -1;

#if 0
static lcec_master_data_t *global_hal_data;
static ec_master_state_t   global_ms;
#endif

static void lcec_request_lock(void *data)
{
	lcec_master_t *master = (lcec_master_t *)data;
	rtapi_mutex_get(&master->mutex);
}

static void lcec_release_lock(void *data)
{
	lcec_master_t *master = (lcec_master_t *)data;
	rtapi_mutex_give(&master->mutex);
}

int lcec_read_idn(struct lcec_slave *slave, uint8_t drive_no, uint16_t idn, uint8_t *target, size_t size)
{
	lcec_master_t *master = slave->master;
	int err;
	size_t result_size;
	uint16_t error_code;

	if ((err = ecrt_master_read_idn(master->master, slave->index, drive_no, idn, target, size, &result_size, &error_code))) {
		rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "slave %s.%s: Failed to execute IDN read (drive %u idn %c-%u-%u, error %d, error_code %08x)\n",
		     master->name, slave->name, drive_no, (idn & 0x8000) ? 'P' : 'S', (idn >> 12) & 0x0007, idn & 0x0fff, err, error_code);
		return -1;
	}

	if (result_size != size) {
		rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "slave %s.%s: Invalid result size on IDN read (drive %u idn %c-%d-%d, req: %u, res: %u)\n",
		         master->name, slave->name, drive_no, (idn & 0x8000) ? 'P' : 'S', (idn >> 12) & 0x0007, idn & 0x0fff, (unsigned int) size, (unsigned int) result_size);
		return -1;
	}

	return 0;
}

static int lcec_pin_newfv(hal_type_t type, hal_pin_dir_t dir, void **data_ptr_addr, const char *fmt, va_list ap)
{
	char name[HAL_NAME_LEN + 1];
	int  sz;
	int  err;

	sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
	if (sz == -1 || sz > HAL_NAME_LEN) {
		rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "length %d too long for name starting '%s'\n", sz, name);
		return -ENOMEM;
	}

	err = hal_pin_new(__core_hal_user, name, type, dir, data_ptr_addr, comp_id);
	if (err) {
		rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "exporting pin %s failed\n", name);
		return err;
	}

	switch (type) {
	case HAL_BIT:
		**((hal_bit_t **)data_ptr_addr) = 0;
		break;
	case HAL_FLOAT:
		**((hal_float_t **)data_ptr_addr) = 0.0;
		break;
	case HAL_S32:
		**((hal_s32_t **)data_ptr_addr) = 0;
		break;
	case HAL_U32:
		**((hal_u32_t **)data_ptr_addr) = 0;
		break;
	default:
		break;
	}

	return 0;
}

int lcec_pin_newf(hal_type_t type, hal_pin_dir_t dir, void **data_ptr_addr, const char *fmt, ...) {
	va_list ap;
	int     err;

	va_start(ap, fmt);
	err = lcec_pin_newfv(type, dir, data_ptr_addr, fmt, ap);
	va_end(ap);

	return err;
}

static int lcec_pin_newfv_list(void *base, const lcec_pindesc_t *list, va_list ap)
{
	va_list               ac;
	int                   err;
	const lcec_pindesc_t *p;

	for (p = list; p->type != HAL_TYPE_UNSPECIFIED; p++) {
		va_copy(ac, ap);
		err = lcec_pin_newfv(p->type, p->dir, (void **)(base + p->offset), p->fmt, ac);
		va_end(ac);
		if (err) {
			return err;
		}
	}

	return 0;
}

int lcec_pin_newf_list(void *base, const lcec_pindesc_t *list, ...)
{
	va_list ap;
	int     err;

	va_start(ap, list);
	err = lcec_pin_newfv_list(base, list, ap);
	va_end(ap);

	return err;
}

static int lcec_param_newfv(hal_type_t type, hal_pin_dir_t dir, void *data_addr, const char *fmt, va_list ap)
{
	char name[HAL_NAME_LEN + 1];
	int  sz;
	int  err;

	sz = rtapi_vsnprintf(name, sizeof(name), fmt, ap);
	if (sz == -1 || sz > HAL_NAME_LEN) {
		rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "length %d too long for name starting '%s'\n", sz, name);
		return -ENOMEM;
	}

	err = hal_param_new(__core_hal_user, name, type, dir, data_addr, comp_id);
	if (err) {
		rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "exporting param %s failed\n", name);
		return err;
	}

	switch (type) {
	case HAL_BIT:
		*((hal_bit_t *)data_addr) = 0;
		break;
	case HAL_FLOAT:
		*((hal_float_t *)data_addr) = 0.0;
		break;
	case HAL_S32:
		*((hal_s32_t *)data_addr) = 0;
		break;
	case HAL_U32:
		*((hal_u32_t *)data_addr) = 0;
		break;
	default:
		break;
	}

	return 0;
}

static int lcec_param_newfv_list(void *base, const lcec_pindesc_t *list, va_list ap)
{
	va_list               ac;
	int                   err;
	const lcec_pindesc_t *p;

	for (p = list; p->type != HAL_TYPE_UNSPECIFIED; p++) {
		va_copy(ac, ap);
		err = lcec_param_newfv(p->type, p->dir, (void *)(base + p->offset), p->fmt, ac);
		va_end(ac);
		if (err) {
			return err;
		}
	}

	return 0;
}

int lcec_param_newf_list(void *base, const lcec_pindesc_t *list, ...)
{
	va_list ap;
	int     err;

	va_start(ap, list);
	err = lcec_param_newfv_list(base, list, ap);
	va_end(ap);

	return err;
}


static lcec_slave_state_t *lcec_init_slave_state_hal(char *master_name, char *slave_name)
{
	lcec_slave_state_t *hal_data;

	/* alloc hal data */
	if ((hal_data = (lcec_slave_state_t *)hal_malloc(__core_hal_user, sizeof(lcec_slave_state_t))) == NULL) {
		rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for %s.%s.%s failed\n", lcec_module_name, master_name, slave_name);
		return NULL;
	}
	memset(hal_data, 0, sizeof(lcec_slave_state_t));

	/* export pins */
	if (lcec_pin_newf_list(hal_data, slave_pins, lcec_module_name, master_name, slave_name) != 0) {
		return NULL;
	}

	return hal_data;
}


static lcec_master_data_t *lcec_init_master_hal(const char *pfx, int global)
{
	lcec_master_data_t *hal_data;

	/* alloc hal data */
	if ((hal_data = hal_malloc(__core_hal_user, sizeof(lcec_master_data_t))) == NULL) {
		rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for %s failed\n", pfx);
		return NULL;
	}
	memset(hal_data, 0, sizeof(lcec_master_data_t));

	/* export pins */
	if (lcec_pin_newf_list(hal_data, master_global_pins, pfx) != 0)
		return NULL;

#if 0 /* need to be clean-up & checked */
	if (!global) {
		if (lcec_pin_newf_list(hal_data, master_pins, pfx) != 0)
			return NULL;
		if (lcec_param_newf_list(hal_data, master_params, pfx) != 0)
			return NULL;
	}
#endif

	return hal_data;
}

static void lcec_update_master_hal(lcec_master_data_t *hal_data, ec_master_state_t *ms)
{
	*(hal_data->slaves_responding) = ms->slaves_responding;
	*(hal_data->state_init)        = (ms->al_states & 0x01) != 0;
	*(hal_data->state_preop)       = (ms->al_states & 0x02) != 0;
	*(hal_data->state_safeop)      = (ms->al_states & 0x04) != 0;
	*(hal_data->state_op)          = (ms->al_states & 0x08) != 0;
	*(hal_data->link_up)           = ms->link_up;
	*(hal_data->all_op)            = (ms->al_states == 0x08);
}

static void lcec_update_slave_state_hal(lcec_slave_state_t *hal_data, ec_slave_config_state_t *ss, int debug)
{
	if (debug) {
		*(hal_data->online)       = 1;
		*(hal_data->operational)  = 1;
		*(hal_data->state_init)   = 0;
		*(hal_data->state_preop)  = 0;
		*(hal_data->state_safeop) = 0;
		*(hal_data->state_op)     = 1;

	} else {
		*(hal_data->online)       = ss->online;
		*(hal_data->operational)  = ss->operational;
		*(hal_data->state_init)   = (ss->al_state & 0x01) != 0;
		*(hal_data->state_preop)  = (ss->al_state & 0x02) != 0;
		*(hal_data->state_safeop) = (ss->al_state & 0x04) != 0;
		*(hal_data->state_op)     = (ss->al_state & 0x08) != 0;
	}
}

/************************************************************************
 *                    EtherCAT cycle task                               *
 ************************************************************************/

extern void clock_correction_init_fp(struct clock_correction* corr, uint64_t task_period_ns);

extern int64_t clock_correction_compute(struct clock_correction *corr,
								 uint32_t last_app_time,
								 uint32_t dc_time, uint64_t period_limit);

static void clock_correction_init(struct clock_correction* corr, uint64_t task_period_ns)
{
#ifdef CONFIG_ARM
	kernel_neon_begin();
#endif

	clock_correction_init_fp(corr, task_period_ns);

#ifdef CONFIG_ARM
	kernel_neon_end();
#endif

}

static void lcec_cyclic_task(void *arg)
{
	hal_thread_t *thread = (hal_thread_t *)arg;

	lcec_master_t *master = (lcec_master_t *)thread->param;
	uint64_t period = (uint64_t)(master->app_time_period);
	uint64_t period_limit;
	lcec_slave_t *slave;

	bool started = false;
	bool dc_time_valid;
	int ret;
	uint64_t nextstart;
	uint32_t dc_time;
	uint64_t now;
	uint64_t app_time = 0ull;
	uint32_t last_app_time = 0u;
	int64_t clock_offset = 0ll;
	struct clock_correction corr;

	period_limit = period;
	do_div(period_limit, 100);


	clock_correction_init(&corr, period);

	master->app_time_base = rtapi_get_time();
	nextstart = master->app_time_base;

	thread->period = period;

	while (!rtdm_task_should_stop()) {


		now = rtapi_get_time();
		if (nextstart > now) {
			ret = rtdm_task_sleep_abs(nextstart, RTDM_TIMERMODE_ABSOLUTE);
			if (ret) {
				rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX
								"rtdm_task_sleep_until failed with error %d\n",
								ret);
#warning is it the correct action in case of errer ??
				break;
			}
		}

		if (!hal_is_threads_started(__core_hal_user)) {
			nextstart += period;
			continue;
		}

		/* receive process data & master state */
		if (!master->debug) {
			rtapi_mutex_get(&master->mutex);
			ecrt_master_receive(master->master);
			ecrt_domain_process(master->domain);
			rtapi_mutex_give(&master->mutex);
		}

		/* update state pins */
		lcec_update_master_hal(master->hal_data, &master->ms);

		/* process slaves */
#ifdef CONFIG_ARM
		kernel_neon_begin();
#endif

		for (slave = master->first_slave; slave != NULL; slave = slave->next) {

			/* Update slaves state
			 * In debug mode, there are no EtherCAT  --> don't call EtherCAT master functions
			 */
			if (!master->debug)
				ecrt_slave_config_state(slave->config, &slave->state);
			lcec_update_slave_state_hal(slave->hal_state_data, &slave->state, master->debug);
			
			/* process read/write functions */
			if (slave->proc_read != NULL)
				slave->proc_read(slave, period);
		}

		/* Execute external / added functions */
		hal_execute_thread_func(thread);

		for (slave = master->first_slave; slave != NULL; slave = slave->next) {
			if (slave->proc_write != NULL)
				slave->proc_write(slave, period);
		}

#ifdef CONFIG_ARM
		kernel_neon_end();
#endif

		/* send process data */
		if (!master->debug) {
			rtapi_mutex_get(&master->mutex);
			ecrt_domain_queue(master->domain);

			/* Set application time */
			now = rtapi_get_time();
			master->dc_ref += period;
			app_time = master->app_time_base + master->dc_ref + (now - nextstart);
			ecrt_master_application_time(master->master, app_time);

			/* sync master to ref clock */
			ret = ecrt_master_reference_clock_time(master->master, &dc_time);
			dc_time_valid = ret == 0;

			/* sync slaves to ref clock */
			ecrt_master_sync_slave_clocks(master->master);

			/* send domain data */
			ecrt_master_send(master->master);
			rtapi_mutex_give(&master->mutex);

			/* PI controller for master thread PLL sync
			   this part is done after ecrt_master_send() to reduce jitter */
			if (started) {
				if (dc_time_valid) {

#ifdef CONFIG_ARM
					kernel_neon_begin();
#endif
					clock_offset = clock_correction_compute(&corr,
									last_app_time,
									dc_time, period_limit);

#ifdef CONFIG_ARM
					kernel_neon_end();
#endif
				} else {
					clock_offset = 0ll;
				}
			} else {
				started = true;
			}

			last_app_time = (uint32_t)app_time;

		} else {
			clock_offset = 0;
		}

		nextstart += period + clock_offset;
	}
}

static int lcec_create_thread(lcec_master_t *master)
{
	int retval;
	char name[HAL_NAME_LEN + 1];

	rtapi_snprintf(name, sizeof(name), "lcec_thread.%d", master->index);

	retval = hal_create_custom_thread(__core_hal_user, name, 1, lcec_cyclic_task, master);
	return 0;
}

/* Startup of the lcec component in DEBUG mode
 * In debug mode, there are no connection with the EtherCAT master.
 * Only HAL objects (pins, parameters) are exported
 *
 * For slaves has to handle the pins behavior in this case
 */
void lcec_startup_debug_mode(void)
{
	lcec_master_t *master;
	lcec_slave_t  *slave;
	char  name[HAL_NAME_LEN + 1];
	ec_pdo_entry_reg_t *pdo_entry_regs;

	/* initialize masters */
	for (master = first_master; master != NULL; master = master->next) {
		master->debug = 1;

		/* initialize slaves */
		pdo_entry_regs = master->pdo_entry_regs;
		for (slave = master->first_slave; slave != NULL; slave = slave->next) {
			/* setup pdos */
			if (slave->proc_init != NULL) {
				if ((slave->proc_init(comp_id, slave, pdo_entry_regs)) != 0) {
					lcec_startup_error = LCEC_STARTUP_ERR_PROC_INIT;
					return;
				}
			}

			/* export state pins */
			if ((slave->hal_state_data = lcec_init_slave_state_hal(master->name, slave->name)) == NULL) {
				lcec_startup_error = LCEC_STARTUP_ERR_PIN_EXPORT;
				return;
			}
		}
		pdo_entry_regs += slave->pdo_entry_count;

		/* init hal data */
		rtapi_snprintf(name, HAL_NAME_LEN, "%s.%s", lcec_module_name, master->name);
		if ((master->hal_data = lcec_init_master_hal(name, 0)) == NULL) {
			lcec_startup_error =  LCEC_STARTUP_ERR_INIT_MASTER_HAL;
			return;
		}

		/* create "lcec thread" for this master */
		lcec_create_thread(master);
	}
}

static void lcec_startup(dc_event_t dc_event)
{
	lcec_master_t *master;
	lcec_slave_t *        slave;
	char                  name[HAL_NAME_LEN + 1];
	ec_pdo_entry_reg_t *  pdo_entry_regs;
	lcec_slave_sdoconf_t *sdo;
	lcec_slave_idnconf_t *idn;

	/* initialize masters */
	for (master = first_master; master != NULL; master = master->next) {
		master->debug = 0;

		/* request ethercat master */
		if (!(master->master = ecrt_request_master(master->index))) {
			rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "requesting master %s (index %d) failed\n", master->name, master->index);
			lcec_startup_error = LCEC_STARTUP_ERR_MASTER_REQ;
			goto lcec_startup_end;
		}

		/* register callbacks */
		ecrt_master_callbacks(master->master, lcec_request_lock, lcec_release_lock, master);

		/* create domain */
		if (!(master->domain = ecrt_master_create_domain(master->master))) {
			rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "master %s domain creation failed\n", master->name);
			lcec_startup_error = LCEC_STARTUP_ERR_DOMAIN_CREATION;
			goto lcec_startup_end;
		}

		/* initialize slaves */
		pdo_entry_regs = master->pdo_entry_regs;
		for (slave = master->first_slave; slave != NULL; slave = slave->next) {
			/* read slave config */
			if (!(slave->config = ecrt_master_slave_config(master->master, 0, slave->index, slave->vid, slave->pid))) {
				rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to read slave %s.%s configuration\n", master->name, slave->name);
				lcec_startup_error = LCEC_STARTUP_ERR_SLAVE_CFG;
				goto lcec_startup_end;
			}

			/* initialize sdos */
			if (slave->sdo_config != NULL) {
				for (sdo = slave->sdo_config; sdo != NULL; sdo = sdo->next) {
					if (sdo->subindex == LCEC_CONF_SDO_COMPLETE_SUBIDX) {
						if (ecrt_slave_config_complete_sdo(slave->config, sdo->index, &sdo->data[0], sdo->length) != 0) {
							rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo %04x (complete)\n", master->name,
											slave->name, sdo->index);
							lcec_startup_error = LCEC_STARTUP_ERR_SDO_CFG;
							goto lcec_startup_end;
						}
					} else {
						if (ecrt_slave_config_sdo(slave->config, sdo->index, sdo->subindex, &sdo->data[0], sdo->length) != 0) {
							rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo %04x:%02x\n", master->name,
											slave->name, sdo->index, sdo->subindex);
							lcec_startup_error = LCEC_STARTUP_ERR_SDO_CFG;
							goto lcec_startup_end;
						}
					}
				}
			}

			/* initialize idns */
			if (slave->idn_config != NULL) {
				for (idn = slave->idn_config; idn != NULL; idn = idn->next) {
					if (ecrt_slave_config_idn(slave->config, idn->drive, idn->idn, idn->state, &idn->data[0], idn->length) != 0) {
						rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s drive %d idn %c-%d-%d (state %d, length %u)\n", master->name, slave->name, idn->drive,
								  (idn->idn & 0x8000) ? 'P' : 'S', (idn->idn >> 12) & 0x0007, idn->idn & 0x0fff, idn->state, (unsigned int) idn->length);
					}
				}
			}


			/* setup pdos */
			if (slave->proc_init != NULL) {
				if ((slave->proc_init(comp_id, slave, pdo_entry_regs)) != 0) {
					lcec_startup_error = LCEC_STARTUP_ERR_PROC_INIT;
					goto lcec_startup_end;
				}
			}
			pdo_entry_regs += slave->pdo_entry_count;

			/* configure dc for this slave */
			if (slave->dc_conf != NULL) {
				ecrt_slave_config_dc(slave->config, slave->dc_conf->assignActivate, slave->dc_conf->sync0Cycle, slave->dc_conf->sync0Shift,
									 slave->dc_conf->sync1Cycle, slave->dc_conf->sync1Shift);
				rtapi_print_msg(
					RTAPI_MSG_DBG,
					LCEC_MSG_PFX
					"configuring DC for slave %s.%s: assignActivate=x%x sync0Cycle=%d sync0Shift=%d sync1Cycle=%d sync1Shift=%d\n",
					master->name, slave->name, slave->dc_conf->assignActivate, slave->dc_conf->sync0Cycle, slave->dc_conf->sync0Shift,
					slave->dc_conf->sync1Cycle, slave->dc_conf->sync1Shift);
			}

			/* configure slave */
			if (slave->sync_info != NULL) {
				if (ecrt_slave_config_pdos(slave->config, EC_END, slave->sync_info)) {
					rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s\n", master->name, slave->name);
					lcec_startup_error = LCEC_STARTUP_ERR_PDO_CFG;
					goto lcec_startup_end;
				}
			}

			/* export state pins */
			if ((slave->hal_state_data = lcec_init_slave_state_hal(master->name, slave->name)) == NULL) {
				lcec_startup_error = LCEC_STARTUP_ERR_PIN_EXPORT;
				goto lcec_startup_end;
			}
		}
		/* terminate POD entries */
		pdo_entry_regs->index = 0;

		/* register PDO entries */
		if (ecrt_domain_reg_pdo_entry_list(master->domain, master->pdo_entry_regs)) {
			rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "master %s PDO entry registration failed\n", master->name);
			lcec_startup_error = LCEC_STARTUP_ERR_REG_PDO;
			goto lcec_startup_end;
		}

		/* activating master */
		if (ecrt_master_activate(master->master)) {
			rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "failed to activate master %s\n", master->name);
			lcec_startup_error =  LCEC_STARTUP_ERR_MASTER_ACTIVATE;
			goto lcec_startup_end;
		}

		/* Get internal process data for domain */
		master->process_data     = ecrt_domain_data(master->domain);
		master->process_data_len = ecrt_domain_size(master->domain);

		// init hal data
		rtapi_snprintf(name, HAL_NAME_LEN, "%s.%s", lcec_module_name, master->name);
		if ((master->hal_data = lcec_init_master_hal(name, 0)) == NULL) {
			lcec_startup_error =  LCEC_STARTUP_ERR_INIT_MASTER_HAL;
			goto lcec_startup_end;
		}

		/* create "lcec thread" for this master */
		lcec_create_thread(master);
	}

lcec_startup_end:
	tell_dc_stable(DC_LCEC_INIT);
}

/***********************************************************************
 *                       INIT AND EXIT CODE                            *
 ***********************************************************************/

static int lcec_app_main(int n, lcec_connect_args_t *args)
{
	int	slave_count;

	/* Store component name */
	strcpy(lcec_module_name, args->name);

	comp_id = hal_init(__core_hal_user, lcec_module_name);
	if (comp_id < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "LCEC: ERROR: hal_init() failed\n");
		goto fail0;
	}

	/* Build EtherCAT bus configuration */
	slave_count = lcec_build_config(args->config);
	if (slave_count < 0)
		goto fail1;

#ifdef PRINT_CONFIG
	lcec_print_cfg();
#endif

	/* lcec startup */
	lcec_startup_error = LCEC_STARTUP_ERR_NO_ERROR;
	if (args->debug) {
		lcec_startup_debug_mode();
	} else {
		do_sync_dom(DOMID_AGENCY_RT, DC_LCEC_INIT);
		if (lcec_startup_error)
			goto fail1;
	}

	hal_ready(__core_hal_user, comp_id);
	rtapi_print_msg(RTAPI_MSG_INFO, LCEC_MSG_PFX "installed driver for %d slaves\n", slave_count);

	return lcec_startup_error;

fail1:
	hal_exit(__core_hal_user, comp_id);
fail0:
	return -EINVAL;
}

static void lcec_app_exit(void)
{
	lcec_master_t *master;
	char name[HAL_NAME_LEN + 1];

	for (master = first_master; master != NULL; master = master->next) {
		/* Delete thread */
		rtapi_snprintf(name, sizeof(name), "lcec_thread.%d", master->index);
		hal_thread_delete(__core_hal_user, name);

		/* deactivate all masters */
		ecrt_master_deactivate(master->master);
	}

	lcec_clear_config();

	hal_exit(__core_hal_user, comp_id);
}

/************************************************************************
 *                Char Device & file operation definitions              *
 ************************************************************************/

static int lcec_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int lcec_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static long lcec_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0, major, minor;
	hal_user_t *hal_user;

	major = imajor(filp->f_path.dentry->d_inode);
	minor = iminor(filp->f_path.dentry->d_inode);

	switch (cmd) {

	case LCEC_IOCTL_CONNECT:

		/* Pure kernel side init */
#warning Check if already present (initialized) ...

		rc = lcec_app_main(minor, (lcec_connect_args_t *)arg);
		if (rc) {
			printk("%s: failed to initialize...\n", __func__);
			goto out;
		}
		break;

	case LCEC_IOCTL_DISCONNECT:

		lcec_app_exit();

		hal_user = find_hal_user_by_dev(major, minor);
		BUG_ON(hal_user == NULL);
		hal_exit(hal_user, hal_user->comp_id);
		break;

	}
out:
	return rc;
}

struct file_operations lcec_fops = {
	.owner = THIS_MODULE,
	.open = lcec_open,
	.release = lcec_release,
	.unlocked_ioctl = lcec_ioctl,
};

int lcec_comp_init(void)
{
	int rc;

	printk("OpenCN: lcec subsystem initialization.\n");

	/* Registering device */
	rc = register_chrdev(LCEC_DEV_MAJOR, LCEC_DEV_NAME, &lcec_fops);
	if (rc < 0) {
		printk("Cannot obtain the major number %d\n", LCEC_DEV_MAJOR);
		return rc;
	}

	rtdm_register_dc_event_callback(DC_LCEC_INIT, lcec_startup);
	rtdm_register_dc_event_callback(DC_EC_IOCTL_SOE_READ, ecrt_ioctl_soe_rt_read);

	return 0;
}

late_initcall(lcec_comp_init)
