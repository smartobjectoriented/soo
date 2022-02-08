/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/mm.h>

#ifdef CONFIG_X86
#include <asm/fpu/api.h>
#endif

#include <asm/set_memory.h>

#include <opencn/strtox.h>

#include <opencn/rtapi/rtapi.h>            /* RTAPI realtime OS API */
#include <opencn/rtapi/rtapi_app.h>        /* RTAPI realtime module decls */
#include <opencn/hal/hal.h>                /* HAL public API decls */
#include <opencn/hal/hal_priv.h>

#include <opencn/ctypes/strings.h>

#include <opencn/logfile.h>

#include <linux/uaccess.h>

#include <opencn/uapi/hal.h>

#define halcmd_output printk

static rtapi_log_back_ring_t rtapi_log_ring;

static int tmatch(int req_type, int type) {
	return req_type == -1 || type == req_type;
}

static int match(char **patterns, char *value) {
	int i;
	if (!patterns || !patterns[0] || !patterns[0][0])
		return 1;
	for (i = 0; patterns[i] && *patterns[i]; i++) {
		char *pattern = patterns[i];
		if (strncmp(pattern, value, strlen(pattern)) == 0)
			return 1;
		if (fnmatch(pattern, value, 0) == 0)
			return 1;
	}
	return 0;
}

static int set_common(hal_type_t type, void *d_ptr, char *value) {
	/* This function assumes that the mutex is held */
	int retval = 0;
	double fval;
	long lval;
	unsigned long ulval;
	char *cp = value;

	switch (type) {
	case HAL_BIT:
		if ((strcmp("1", value) == 0) || (strcasecmp("TRUE", value) == 0)) {
			*(hal_bit_t *) (d_ptr) = 1;
		} else if ((strcmp("0", value) == 0) || (strcasecmp("FALSE", value)) == 0) {
			*(hal_bit_t *) (d_ptr) = 0;
		} else {
			rtapi_print_msg(RTAPI_MSG_ERR, "value '%s' invalid for bit\n", value);
			retval = -EINVAL;
		}
		break;
	case HAL_FLOAT:
		fval = strtod(value, &cp);
		if ((*cp != '\0') && (!isspace(*cp))) {
			/* invalid character(s) in string */
			rtapi_print_msg(RTAPI_MSG_ERR, "value '%s' invalid for float\n", value);
			retval = -EINVAL;
		} else {
			*((hal_float_t *) (d_ptr)) = fval;
		}
		break;
	case HAL_S32:
		lval = strtol(value, &cp, 0);
		if ((*cp != '\0') && (!isspace(*cp))) {
			/* invalid chars in string */
			rtapi_print_msg(RTAPI_MSG_ERR, "value '%s' invalid for S32\n", value);
			retval = -EINVAL;
		} else {
			*((hal_s32_t *) (d_ptr)) = lval;
		}
		break;
	case HAL_U32:
		ulval = strtoul(value, &cp, 0);
		if ((*cp != '\0') && (!isspace(*cp))) {
			/* invalid chars in string */
			rtapi_print_msg(RTAPI_MSG_ERR, "value '%s' invalid for U32\n", value);
			retval = -EINVAL;
		} else {
			*((hal_u32_t *) (d_ptr)) = ulval;
		}
		break;
	default:
		/* Shouldn't get here, but just in case... */
		rtapi_print_msg(RTAPI_MSG_ERR, "bad type %d\n", type);
		retval = -EINVAL;
	}
	return retval;
}

/* Switch function for arrow direction for the print_*_list functions  */
static const char *data_arrow1(int dir) {
	const char *arrow;

	switch (dir) {
		case HAL_IN:
			arrow = "<==";
			break;
		case HAL_OUT:
			arrow = "==>";
			break;
		case HAL_IO:
			arrow = "<=>";
			break;
		default:
			/* Shouldn't get here, but just in case... */
			arrow = "???";
	}
	return arrow;
}

/* Switch function for arrow direction for the print_*_list functions  */
static const char *data_arrow2(int dir) {
	const char *arrow;

	switch (dir) {
		case HAL_IN:
			arrow = "==>";
			break;
		case HAL_OUT:
			arrow = "<==";
			break;
		case HAL_IO:
			arrow = "<=>";
			break;
		default:
			/* Shouldn't get here, but just in case... */
			arrow = "???";
	}
	return arrow;
}

/* Switch function to return var value for the print_*_list functions  */
/* the value is printed in a 12 character wide field */
static char *data_value(int type, void *valptr) {
	char *value_str;
	static char buf[15];

	switch (type) {
		case HAL_BIT:
			if (*((char *) valptr) == 0)
				value_str = "       FALSE";
			else
				value_str = "        TRUE";
			break;
		case HAL_FLOAT:
			opencn_snprintf(buf, 14, "%12.7g", (double) *((hal_float_t *) valptr));
			value_str = buf;
			break;
		case HAL_S32:
			snprintf(buf, 14, "  %10ld", (long) *((hal_s32_t *) valptr));
			value_str = buf;
			break;
		case HAL_U32:
			snprintf(buf, 14, "  0x%08lX", (unsigned long) *((hal_u32_t *) valptr));
			value_str = buf;
			break;
		default:
			/* Shouldn't get here, but just in case... */
			value_str = "   undef    ";
	}
	return value_str;
}

#if 0 /* Not useful at the moment */
/* Switch function to return var value in string form  */
/* the value is printed as a packed string (no whitespace */
static char *data_value2(int type, void *valptr) {
	char *value_str;
	static char buf[15];

	switch (type) {
		case HAL_BIT:
			if (*((char *) valptr) == 0)
				value_str = "FALSE";
			else
				value_str = "TRUE";
			break;
		case HAL_FLOAT:
			snprintf(buf, 14, "%.7g", (double) *((hal_float_t *) valptr));
			value_str = buf;
			break;
		case HAL_S32:
			snprintf(buf, 14, "%ld", (long) *((hal_s32_t *) valptr));
			value_str = buf;
			break;
		case HAL_U32:
			snprintf(buf, 14, "%ld", (unsigned long) *((hal_u32_t *) valptr));
			value_str = buf;
			break;
		default:
			/* Shouldn't get here, but just in case... */
			value_str = "unknown_type";
	}
	return value_str;
}
#endif /* 0 */

/* Switch function for pin/sig/param type for the print_*_list functions */
static const char *data_type(int type) {
	const char *type_str;

	switch (type) {
		case HAL_BIT:
			type_str = "bit  ";
			break;
		case HAL_FLOAT:
			type_str = "float";
			break;
		case HAL_S32:
			type_str = "s32  ";
			break;
		case HAL_U32:
			type_str = "u32  ";
			break;
		default:
			/* Shouldn't get here, but just in case... */
			type_str = "undef";
	}
	return type_str;
}

static const char *data_type2(int type) {
	const char *type_str;

	switch (type) {
	case HAL_BIT:
		type_str = "bit";
		break;
	case HAL_FLOAT:
		type_str = "float";
		break;
	case HAL_S32:
		type_str = "s32";
		break;
	case HAL_U32:
		type_str = "u32";
		break;
	default:
		/* Shouldn't get here, but just in case... */
		type_str = "undef";
	}
	return type_str;
}

/* Switch function for param direction for the print_*_list functions  */
static const char *param_data_dir(int dir) {
	const char *param_dir;

	switch (dir) {
		case HAL_RO:
			param_dir = "RO";
			break;
		case HAL_RW:
			param_dir = "RW";
			break;
		default:
			/* Shouldn't get here, but just in case... */
			param_dir = "??";
	}
	return param_dir;
}

/* Switch function for pin direction for the print_*_list functions  */
static const char *pin_data_dir(int dir) {
	const char *pin_dir;

	switch (dir) {
	case HAL_IN:
		pin_dir = "IN";
		break;
	case HAL_OUT:
		pin_dir = "OUT";
		break;
	case HAL_IO:
		pin_dir = "I/O";
		break;
	default:
		/* Shouldn't get here, but just in case... */
		pin_dir = "???";
	}
	return pin_dir;
}

static int preflight_net_cmd(char *signal, hal_sig_t *sig, char *pins[]) {
	int i, type = -1, writers = 0, bidirs = 0, pincnt = 0;
	char *writer_name = 0, *bidir_name = 0;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	/* if signal already exists, use its info */
	if (sig) {
		type = sig->type;
		writers = sig->writers;
		bidirs = sig->bidirs;
	}

	if (writers || bidirs) {
		hal_pin_t *pin;
		int next;
		for (next = hal_data->pin_list_ptr; next; next = pin->next_ptr) {
			pin = SHMPTR(__core_hal_user, next);
			if (SHMPTR(__core_hal_user, pin->signal) == sig && (pin->dir == HAL_OUT))
				writer_name = pin->name;
			if (SHMPTR(__core_hal_user, pin->signal) == sig && (pin->dir == HAL_IO))
				bidir_name = writer_name = pin->name;
		}
	}

	for (i = 0; pins[i] && *pins[i]; i++) {
		hal_pin_t *pin = 0;
		pin = halpr_find_pin_by_name(__core_hal_user, pins[i]);
		if (!pin) {
			rtapi_print_msg(RTAPI_MSG_ERR, "Pin '%s' does not exist\n", pins[i]);
			return -ENOENT;
		}
		if (SHMPTR(__core_hal_user, pin->signal) == sig) {
			/* Already on this signal */
			pincnt++;
			continue;
		} else if (pin->signal != 0) {
			hal_sig_t *osig = SHMPTR(__core_hal_user, pin->signal);
			rtapi_print_msg(RTAPI_MSG_ERR, "Pin '%s' was already linked to signal '%s'\n", pin->name, osig->name);
			return -EINVAL;
		}
		if (type == -1) {
			/* no pre-existing type, use this pin's type */
			type = pin->type;
		}
		if (type != pin->type) {
			rtapi_print_msg(RTAPI_MSG_ERR, "Signal '%s' of type '%s' cannot add pin '%s' of type '%s'\n", signal, data_type2(type),
					pin->name, data_type2(pin->type));
			return -EINVAL;
		}
		if (pin->dir == HAL_OUT) {
			if (writers || bidirs) {
dir_error:
				rtapi_print_msg(RTAPI_MSG_ERR, "Signal '%s' can not add %s pin '%s', it already has %s pin '%s'\n", signal, pin_data_dir(pin->dir),
						pin->name, bidir_name ? pin_data_dir(HAL_IO) : pin_data_dir(HAL_OUT), bidir_name ? bidir_name : writer_name);
				return -EINVAL;
			}
			writer_name = pin->name;
			writers++;
		}
		if (pin->dir == HAL_IO) {
			if (writers) {
				goto dir_error;
			}
			bidir_name = pin->name;
			bidirs++;
		}
		pincnt++;
	}
	if (pincnt)
		return 0;
	rtapi_print_msg(RTAPI_MSG_ERR, "'net' requires at least one pin, none given\n");

	return -EINVAL;
}

static void print_comp_info(char **patterns) {
	int next;
	hal_comp_t *comp;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	halcmd_output("Loaded HAL Components:\n");
	halcmd_output("ID      Type  %-*s PID   State\n", HAL_NAME_LEN, "Name");

	rtapi_mutex_get(&(hal_data->mutex));
	next = hal_data->comp_list_ptr;
	while (next != 0) {
		comp = SHMPTR(__core_hal_user, next);
		if (match(patterns, comp->name)) {
			if (comp->type == 2) {
				hal_comp_t *comp1 = halpr_find_comp_by_id(__core_hal_user, comp->comp_id & 0xffff);
				halcmd_output("    INST %s %s", comp1 ? comp1->name : "(unknown)", comp->name);
			} else {
				halcmd_output(" %5d  %-4s  %-*s", comp->comp_id, (comp->type ? "RT" : "User"), HAL_NAME_LEN, comp->name);
				halcmd_output(" %5s %s", "", comp->ready > 0 ? "ready" : "initializing");
			}
			halcmd_output("\n");
		}
		next = comp->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_output("\n");
}

static void print_pin_info(int type, char **patterns) {
	int next;
	hal_pin_t *pin;
	hal_comp_t *comp;
	hal_sig_t *sig;
	void *dptr;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	halcmd_output("Component Pins:\n");
	halcmd_output("Owner    Type  Dir         Value  Name\n");

	rtapi_mutex_get(&(hal_data->mutex));
	next = hal_data->pin_list_ptr;
	while (next != 0) {
		pin = SHMPTR(__core_hal_user, next);
		if (tmatch(type, pin->type) && match(patterns, pin->name)) {
			comp = NULL;
			if (pin->owner_ptr != -1)
				comp = SHMPTR(__core_hal_user, pin->owner_ptr);
			if (pin->signal != 0) {
				sig = SHMPTR(__core_hal_user, pin->signal);
				dptr = SHMPTR(__core_hal_user, sig->data_ptr);
			} else {
				sig = 0;
				dptr = &(pin->dummysig);
			}

			if (comp)
				halcmd_output(KERN_CONT " %5d     %5s %-3s  %9s  %s", comp->comp_id, data_type((int) pin->type),
												pin_data_dir((int) pin->dir),
												data_value((int) pin->type, dptr),
												pin->name);
			else
				halcmd_output(KERN_CONT "(anon)   %5s %-3s  %9s  %s", data_type((int) pin->type),
												pin_data_dir((int) pin->dir),
												data_value((int) pin->type, dptr),
												pin->name);

			if (sig == 0) {
				halcmd_output(KERN_CONT "\n");
			} else {
				halcmd_output(KERN_CONT " %s %s\n", data_arrow1((int) pin->dir), sig->name);
			}
		}
		next = pin->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_output("\n");
}


static void print_pin_aliases(char **patterns) {
	int next;
	hal_oldname_t *oldname;
	hal_pin_t *pin;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	halcmd_output("Pin Aliases:\n");
	halcmd_output(" %-*s  %s\n", HAL_NAME_LEN, "Alias", "Original Name");

	rtapi_mutex_get(&(hal_data->mutex));
	next = hal_data->pin_list_ptr;
	while (next != 0) {
		pin = SHMPTR(__core_hal_user, next);
		if (pin->oldname != 0) {
			/* name is an alias */
			oldname = SHMPTR(__core_hal_user, pin->oldname);
			if (match(patterns, pin->name) || match(patterns, oldname->name))
				halcmd_output(" %-*s  %s\n", HAL_NAME_LEN, pin->name, oldname->name);
		}
		next = pin->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_output("\n");
}

static void print_sig_info(int type, char **patterns) {
	int next;
	hal_sig_t *sig;
	void *dptr;
	hal_pin_t *pin;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	halcmd_output("Signals:\n");
	halcmd_output("Type          Value  Name     (linked to)\n");

	rtapi_mutex_get(&(hal_data->mutex));
	next = hal_data->sig_list_ptr;
	while (next != 0) {
		sig = SHMPTR(__core_hal_user, next);
		if (tmatch(type, sig->type) && match(patterns, sig->name)) {
			dptr = SHMPTR(__core_hal_user, sig->data_ptr);
			halcmd_output("%s  %s  %s\n", data_type((int) sig->type), data_value((int) sig->type, dptr), sig->name);
			/* look for pin(s) linked to this signal */
			pin = halpr_find_pin_by_sig(__core_hal_user, sig, 0);
			while (pin != 0) {
				halcmd_output("                         %s %s\n", data_arrow2((int) pin->dir), pin->name);
				pin = halpr_find_pin_by_sig(__core_hal_user, sig, pin);
			}
		}
		next = sig->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_output("\n");
}

#if 0 /* Not useful at the moment */
static void print_script_sig_info(int type, char **patterns) {
	int next;
	hal_sig_t *sig;
	void *dptr;
	hal_pin_t *pin;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	rtapi_mutex_get(&(hal_data->mutex));
	next = hal_data->sig_list_ptr;
	while (next != 0) {
		sig = SHMPTR(__core_hal_user, next);
		if (tmatch(type, sig->type) && match(patterns, sig->name)) {
			dptr = SHMPTR(__core_hal_user, sig->data_ptr);
			halcmd_output("%s  %s  %s", data_type((int) sig->type), data_value2((int) sig->type, dptr), sig->name);
			/* look for pin(s) linked to this signal */
			pin = halpr_find_pin_by_sig(__core_hal_user, sig, 0);
			while (pin != 0) {
				halcmd_output(" %s %s", data_arrow2((int) pin->dir), pin->name);
				pin = halpr_find_pin_by_sig(__core_hal_user, sig, pin);
			}
			halcmd_output("\n");
		}
		next = sig->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_output("\n");
}
#endif /* 0 */

static void print_param_info(int type, char **patterns) {
	int next;
	hal_param_t *param;
	hal_comp_t *comp;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	halcmd_output("Parameters:\n");
	halcmd_output("Owner   Type  Dir         Value  Name\n");

	rtapi_mutex_get(&(hal_data->mutex));
	next = hal_data->param_list_ptr;
	while (next != 0) {
		param = SHMPTR(__core_hal_user, next);
		if (tmatch(type, param->type), match(patterns, param->name)) {
			comp = SHMPTR(__core_hal_user, param->owner_ptr);

			halcmd_output(" %5d  %5s %-3s  %9s  %s\n", comp->comp_id, data_type((int) param->type),
											param_data_dir((int) param->dir),
											data_value((int) param->type,
											SHMPTR(__core_hal_user, param->data_ptr)),
																											param->name);

		}
		next = param->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_output("\n");
}

static void print_param_aliases(char **patterns) {
	int next;
	hal_oldname_t *oldname;
	hal_param_t *param;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	halcmd_output("Parameter Aliases:\n");
	halcmd_output(" %-*s  %s\n", HAL_NAME_LEN, "Alias", "Original Name");

	rtapi_mutex_get(&(hal_data->mutex));
	next = hal_data->param_list_ptr;
	while (next != 0) {
		param = SHMPTR(__core_hal_user, next);
		if (param->oldname != 0) {
			/* name is an alias */
			oldname = SHMPTR(__core_hal_user, param->oldname);
			if (match(patterns, param->name) || match(patterns, oldname->name))
				 halcmd_output(" %-*s  %s\n", HAL_NAME_LEN, param->name, oldname->name);
		}
		next = param->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_output("\n");
}

static void print_funct_info(char **patterns) {
	int next;
	hal_funct_t *fptr;
	hal_comp_t *comp;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	halcmd_output("Exported Functions:\n");
	halcmd_output("Owner   CodeAddr  Arg       FP   Users  Name\n");

	rtapi_mutex_get(&(hal_data->mutex));
	next = hal_data->funct_list_ptr;
	while (next != 0) {
		fptr = SHMPTR(__core_hal_user, next);
		if (match(patterns, fptr->name)) {
			comp = SHMPTR(__core_hal_user, fptr->owner_ptr);

			halcmd_output(" %05d  %08lx  %08lx  %-3s  %5d   %s\n", comp->comp_id, (long) fptr->funct,
										(long) fptr->arg,
										(fptr->uses_fp ? "YES" : "NO"),
										fptr->users, fptr->name);

		}
		next = fptr->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_output("\n");
}

static void print_thread_info(char **patterns) {
	int next_thread, n;
	hal_thread_t *tptr;
	hal_list_t *list_root, *list_entry;
	hal_funct_entry_t *fentry;
	hal_funct_t *funct;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	halcmd_output("Realtime Threads:\n");
	halcmd_output("     Period  FP     Name               (     Time, Max-Time )\n");

	rtapi_mutex_get(&(hal_data->mutex));
	next_thread = hal_data->thread_list_ptr;
	while (next_thread != 0) {
		tptr = SHMPTR(__core_hal_user, next_thread);
		if (match(patterns, tptr->name)) {
			/* note that the scriptmode format string has no \n */
			char name[HAL_NAME_LEN + 1];
			hal_pin_t* pin;
			hal_sig_t *sig;
			void *dptr;

			snprintf(name, sizeof(name), "%s.time", tptr->name);
			pin = halpr_find_pin_by_name(__core_hal_user, name);
			if (pin) {
				if (pin->signal != 0) {
					sig = SHMPTR(__core_hal_user, pin->signal);
					dptr = SHMPTR(__core_hal_user, sig->data_ptr);
				} else {
					sig = 0;
					dptr = &(pin->dummysig);
				}

				halcmd_output("%11ld  %-3s  %20s ( %8ld, %8ld )\n", tptr->period, (tptr->uses_fp ? "YES" : "NO"),
												tptr->name, (long) *(long*) dptr,
												(long) tptr->maxtime);
			} else {
				rtapi_print_msg(RTAPI_MSG_ERR, "unexpected: cannot find time pin for %s thread", tptr->name);
			}

			list_root = &(tptr->funct_list);
			list_entry = list_next(__core_hal_user, list_root);
			n = 1;
			while (list_entry != list_root) {
				/* print the function info */
				fentry = (hal_funct_entry_t *) list_entry;
				funct = SHMPTR(__core_hal_user, fentry->funct_ptr);
				/* scriptmode only uses one line per thread, which contains:
				 thread period, FP flag, name, then all functs separated by spaces  */

				halcmd_output("                 %2d %s\n", n, funct->name);

				n++;
				list_entry = list_next(__core_hal_user, list_entry);
			}

			halcmd_output("\n");

		}
		next_thread = tptr->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_output("\n");
}

static void print_comp_names(char **patterns) {
	int next;
	hal_comp_t *comp;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	rtapi_mutex_get(&(hal_data->mutex));
	next = hal_data->comp_list_ptr;
	while (next != 0) {
		comp = SHMPTR(__core_hal_user, next);
		if (match(patterns, comp->name)) {
			halcmd_output("%s ", comp->name);
		}
		next = comp->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_output("\n");
}

static void print_pin_names(char **patterns) {
	int next;
	hal_pin_t *pin;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	rtapi_mutex_get(&(hal_data->mutex));
	next = hal_data->pin_list_ptr;
	while (next != 0) {
		pin = SHMPTR(__core_hal_user, next);
		if (match(patterns, pin->name)) {
			halcmd_output("%s ", pin->name);
		}
		next = pin->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_output("\n");
}

static void print_sig_names(char **patterns) {
	int next;
	hal_sig_t *sig;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	rtapi_mutex_get(&(hal_data->mutex));
	next = hal_data->sig_list_ptr;
	while (next != 0) {
		sig = SHMPTR(__core_hal_user, next);
		if (match(patterns, sig->name)) {
			halcmd_output("%s ", sig->name);
		}
		next = sig->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_output("\n");
}

static void print_param_names(char **patterns) {
	int next;
	hal_param_t *param;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	rtapi_mutex_get(&(hal_data->mutex));
	next = hal_data->param_list_ptr;
	while (next != 0) {
		param = SHMPTR(__core_hal_user, next);
		if (match(patterns, param->name)) {
			halcmd_output("%s ", param->name);
		}
		next = param->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_output("\n");
}

static void print_funct_names(char **patterns) {
	int next;
	hal_funct_t *fptr;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	rtapi_mutex_get(&(hal_data->mutex));
	next = hal_data->funct_list_ptr;
	while (next != 0) {
		fptr = SHMPTR(__core_hal_user, next);
		if (match(patterns, fptr->name)) {
			halcmd_output("%s ", fptr->name);
		}
		next = fptr->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_output("\n");
}

static void print_thread_names(char **patterns) {
	int next_thread;
	hal_thread_t *tptr;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	rtapi_mutex_get(&(hal_data->mutex));
	next_thread = hal_data->thread_list_ptr;
	while (next_thread != 0) {
		tptr = SHMPTR(__core_hal_user, next_thread);
		if (match(patterns, tptr->name)) {
			halcmd_output("%s ", tptr->name);
		}
		next_thread = tptr->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	halcmd_output("\n");
}

static void print_lock_status(void) {
	int lock;

	lock = hal_get_lock(__core_hal_user);

	halcmd_output("HAL locking status:\n");
	halcmd_output("  current lock value %d (%02x)\n", lock, lock);

	if (lock == HAL_LOCK_NONE)
		halcmd_output("  HAL_LOCK_NONE - nothing is locked\n");
	if (lock & HAL_LOCK_LOAD)
		halcmd_output("  HAL_LOCK_LOAD    - loading of new components is locked\n");
	if (lock & HAL_LOCK_CONFIG)
		halcmd_output("  HAL_LOCK_CONFIG  - link and addf is locked\n");
	if (lock & HAL_LOCK_PARAMS)
		halcmd_output("  HAL_LOCK_PARAMS  - setting params is locked\n");
	if (lock & HAL_LOCK_RUN)
		halcmd_output("  HAL_LOCK_RUN     - running/stopping HAL is locked\n");
}

static int count_list(int list_root) {
	int n, next;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;

	rtapi_mutex_get(&(hal_data->mutex));
	next = list_root;
	n = 0;
	while (next != 0) {
		n++;
		next = *((int *) SHMPTR(__core_hal_user, next));
	}
	rtapi_mutex_give(&(hal_data->mutex));

	return n;
}

static void print_mem_status(void) {
	int active, recycled, next;
	hal_pin_t *pin;
	hal_param_t *param;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;


	halcmd_output("HAL memory status\n");
	halcmd_output("  used/total shared memory:   %ld/%d\n", (long) (HAL_SIZE - hal_data->shmem_avail), HAL_SIZE);
	/* count components */
	active = count_list(hal_data->comp_list_ptr);
	recycled = count_list(hal_data->comp_free_ptr);
	halcmd_output("  active/recycled components: %d/%d\n", active, recycled);

	/* count pins */
	active = count_list(hal_data->pin_list_ptr);
	recycled = count_list(hal_data->pin_free_ptr);
	halcmd_output("  active/recycled pins:       %d/%d\n", active, recycled);

	/* count parameters */
	active = count_list(hal_data->param_list_ptr);
	recycled = count_list(hal_data->param_free_ptr);
	halcmd_output("  active/recycled parameters: %d/%d\n", active, recycled);

	/* count aliases */
	rtapi_mutex_get(&(hal_data->mutex));
	next = hal_data->pin_list_ptr;
	active = 0;
	while (next != 0) {
		pin = SHMPTR(__core_hal_user, next);
		if (pin->oldname != 0)
		active++;
		next = pin->next_ptr;
	}
	next = hal_data->param_list_ptr;
	while (next != 0) {
		param = SHMPTR(__core_hal_user, next);
		if (param->oldname != 0)
		active++;
		next = param->next_ptr;
	}
	rtapi_mutex_give(&(hal_data->mutex));
	recycled = count_list(hal_data->oldname_free_ptr);
	halcmd_output("  active/recycled aliases:    %d/%d\n", active, recycled);

	/* count signals */
	active = count_list(hal_data->sig_list_ptr);
	recycled = count_list(hal_data->sig_free_ptr);
	halcmd_output("  active/recycled signals:    %d/%d\n", active, recycled);

	/* count functions */
	active = count_list(hal_data->funct_list_ptr);
	recycled = count_list(hal_data->funct_free_ptr);
	halcmd_output("  active/recycled functions:  %d/%d\n", active, recycled);

	/* count threads */
	active = count_list(hal_data->thread_list_ptr);
	recycled = count_list(hal_data->thread_free_ptr);
	halcmd_output("  active/recycled threads:    %d/%d\n", active, recycled);
}

static int get_type(char ***patterns) {
	char *typestr = 0;
	if (!(*patterns))
		return -1;
	if (!(*patterns)[0])
		return -1;
	if ((*patterns)[0][0] != '-' || (*patterns)[0][1] != 't')
		return -1;
	if ((*patterns)[0][2]) {
		typestr = &(*patterns)[0][2];
		*patterns += 1;
	} else if ((*patterns)[1][0]) {
		typestr = (*patterns)[1];
		*patterns += 2;
	}
	if (!typestr)
		return -1;
	if (strcmp(typestr, "float") == 0)
		return HAL_FLOAT;
	if (strcmp(typestr, "bit") == 0)
		return HAL_BIT;
	if (strcmp(typestr, "s32") == 0)
		return HAL_S32;
	if (strcmp(typestr, "u32") == 0)
		return HAL_U32;
	if (strcmp(typestr, "signed") == 0)
		return HAL_S32;
	if (strcmp(typestr, "unsigned") == 0)
		return HAL_U32;
	return -1;
}


/************ HAL commands *************/

int do_linkps(hal_linkps_args_t *hal_linkps_args);

int do_setp(hal_setp_args_t *hal_setp_args) {
	hal_param_t *param;
	hal_pin_t *pin;
	hal_type_t type;
	void *d_ptr;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;
	char *name, *value;
	int retval;

	name = hal_setp_args->name;
	value = hal_setp_args->value;

	/* get mutex before accessing shared data */
	rtapi_mutex_get(&(hal_data->mutex));

	/* search param list for name */
	param = halpr_find_param_by_name(__core_hal_user, name);
	if (param == 0) {
		pin = halpr_find_pin_by_name(__core_hal_user, name);
		if (pin == 0) {
			rtapi_mutex_give(&(hal_data->mutex));
			rtapi_print_msg(RTAPI_MSG_ERR, "parameter or pin '%s' not found\n", name);
			return -EINVAL;
		} else {
			/* found it */
			type = pin->type;
			if (pin->dir == HAL_OUT) {
				rtapi_mutex_give(&(hal_data->mutex));
				rtapi_print_msg(RTAPI_MSG_ERR, "pin '%s' is not writable\n", name);
				return -EINVAL;
			}
			if (pin->signal != 0) {
				rtapi_mutex_give(&(hal_data->mutex));
				rtapi_print_msg(RTAPI_MSG_ERR, "pin '%s' is connected to a signal\n", name);
				return -EINVAL;
			}
			/* d_ptr = (void*)SHMPTR(pin->dummysig); */
			d_ptr = (void*) &pin->dummysig;
		}
	} else {
		/* found it */
		type = param->type;
		/* is it read only? */
		if (param->dir == HAL_RO) {
			rtapi_mutex_give(&(hal_data->mutex));
			rtapi_print_msg(RTAPI_MSG_ERR, "param '%s' is not writable\n", name);
			return -EINVAL;
		}
		d_ptr = SHMPTR(__core_hal_user, param->data_ptr);
	}

	retval = set_common(type, d_ptr, value);

	rtapi_mutex_give(&(hal_data->mutex));
	if (retval == 0) {
		/* print success message */
		if (param) {
			rtapi_print_msg(RTAPI_MSG_INFO, "Parameter '%s' set to %s\n", name, value);
		} else {
			rtapi_print_msg(RTAPI_MSG_INFO, "Pin '%s' set to %s\n", name, value);
		}
	} else
		rtapi_print_msg(RTAPI_MSG_ERR, "setp failed\n");

	return 0;
}

int do_net(hal_net_args_t *hal_net_args) {
	hal_sig_t *sig;
	int i, retval;
	hal_data_t *hal_data = (hal_data_t *) __core_hal_user->hal_data;
	hal_pin_t *pin;
	char *signal;
	char **pins;
	hal_linkps_args_t hal_linkps_args;

	signal = hal_net_args->signal;
	pins = hal_net_args->pins;


	rtapi_mutex_get(&(hal_data->mutex));
	/* see if signal already exists */
	sig = halpr_find_sig_by_name(__core_hal_user, signal);

	/* verify that everything matches up (pin types, etc) */
	retval = preflight_net_cmd(signal, sig, pins);
	if (retval < 0) {
		rtapi_mutex_give(&(hal_data->mutex));
		return retval;
	}

	pin = halpr_find_pin_by_name(__core_hal_user, signal);
	if (pin) {
		rtapi_print_msg(RTAPI_MSG_ERR, "Signal name '%s' must not be the same as a pin. Did you omit the signal name?\n", signal);
		rtapi_mutex_give(&(hal_data->mutex));

		return -ENOENT;
	}

	if (!sig) {
		/* Create the signal with the type of the first pin */
		hal_pin_t *pin = halpr_find_pin_by_name(__core_hal_user, pins[0]);
		rtapi_mutex_give(&(hal_data->mutex));
		if (!pin) {
			return -ENOENT;
		}
		retval = hal_signal_new(__core_hal_user, signal, pin->type);
	} else {
		/* signal already exists */
		rtapi_mutex_give(&(hal_data->mutex));
	}
	/* add pins to signal */
	for (i = 0; (retval == 0) && pins[i] && *pins[i]; i++) {
		strncpy(hal_linkps_args.pin, pins[i], sizeof(hal_linkps_args.pin));
		strncpy(hal_linkps_args.sig, signal, sizeof(hal_linkps_args.sig));
		retval = do_linkps(&hal_linkps_args);
	}

	return 0;
}

int do_linkps(hal_linkps_args_t *hal_linkps_args) {
	int retval;
	char *pin;
	char *sig;

	pin = hal_linkps_args->pin;
	sig = hal_linkps_args->sig;

	retval = hal_link(__core_hal_user, pin, sig);
	if (retval == 0) {
		/* print success message */
		rtapi_print_msg(RTAPI_MSG_INFO, "Pin '%s' linked to signal '%s'\n", pin, sig);
	} else {
		rtapi_print_msg(RTAPI_MSG_ERR, "link failed\n");
	}
	return retval;
}

int do_show(hal_show_args_t *hal_show_args) {

	char *type = hal_show_args->type;
	char **patterns = hal_show_args->patterns;

	if (!type || *type == '\0') {
		/* print everything */
		print_comp_info(NULL);
		print_pin_info(-1, NULL);
		print_pin_aliases(NULL);
		print_sig_info(-1, NULL);
		print_param_info(-1, NULL);
		print_param_aliases(NULL);
		print_funct_info(NULL);
		print_thread_info(NULL);
	} else if (strcmp(type, "all") == 0) {
		/* print everything, using the pattern */
		print_comp_info(patterns);
		print_pin_info(-1, patterns);
		print_pin_aliases(patterns);
		print_sig_info(-1, patterns);
		print_param_info(-1, patterns);
		print_param_aliases(patterns);
		print_funct_info(patterns);
		print_thread_info(patterns);
	} else if (strcmp(type, "comp") == 0) {
		print_comp_info(patterns);
	} else if (strcmp(type, "pin") == 0) {
		int type = get_type(&patterns);
		print_pin_info(type, patterns);
	} else if (strcmp(type, "sig") == 0) {
		int type = get_type(&patterns);
		print_sig_info(type, patterns);
	} else if (strcmp(type, "signal") == 0) {
		int type = get_type(&patterns);
		print_sig_info(type, patterns);
	} else if (strcmp(type, "param") == 0) {
		int type = get_type(&patterns);
		print_param_info(type, patterns);
	} else if (strcmp(type, "parameter") == 0) {
		int type = get_type(&patterns);
		print_param_info(type, patterns);
	} else if (strcmp(type, "funct") == 0) {
		print_funct_info(patterns);
	} else if (strcmp(type, "function") == 0) {
		print_funct_info(patterns);
	} else if (strcmp(type, "thread") == 0) {
		print_thread_info(patterns);
	} else if (strcmp(type, "alias") == 0) {
		print_pin_aliases(patterns);
		print_param_aliases(patterns);
	} else {
		printk("%s: Unknown 'show' type '%s'\n", __func__, type);
		return -1;
	}

	return 0;
}

int do_list(hal_list_args_t *hal_list_args) {
	char *type = hal_list_args->type;
	char **patterns = hal_list_args->patterns;

	if (strcmp(type, "comp") == 0) {
		print_comp_names(patterns);
	} else if (strcmp(type, "pin") == 0) {
		print_pin_names(patterns);
	} else if (strcmp(type, "sig") == 0) {
		print_sig_names(patterns);
	} else if (strcmp(type, "signal") == 0) {
		print_sig_names(patterns);
	} else if (strcmp(type, "param") == 0) {
		print_param_names(patterns);
	} else if (strcmp(type, "parameter") == 0) {
		print_param_names(patterns);
	} else if (strcmp(type, "funct") == 0) {
		print_funct_names(patterns);
	} else if (strcmp(type, "function") == 0) {
		print_funct_names(patterns);
	} else if (strcmp(type, "thread") == 0) {
		print_thread_names(patterns);
	} else {
		printk("%s: Unknown 'list' type '%s'\n", __func__, type);

		return -1;
	}

	return 0;
}

int do_status(hal_status_args_t *hal_status_args) {
	char *type = hal_status_args->type;

	if ((type == NULL) || (strcmp(type, "all") == 0)) {
		/* print everything */
		/* add other status functions here if/when they are defined */
		print_lock_status();
		print_mem_status();
	} else if (strcmp(type, "lock") == 0) {
		print_lock_status();
	} else if (strcmp(type, "mem") == 0) {
		print_mem_status();
	} else {
		printk("%s: Unknown 'status' type '%s'\n", __func__, type);

		return -1;
	}
	return 0;
}

int do_addf(hal_addf_args_t *hal_addf_args) {
	int ret;

	ret = hal_add_funct_to_thread(__core_hal_user, hal_addf_args->func, hal_addf_args->thread, hal_addf_args->position);
	if (ret == 0)
		printk("%s: Function '%s' added to thread '%s'\n", __func__, hal_addf_args->func, hal_addf_args->thread);
	else
		printk("%s: addf failed\n", __func__);

	return ret;
}

int do_start(void) {
	return hal_start_threads(__core_hal_user);
}

int do_stop(void) {
	return hal_stop_threads(__core_hal_user);
}

int anon_pin_new(hal_anon_pin_args_t *hal_anon_pin_args) {

	return hal_pin_new(__core_hal_user, hal_anon_pin_args->name, hal_anon_pin_args->type, hal_anon_pin_args->dir, NULL, -1);
}

void anon_pin_delete(const char *name) {
	hal_pin_delete(__core_hal_user, name);
}

int hal_open(struct inode *inode, struct file *file) {
	hal_anon_pin_args_t hal_anon_pin;
	hal_pin_t *rtapi_log_pin;

	/* Pin associated to rtapi msg log enabling. */
	hal_anon_pin.name = HAL_RTAPI_LOG_PIN;
	hal_anon_pin.type = HAL_BIT;
	hal_anon_pin.dir = HAL_IO;

	rtapi_log_pin = halpr_find_pin_by_name(__core_hal_user, HAL_RTAPI_LOG_PIN);
	if (!rtapi_log_pin) {
		anon_pin_new(&hal_anon_pin);
		rtapi_log_pin = halpr_find_pin_by_name(__core_hal_user, HAL_RTAPI_LOG_PIN);
	}
	BUG_ON(rtapi_log_pin == NULL);

	p_rtapi_log_enabled = (bool *) &(rtapi_log_pin->dummysig);
	*p_rtapi_log_enabled = false;

	SHARED_RING_INIT(rtapi_log_sring);
	BACK_RING_INIT(&rtapi_log_ring, rtapi_log_sring, LOG_RING_SIZE);

	return 0;
}

int hal_release(struct inode *inode, struct file *filp) {
	return 0;
}

long hal_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	int rc = 0, minor;

	minor = iminor(filp->f_path.dentry->d_inode);

#ifdef CONFIG_X86
#warning checking if kernel_fpu_begin() is still needed !
	kernel_fpu_begin();
#endif

	switch (cmd) {

	case HAL_IOCTL_SETP:
		rc = do_setp((hal_setp_args_t *) arg);
		break;

	case HAL_IOCTL_NET:
		rc = do_net((hal_net_args_t *) arg);
		break;

	case HAL_IOCTL_LINKPS:
		rc = do_linkps((hal_linkps_args_t *) arg);
		break;

	case HAL_IOCTL_SHOW:
		rc = do_show((hal_show_args_t *) arg);
		break;

	case HAL_IOCTL_LIST:
		rc = do_list((hal_list_args_t *) arg);
		break;

	case HAL_IOCTL_STATUS:
		rc = do_status((hal_status_args_t *) arg);
		break;

	case HAL_IOCTL_ADDF:
		rc = do_addf((hal_addf_args_t *) arg);
		break;

	case HAL_IOCTL_START:
		rc = do_start();
		break;

	case HAL_IOCTL_STOP:
		rc = do_stop();
		break;

	case HAL_IOCTL_GET_SHMEM_ADDR:
		*((unsigned long *) arg) = (unsigned long) __core_hal_user->hal_shmem_base;
		rc = 0;
		break;

	case HAL_IOCTL_ANON_PIN_NEW:
		rc = anon_pin_new((hal_anon_pin_args_t *) arg);
		break;

	case HAL_IOCTL_ANON_PIN_DELETE:
		anon_pin_delete((char *) arg);
		rc = 0;
		break;
	}

#ifdef CONFIG_X86
	kernel_fpu_end();
#endif

	return rc;
}

/*
 * Prepare to map shmem pages to the user space for aflib usage.
 */
static int hal_mmap(struct file *filp, struct vm_area_struct *vma)
{
	void *shmem_base;
	unsigned long start, size;

#ifdef CONFIG_X86
	static bool __set_memory_uc_done = false;
#endif

	/* Get the pointer of the shared mem bound to the __core_hal_user. */
	rtapi_shmem_getptr(__core_hal_user->lib_mem_id, &shmem_base);

	vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;

	start = page_to_pfn(virt_to_page(shmem_base));
	size = vma->vm_end - vma->vm_start;

#ifdef CONFIG_X86
	/* The region must be set as uncached only the first time the function is called. */
	if (!__set_memory_uc_done) {

		/* PAT (Page Attribute) enabled, therefore prepare to set the region as uncached. */

		set_memory_uc((long unsigned int) shmem_base, size / PAGE_SIZE);

		__set_memory_uc_done = true;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_page_prot = phys_mem_access_prot(filp, vma->vm_pgoff, size, vma->vm_page_prot);

#endif /* CONFIG_X86 */

	if (remap_pfn_range(vma, vma->vm_start, start, size, vma->vm_page_prot))
		BUG();

	return 0;
}

ssize_t hal_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {

	rtapi_log_request_t *ring_req;
	RING_IDX i, rp;
	int msglen = 0;

	rp = rtapi_log_ring.sring->req_prod;
	mb();

	i = rtapi_log_ring.sring->req_cons;
	if (i != rp) {
		ring_req = RING_GET_REQUEST(&rtapi_log_ring, i);
		rtapi_log_ring.sring->req_cons++;

		/* TODO: sanity check... */
		strcpy(buf, ring_req->line);
		msglen = strlen(buf)+1;
	}

	return msglen;

}

struct file_operations hal_fops = {
	.owner          = THIS_MODULE,
	.open           = hal_open,
	.release        = hal_release,
	.unlocked_ioctl = hal_ioctl,
	.read		= hal_read,
	.mmap		= hal_mmap
};


int halcmd_init(void) {

	int rc;

	printk("OpenCN: halcmd_init streamer subsystem initialization.\n");

	/* Registering device */
	rc = register_chrdev(HAL_DEV_MAJOR, HAL_DEV_NAME, &hal_fops);
	if (rc < 0) {
		printk("Cannot obtain the major number %d\n", HAL_DEV_MAJOR);
		return rc;
	}

	return 0;
}

subsys_initcall(halcmd_init)
