/********************************************************************
 *  Copyright (C) 2019  Peter Lichard  <peter.lichard@heig-vd.ch>
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

/**
  * @file lcct.c
  */

#include "lcct_home.h"
#include "lcct_gcode.h"
#include "lcct_internal.h"
#include "lcct_jog.h"
#include "lcct_stream.h"

#ifdef CONFIG_ARM
#include <asm/neon.h>
#endif

#include <soo/uapi/console.h>
#include <opencn/uapi/lcct.h>

#define HAL_CHECK(expr)                                                                                                \
	{                                                                                                                  \
		int __retval = expr;                                                                                           \
		if (__retval != 0)                                                                                             \
			return __retval;                                                                                           \
	}

#define LCCT_STR_MAXLEN 48

extern void lcct_update_rt(void *arg, long period);

int hal_pin_newf(hal_user_t *hal_user, hal_type_t type, hal_pin_dir_t dir, void **data_ptr_addr, int comp_id,
				 const char *fmt, ...)
{
	int ret;
	va_list va;
	va_start(va, fmt);
	ret = hal_pin_newfv(hal_user, type, dir, data_ptr_addr, comp_id, fmt, va);
	va_end(va);
	return ret;
}

int init_pins_from_def(const pin_def_t *defs, int comp_id, void* pbase)
{
	int ret;
	BUG_ON(pbase == NULL);

	while(defs->pin_type != HAL_TYPE_UNSPECIFIED) {
		if ((ret = hal_pin_newf(__core_hal_user, defs->pin_type, defs->pin_dir, (void**)(pbase + defs->off), comp_id,
								defs->name)) != 0) {
			opencn_cprintf(OPENCN_COLOR_BRED, "init_pins_from_def: Failed to create pin '%s'\n", defs->name);
			return ret;
		}

		defs++;
	}
	return 0;
}

#define PIN(member) offsetof(lcct_data_t, member)

lcct_data_t *lcct_data = NULL;
hal_bit_t *hal_buttons[1024] = {0};
int hal_button_count = 0;
void add_hal_button(hal_bit_t *pin) { hal_buttons[hal_button_count++] = pin; }

static int comp_id; /* component ID */
char lcct_module_name[LCCT_STR_MAXLEN];


/************************************************************************
 *                       LCCT PINS CONNECTIONS                          *
 ************************************************************************/

static const pin_def_t pin_def[] = {
	{HAL_FLOAT, HAL_OUT, PIN(spindle_cmd_out), 		"lcct.spindle-cmd-out"},    /* Spindle speed command */
	{HAL_FLOAT, HAL_IN,  PIN(spindle_cur_in), 		"lcct.spindle-cur-in"},		/* Measured spindle speed */
	{HAL_FLOAT, HAL_OUT,  PIN(spindle_cur_out), 		"lcct.spindle-cur-out"}, /* Current spindle speed in rpm */

	{HAL_FLOAT, HAL_IN,  PIN(joint_pos_cur_in[0]), "lcct.joint-pos-cur-in-0"},
	{HAL_FLOAT, HAL_IN,  PIN(joint_pos_cur_in[1]), "lcct.joint-pos-cur-in-1"},
	{HAL_FLOAT, HAL_IN,  PIN(joint_pos_cur_in[2]), "lcct.joint-pos-cur-in-2"},
	{HAL_FLOAT, HAL_IN,  PIN(joint_pos_cur_in[3]), "lcct.joint-pos-cur-in-3"},

	{HAL_FLOAT, HAL_OUT,  PIN(target_position_out[0]), "lcct.target-position-0"},
	{HAL_FLOAT, HAL_OUT,  PIN(target_position_out[1]), "lcct.target-position-1"},
	{HAL_FLOAT, HAL_OUT,  PIN(target_position_out[2]), "lcct.target-position-2"},
	{HAL_FLOAT, HAL_OUT,  PIN(target_position_out[3]), "lcct.target-position-3"},

	{HAL_BIT,   HAL_IN, PIN(in_mode_csp_in[0]), "lcct.in-mode-csp-0"},
	{HAL_BIT,   HAL_IN, PIN(in_mode_csp_in[1]), "lcct.in-mode-csp-1"},
	{HAL_BIT,   HAL_IN, PIN(in_mode_csp_in[2]), "lcct.in-mode-csp-2"},
	{HAL_BIT,   HAL_IN, PIN(in_mode_csp_in[3]), "lcct.in-mode-csp-3"},

	{HAL_BIT,   HAL_IN, PIN(in_mode_csv_in[0]), "lcct.in-mode-csv-0"},
	{HAL_BIT,   HAL_IN, PIN(in_mode_csv_in[1]), "lcct.in-mode-csv-1"},
	{HAL_BIT,   HAL_IN, PIN(in_mode_csv_in[2]), "lcct.in-mode-csv-2"},
	{HAL_BIT,   HAL_IN, PIN(in_mode_csv_in[3]), "lcct.in-mode-csv-3"},

	{HAL_BIT,   HAL_IN, PIN(in_mode_hm_in[0]), "lcct.in-mode-hm-0"},
	{HAL_BIT,   HAL_IN, PIN(in_mode_hm_in[1]), "lcct.in-mode-hm-1"},
	{HAL_BIT,   HAL_IN, PIN(in_mode_hm_in[2]), "lcct.in-mode-hm-2"},
	{HAL_BIT,   HAL_IN, PIN(in_mode_hm_in[3]), "lcct.in-mode-hm-3"},

	{HAL_BIT,   HAL_IN, PIN(in_mode_inactive_in[0]), "lcct.in-mode-inactive-0"},
	{HAL_BIT,   HAL_IN, PIN(in_mode_inactive_in[1]), "lcct.in-mode-inactive-1"},
	{HAL_BIT,   HAL_IN, PIN(in_mode_inactive_in[2]), "lcct.in-mode-inactive-2"},
	{HAL_BIT,   HAL_IN, PIN(in_mode_inactive_in[3]), "lcct.in-mode-inactive-3"},

	{HAL_BIT,   HAL_IN, PIN(in_fault_in[0]), "lcct.in-fault-0"},
	{HAL_BIT,   HAL_IN, PIN(in_fault_in[1]), "lcct.in-fault-1"},
	{HAL_BIT,   HAL_IN, PIN(in_fault_in[2]), "lcct.in-fault-2"},
	{HAL_BIT,   HAL_IN, PIN(in_fault_in[3]), "lcct.in-fault-3"},

	{HAL_BIT,   HAL_OUT, PIN(set_mode_csp_out[0]), "lcct.set-mode-csp-0"},
	{HAL_BIT,   HAL_OUT, PIN(set_mode_csp_out[1]), "lcct.set-mode-csp-1"},
	{HAL_BIT,   HAL_OUT, PIN(set_mode_csp_out[2]), "lcct.set-mode-csp-2"},
	{HAL_BIT,   HAL_OUT, PIN(set_mode_csp_out[3]), "lcct.set-mode-csp-3"},

	{HAL_BIT,   HAL_OUT, PIN(set_mode_csv_out[0]), "lcct.set-mode-csv-0"},
	{HAL_BIT,   HAL_OUT, PIN(set_mode_csv_out[1]), "lcct.set-mode-csv-1"},
	{HAL_BIT,   HAL_OUT, PIN(set_mode_csv_out[2]), "lcct.set-mode-csv-2"},
	{HAL_BIT,   HAL_OUT, PIN(set_mode_csv_out[3]), "lcct.set-mode-csv-3"},

	{HAL_BIT,   HAL_OUT, PIN(set_mode_hm_out[0]), "lcct.set-mode-hm-0"},
	{HAL_BIT,   HAL_OUT, PIN(set_mode_hm_out[1]), "lcct.set-mode-hm-1"},
	{HAL_BIT,   HAL_OUT, PIN(set_mode_hm_out[2]), "lcct.set-mode-hm-2"},
	{HAL_BIT,   HAL_OUT, PIN(set_mode_hm_out[3]), "lcct.set-mode-hm-3"},

	{HAL_BIT,   HAL_OUT, PIN(set_mode_inactive_out[0]), "lcct.set-mode-inactive-0"},
	{HAL_BIT,   HAL_OUT, PIN(set_mode_inactive_out[1]), "lcct.set-mode-inactive-1"},
	{HAL_BIT,   HAL_OUT, PIN(set_mode_inactive_out[2]), "lcct.set-mode-inactive-2"},
	{HAL_BIT,   HAL_OUT, PIN(set_mode_inactive_out[3]), "lcct.set-mode-inactive-3"},

	{HAL_BIT,   HAL_IN,  PIN(set_machine_mode_gcode_in), "lcct.set-machine-mode-gcode"},
	{HAL_BIT,   HAL_IN,  PIN(set_machine_mode_stream_in), "lcct.set-machine-mode-stream"},
	{HAL_BIT,   HAL_IN,  PIN(set_machine_mode_homing_in), "lcct.set-machine-mode-homing"},
	{HAL_BIT,   HAL_IN,  PIN(set_machine_mode_jog_in), "lcct.set-machine-mode-jog"},
	{HAL_BIT,   HAL_IN,  PIN(set_machine_mode_inactive_in), "lcct.set-machine-mode-inactive"},

	{HAL_BIT,   HAL_OUT,  PIN(in_machine_mode_gcode_out), "lcct.in-machine-mode-gcode"},
	{HAL_BIT,   HAL_OUT,  PIN(in_machine_mode_stream_out), "lcct.in-machine-mode-stream"},
	{HAL_BIT,   HAL_OUT,  PIN(in_machine_mode_homing_out), "lcct.in-machine-mode-homing"},
	{HAL_BIT,   HAL_OUT,  PIN(in_machine_mode_jog_out), "lcct.in-machine-mode-jog"},
	{HAL_BIT,   HAL_OUT,  PIN(in_machine_mode_inactive_out), "lcct.in-machine-mode-inactive"},

	{HAL_BIT,	HAL_OUT, PIN(homing_finished_out), 	"lcct.homing-finished"},
	{HAL_BIT,	HAL_OUT, PIN(stream_finished_out), 	"lcct.stream-finished"},
	{HAL_BIT,	HAL_OUT, PIN(stream_running_out), 	"lcct.stream-running"},
	{HAL_BIT,	HAL_OUT, PIN(jog_finished_out), 	"lcct.jog-finished"},
	{HAL_BIT,	HAL_OUT, PIN(gcode_finished_out), 	"lcct.gcode-finished"},
	{HAL_BIT,	HAL_OUT, PIN(gcode_running_out), 	"lcct.gcode-running"},

	{HAL_BIT, HAL_IN,  PIN(fault_reset_in), "lcct.fault-reset"},
	{HAL_BIT, HAL_OUT, PIN(fault_reset_out[0]), "lcct.fault-reset-0"},
	{HAL_BIT, HAL_OUT, PIN(fault_reset_out[1]), "lcct.fault-reset-1"},
	{HAL_BIT, HAL_OUT, PIN(fault_reset_out[2]), "lcct.fault-reset-2"},
	{HAL_BIT, HAL_OUT, PIN(fault_reset_out[3]), "lcct.fault-reset-3"},

	{HAL_FLOAT, HAL_IN, PIN(spinbox_offset_in[AXIS_X_OFFSET]), "lcct.spinbox-offset-X"},
	{HAL_FLOAT, HAL_IN, PIN(spinbox_offset_in[AXIS_Y_OFFSET]), "lcct.spinbox-offset-Y"},
	{HAL_FLOAT, HAL_IN, PIN(spinbox_offset_in[AXIS_Z_OFFSET]), "lcct.spinbox-offset-Z"},
	{HAL_FLOAT, HAL_IN, PIN(spinbox_offset_thetaZ), "lcct.spinbox-offset-ThetaZ"},

	{HAL_FLOAT, HAL_IN, PIN(home_position_in[AXIS_X_OFFSET]), "lcct.home-position-X"},
	{HAL_FLOAT, HAL_IN, PIN(home_position_in[AXIS_Y_OFFSET]), "lcct.home-position-Y"},
	{HAL_FLOAT, HAL_IN, PIN(home_position_in[AXIS_Z_OFFSET]), "lcct.home-position-Z"},

    {HAL_FLOAT, HAL_IN, PIN(gui_spindle_target_velocity), "lcct.gui.spindle-target-velocity"},
	{HAL_BIT, HAL_IN, PIN(spindle_active), "lcct.spindle-active"},

	{HAL_BIT, HAL_OUT, PIN(disable_abs_jog), "lcct.disable-abs-jog"},
	{HAL_BIT, HAL_OUT, PIN(homed_out), "lcct.homed"},

	{HAL_BIT, HAL_OUT, PIN(sampler_enable_out), "lcct.sampler-enable-out"},
	{HAL_S32, HAL_OUT, PIN(external_trigger_out), "lcct.external-trigger"},
	{HAL_S32, HAL_OUT, PIN(electrovalve_out), "lcct.electrovalve"},

    {HAL_FLOAT, HAL_IN, PIN(spindle_threshold), "lcct.spindle-threshold"},
    {HAL_FLOAT, HAL_IN, PIN(spindle_acceleration), "lcct.spindle-acceleration"},

    HAL_PINDEF_END
};

static void init_lcct_buttons(void) {
//	add_hal_button(lcct_data->set_machine_mode_gcode_in);
//	add_hal_button(lcct_data->set_machine_mode_stream_in);
//	add_hal_button(lcct_data->set_machine_mode_homing_in);
//	add_hal_button(lcct_data->set_machine_mode_jog_in);
//	add_hal_button(lcct_data->set_machine_mode_inactive_in);
//	add_hal_button(lcct_data->fault_reset_in);
}

/************************************************************************
 *                       EXPORTED FUNCTONS                              *
 ************************************************************************/

/**
 * @brief       HAL-exported function acting as the realtime-callback
 * \callgraph
 */
static void lcct_update(void *arg, long period)
{
#ifdef CONFIG_ARM
	kernel_neon_begin();
#endif

	lcct_update_rt(arg, period);

#ifdef CONFIG_ARM
	kernel_neon_end();
#endif
}

/************************************************************************
 *                       INIT AND EXIT CODE                             *
 ************************************************************************/

static int lcct_app_main(int n, lcct_connect_args_t *args)
{
	rtapi_set_msg_level(RTAPI_MSG_WARN);

	/* Store component name */
	strcpy(lcct_module_name, args->name);

	if ((comp_id = hal_init(__core_hal_user, lcct_module_name)) < 0) {
		RTAPI_PRINT_MSG(RTAPI_MSG_ERR, "LCCT: hal_init() failed\n");
		return -EINVAL;
	}

	memset(hal_buttons, 0, sizeof(hal_buttons));

	set_command_finished();

    hal_export_funct(__core_hal_user, "lcct.update", lcct_update, NULL, 1, 0, comp_id);

	HAL_INIT_PINS(pin_def, comp_id, lcct_data);
	init_lcct_buttons();

    *lcct_data->spindle_threshold = 0.6;
    *lcct_data->spindle_acceleration = 30000; // RPM/s

	HAL_CHECK(lcct_home_init(comp_id));
	HAL_CHECK(lcct_jog_init(comp_id));
	HAL_CHECK(lcct_stream_init(comp_id));
	HAL_CHECK(lcct_gcode_init(comp_id));

	/* --------------------------------- */

	hal_ready(__core_hal_user, comp_id);

	main_state_init();
	return 0;
}

static void lcct_app_exit(void) { hal_exit(__core_hal_user, comp_id); }

/************************************************************************
 *                Char Device & file operation definitions              *
 ************************************************************************/

static int lcct_open(struct inode *inode, struct file *file) { return 0; }

static int lcct_release(struct inode *inode, struct file *filp) { return 0; }

static long lcct_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0, major, minor;
	hal_user_t *hal_user;

	major = imajor(filp->f_path.dentry->d_inode);
	minor = iminor(filp->f_path.dentry->d_inode);

	switch (cmd) {

	case LCCT_IOCTL_CONNECT:

		rc = lcct_app_main(minor, (lcct_connect_args_t *)arg);
		if (rc) {
			printk("%s: failed to initialize...\n", __func__);
			goto out;
		}
		break;

	case LCCT_IOCTL_DISCONNECT:
		lcct_app_exit();

		hal_user = find_hal_user_by_dev(major, minor);
		BUG_ON(hal_user == NULL);
		hal_exit(hal_user, hal_user->comp_id);
		break;
	}

	return 0;

out:
	hal_exit(__core_hal_user, comp_id);

	return rc;
}

struct file_operations lcct_fops = {
	.owner = THIS_MODULE,
	.open = lcct_open,
	.release = lcct_release,
	.unlocked_ioctl = lcct_ioctl,
};

int lcct_comp_init(void)
{
	int rc;

	printk("OpenCN: lcct subsystem initialization.\n");

	/* Registering device */
	rc = register_chrdev(LCCT_DEV_MAJOR, LCCT_DEV_NAME, &lcct_fops);
	if (rc < 0) {
		printk("Cannot obtain the major number %d\n", LCCT_DEV_MAJOR);
		return rc;
	}

	printk("OpenCN: lcct subsystem initialized\n");

	return 0;
}

late_initcall(lcct_comp_init)
