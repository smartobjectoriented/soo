/********************************************************************
 * Description:  loopback.c
 *               A HAL component that can be used to loopback data
 *               from input pins to output pins.
 *
 * Author: Elieva Pignat <elieva.pignat at heig-vd dot ch>
 * License: GPL Version 2
 *
 * Copyright (c) 2006 All rights reserved.
 *
 ********************************************************************/
/** This file, 'loopback.c', is the realtime part of a HAL component
 that allows to copy any data coming on a HAL input pin to its paired
 HAL output  pin.  The component is created with paired input and output
 pins of the same type.
 */

/** Copyright (C) 2021 Elieva Pignat
 */

/** This program is free software; you can redistribute it and/or
 modify it under the terms of version 2 of the GNU General
 Public License as published by the Free Software Foundation.
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
 ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
 TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
 harming persons must have provisions for completely removing power
 from all motors, etc, before persons enter any danger area.  All
 machinery must be designed to comply with local and national safety
 codes, and the authors of this software can not, and do not, take
 any responsibility for such compliance.

 This code was written as part of the EMC HAL project.  For more
 information, go to www.linuxcnc.org.
 */
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/kernel.h>

#include <opencn/hal/hal.h>
#include <opencn/uapi/hal.h>
#include <opencn/uapi/loopback.h>
#include <opencn/rtapi/rtapi.h> /* RTAPI realtime OS API */
#include <opencn/rtapi/rtapi_app.h> /* RTAPI realtime module decls */
#include <opencn/rtapi/rtapi_errno.h>
#include <opencn/rtapi/rtapi_string.h>

typedef union {
    hal_bit_t *b;
    hal_float_t *f;
    hal_u32_t *u;
    hal_s32_t *s;
} loopback_pin_t;

typedef struct {
    unsigned int pins_number;
    hal_type_t pin_type[LOOPBACK_MAX_PINS];
    loopback_pin_t pins_in[LOOPBACK_MAX_PINS];
    loopback_pin_t pins_out[LOOPBACK_MAX_PINS];
} loopback_t;

static int comp_id; /* component ID */
static loopback_t *loopbacks[MAX_LOOPBACKS];

/***********************************************************************
 *                  LOCAL FUNCTION DECLARATIONS                         *
 ************************************************************************/

static int init_loopback(int num, loopback_t *loopback,
    loopback_connect_args_t *args);
static void loopback_update(void *arg, long period);

/***********************************************************************
 *                       INIT AND EXIT CODE                             *
 ************************************************************************/

static int loopback_app_main(int n, loopback_connect_args_t *args,
    bool initialized)
{
    int retval;

    if (n >= MAX_LOOPBACKS) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "[loopback] Error: Loopback ID is too large\n");
        return -EINVAL;
    }

    if (!initialized) {
        comp_id = hal_init(__core_hal_user, "loopback");
        if (comp_id < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                "[loopback] Error: hal_init() failed\n");
            return -EINVAL;
        }
    }

    loopbacks[n] =
        hal_malloc(__core_hal_user, sizeof(loopback_t));

    /* create pins pairs */
    retval = init_loopback(n, loopbacks[n], args);
    if (retval != 0) {
        goto fail;
    }

    hal_ready(__core_hal_user, comp_id);

    return 0;

fail:
    hal_exit(__core_hal_user, comp_id);

    return retval;
}

void loopback_app_exit(void)
{
    hal_exit(__core_hal_user, comp_id);
}

/***********************************************************************
 *            REALTIME COUNTER COUNTING AND UPDATE FUNCTIONS            *
 ************************************************************************/

static void loopback_update(void *arg, long period)
{
    loopback_t *loopback;
    int i;

    /* point at loopback struct in HAL */
    loopback = arg;

    /* copy data from input pins to HAL pins */
    for (i = 0; i < loopback->pins_number; i++) {
        switch (loopback->pin_type[i]) {
        case HAL_BIT:
            *(loopback->pins_out[i].b) = *(loopback->pins_in[i].b);
            break;
        case HAL_FLOAT:
            *(loopback->pins_out[i].f) = *(loopback->pins_in[i].f);
            break;
        case HAL_S32:
            *(loopback->pins_out[i].s) = *(loopback->pins_in[i].s);
            break;
        case HAL_U32:
            *(loopback->pins_out[i].u) = *(loopback->pins_in[i].u);
            break;
        default:
            break;
        }
    }

}

static int init_loopback(int num, loopback_t *loopback,
    loopback_connect_args_t *args)
{
    int retval = 0;
    /* Indicate if floating point are used */
    int usefp = 0;
    char buf[HAL_NAME_LEN + 1];
    int i = 0;
    /*
     * Parse cfg parameter and create a pair of input/output pins for each char
     */
    loopback->pins_number = strlen(args->cfg);
    if (loopback->pins_number >= LOOPBACK_MAX_PINS) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "[loopback] Error: Number of pins too large\n");
        return -EINVAL;
    }

    for (i = 0; i < loopback->pins_number; i++) {
        switch (args->cfg[i]) {
        case 'f':
            loopback->pin_type[i] = HAL_FLOAT;
            retval = hal_pin_float_newf(__core_hal_user, HAL_IN,
                &(loopback->pins_in[i].f), comp_id, "loopback.%d.f_in.%d",
                num, i);
            if (retval != 0) {
                break;
            }
            retval = hal_pin_float_newf(__core_hal_user, HAL_OUT,
                &(loopback->pins_out[i].f), comp_id, "loopback.%d.f_out.%d",
                num, i);
            usefp = 1;
            break;
        case 's':
            loopback->pin_type[i] = HAL_S32;
            retval = hal_pin_s32_newf(__core_hal_user, HAL_IN,
                &(loopback->pins_in[i].s), comp_id, "loopback.%d.s32_in.%d",
                num, i);
            if (retval != 0) {
                break;
            }
            retval = hal_pin_s32_newf(__core_hal_user, HAL_OUT,
                &(loopback->pins_out[i].s), comp_id, "loopback.%d.s32_out.%d",
                num, i);
            break;
        case 'u':
            loopback->pin_type[i] = HAL_U32;
            retval = hal_pin_u32_newf(__core_hal_user, HAL_IN,
                &(loopback->pins_in[i].u), comp_id, "loopback.%d.u32_in.%d",
                num, i);
            if (retval != 0) {
                break;
            }
            retval = hal_pin_u32_newf(__core_hal_user, HAL_OUT,
                &(loopback->pins_out[i].u), comp_id, "loopback.%d.u32_out.%d",
                num, i);
            break;
        case 'b':
            loopback->pin_type[i] = HAL_BIT;
            retval = hal_pin_bit_newf(__core_hal_user, HAL_IN,
                &(loopback->pins_in[i].b), comp_id, "loopback.%d.b_in.%d",
                num, i);
            if (retval != 0) {
                break;
            }
            retval = hal_pin_bit_newf(__core_hal_user, HAL_OUT,
                &(loopback->pins_out[i].b), comp_id, "loopback.%d.b_out.%d",
                num, i);
            break;
        default:
            retval = -1;
            break;
        }
    }
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "[loopback] Error: pin export failed\n");
        return -EIO;
    }

    rtapi_snprintf(buf, sizeof(buf), "loopback.%d", num);
    retval = hal_export_funct(__core_hal_user, buf, loopback_update, loopback,
        usefp, 0, comp_id);
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "[loopback] Error: function export failed\n");

        return retval;
    }

    return 0;
}

int loopback_open(struct inode *inode, struct file *file)
{
    return 0;
}

int loopback_release(struct inode *inode, struct file *filp)
{
    return 0;
}

ssize_t loopback_write(struct file *filp, const char __user *_sample,
    size_t len, loff_t *off)
{
    return 0;
}

long loopback_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int rc = 0, minor;
    static bool initialized = false;

    minor = iminor(filp->f_path.dentry->d_inode);

    switch (cmd) {
    case LOOPBACK_IOCTL_CONNECT:

        BUG_ON(minor + 1 > MAX_LOOPBACKS);

        /* Pure kernel side init */
        rc = loopback_app_main(minor, (loopback_connect_args_t *)arg,
            initialized);

        if (rc) {
            printk("%s: failed to initialize...\n", __func__);
            goto out;
        }
        initialized = true;
        break;

    case LOOPBACK_IOCTL_DISCONNECT:
        break;
    }
out:
    return rc;
}

struct file_operations loopback_fops = {
    .owner = THIS_MODULE,
    .open = loopback_open,
    .release = loopback_release,
    .write = loopback_write,
    .unlocked_ioctl = loopback_ioctl,
};

int loopback_comp_init(void)
{
    int rc;

    printk("OpenCN: loopback subsystem initialization.\n");

    /* Registering device */
    rc = register_chrdev(LOOPBACK_DEV_MAJOR, LOOPBACK_DEV_NAME,
                 &loopback_fops);
    if (rc < 0) {
        printk("Cannot obtain the major number %d\n",
               LOOPBACK_DEV_MAJOR);
        return rc;
    }

    return 0;
}

late_initcall(loopback_comp_init)

