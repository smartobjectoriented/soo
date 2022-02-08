/********************************************************************
* Description: trivkins.c
*   general trivkins for 3 axis Cartesian machine
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* License: GPL Version 2
*
* Copyright (c) 2009 All rights reserved.
*
********************************************************************/

#include <linux/fs.h>
#include <linux/string.h>

#include <opencn/rtapi/rtapi.h>            /* RTAPI realtime OS API */
#include <opencn/rtapi/rtapi_app.h>        /* RTAPI realtime module decls */
#include <opencn/hal/hal.h>

#include <opencn/motion/emcmotcfg.h>
#include <opencn/motion/emcpos.h>
#include <opencn/motion/kinematics.h>

#include <opencn/uapi/kins.h>

#define SET(f) pos->f = joints[i]

struct data {
	hal_s32_t joints[EMCMOT_MAX_JOINTS];
} *data;

static char coordinates[EMCMOT_MAX_JOINTS+1];
static char kinstype[2];

static kinematics_type_t ktype = -1;

static int comp_id;

/************************************************************************
 *                       Trivial kinematics code                        *
 ************************************************************************/

kinematics_type_t kinematicsType(void)
{
    return ktype;
}

int kinematicsForward(const double *joints,
		      EmcPose * pos,
		      const KINEMATICS_FORWARD_FLAGS * fflags,
		      KINEMATICS_INVERSE_FLAGS * iflags)
{
	int i;

	for(i = 0; i < EMCMOT_MAX_JOINTS; i++) {
		switch(data->joints[i]) {
		case 0: SET(tran.x); break;
		case 1: SET(tran.y); break;
		case 2: SET(tran.z); break;
		case 3: SET(a); break;
		case 4: SET(b); break;
		case 5: SET(c); break;
		case 6: SET(u); break;
		case 7: SET(v); break;
		case 8: SET(w); break;
        }
    }

    return 0;
}

int kinematicsInverse(const EmcPose * pos,
		      double *joints,
		      const KINEMATICS_INVERSE_FLAGS * iflags,
		      KINEMATICS_FORWARD_FLAGS * fflags)
{
	int i;
	for(i = 0; i < EMCMOT_MAX_JOINTS; i++) {
		switch(data->joints[i]) {
		case 0: joints[i] = pos->tran.x; break;
		case 1: joints[i] = pos->tran.y; break;
		case 2: joints[i] = pos->tran.z; break;
		case 3: joints[i] = pos->a; break;
		case 4: joints[i] = pos->b; break;
		case 5: joints[i] = pos->c; break;
		case 6: joints[i] = pos->u; break;
		case 7: joints[i] = pos->v; break;
		case 8: joints[i] = pos->w; break;
		}
	}

	return 0;
}

EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);


/************************************************************************
 *                       INIT AND EXIT CODE                             *
 ************************************************************************/

static int next_axis_number(void)
{
	char *coor = coordinates;

	while(*coor) {
		switch(*coor) {
		case 'x': case 'X': coor++; return 0;
		case 'y': case 'Y': coor++; return 1;
		case 'z': case 'Z': coor++; return 2;
		case 'a': case 'A': coor++; return 3;
		case 'b': case 'B': coor++; return 4;
		case 'c': case 'C': coor++; return 5;
		case 'u': case 'U': coor++; return 6;
		case 'v': case 'V': coor++; return 7;
		case 'w': case 'W': coor++; return 8;
		case ' ': case '\t': coor++; continue;
		}

		rtapi_print_msg(RTAPI_MSG_ERR,
			"trivkins: ERROR: Invalid character '%c' in coordinates\n",
			*coor);
		return -1;
    }
    return -1;
}


static int trivkins_app_main(kins_connect_args_t *args)
{
	int i;

	printk("type: %s\n", args->type);
	printk("coordinates: %s\n", args->coordinates);

	strcpy(kinstype, args->type);
	strcpy(coordinates, args->coordinates);

	printk("type: %s\n", kinstype);
	printk("coordinates: %s\n", coordinates);

	comp_id = hal_init(__core_hal_user, "trivkins");
	if(comp_id < 0)
		return comp_id;

	data = hal_malloc(__core_hal_user, sizeof(struct data));

	for(i=0; i<EMCMOT_MAX_JOINTS; i++) {
		data->joints[i] = next_axis_number();
	}

    switch (kinstype[0]) {
	case 'b':
	case 'B':
		ktype = KINEMATICS_BOTH;
		break;
	case 'f':
	case 'F':
		ktype = KINEMATICS_FORWARD_ONLY;
		break;
	case 'i':
	case 'I':
		ktype = KINEMATICS_INVERSE_ONLY;
		break;
	case '1':
	default:
		ktype = KINEMATICS_IDENTITY;
    }

    hal_ready(__core_hal_user, comp_id);
    return 0;
}


void trivkins_app_exit(void)
{
	hal_exit(__core_hal_user, comp_id);
}

/************************************************************************
 *                       File operation cod                             *
 ************************************************************************/

int trivkins_open(struct inode *inode, struct file *file)
{
	return 0;
}

int trivkins_release(struct inode *inode, struct file *filp)
{
	return 0;
}

long trivkins_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int major, minor;
	hal_user_t *hal_user;

	major = imajor(filp->f_path.dentry->d_inode);
	minor = iminor(filp->f_path.dentry->d_inode);

	printk(" ioctl called\n");

	switch (cmd) {

	case KINS_IOCTL_CONNECT:
		printk(" ioctl connect called\n");

		trivkins_app_main((kins_connect_args_t *)arg);
		break;

	case KINS_IOCTL_DISCONNECT:
		hal_user = find_hal_user_by_dev(major, minor);
		BUG_ON(hal_user == NULL);

		hal_exit(hal_user, hal_user->comp_id);

		break;
	}

	return 0;
}


/************************************************************************
 *                       Module initialization                          *
 ************************************************************************/

struct file_operations trivkins_fops = {
	.owner = THIS_MODULE,
	.open = trivkins_open,
	.release = trivkins_release,
	.unlocked_ioctl = trivkins_ioctl,
};

int trivkins_comp_init(void) {

	int rc;

	printk("OpenCN: trivial kinematics subsystem initialization.\n");

	/* Registering device */
	rc = register_chrdev(KINS_DEV_MAJOR, KINS_DEV_NAME, &trivkins_fops);
	if (rc < 0) {
		printk("Cannot obtain the major number %d\n", KINS_DEV_MAJOR);
		return rc;
	}

	return 0;
}

late_initcall(trivkins_comp_init)
