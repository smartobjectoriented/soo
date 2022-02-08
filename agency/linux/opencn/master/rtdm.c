/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2009-2010  Moehwald GmbH B. Benner
 *                     2011  IgH Andreas Stewering-Bone
 *                     2012  Florian Pose <fp@igh-essen.com>
 *
 *  This file is part of the IgH EtherCAT master.
 *
 *  The IgH EtherCAT master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation; version 2 of the License.
 *
 *  The IgH EtherCAT master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT master. If not, see <http://www.gnu.org/licenses/>.
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 ****************************************************************************/

/** \file
 * RTDM interface.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mman.h>

#include <xenomai/rtdm/driver.h>

#include "master.h"
#include "ioctl.h"
#include "rtdm.h"


#define MAX_RTDM_DEVICES			4
#define MAX_RTDM_DEVICES_LAST		3

/** Set to 1 to enable device operations debugging.
 */
#define DEBUG 0

/****************************************************************************/

/** Context structure for an open RTDM file handle.
 */
typedef struct {
	struct rtdm_fd* fd;  /**< RTDM user data. */
	ec_ioctl_context_t ioctl_ctx; /**< Context structure. */
	ec_rtdm_dev_t* dev;
} ec_rtdm_context_t;






/****************************************************************************/

static int ec_rtdm_open( struct rtdm_fd *, int);
static void ec_rtdm_close(struct rtdm_fd *);
int ec_rtdm_ioctl(struct rtdm_fd *, unsigned int, void __user *);

/****************************************************************************/


static struct rtdm_driver ec_rtdm_driver = {
	.profile_info = RTDM_PROFILE_INFO(
		EtherCAT,					// name
		RTDM_CLASS_EXPERIMENTAL,	// device class
		222,					// device sub class
		42 ),					// Profile version
								// -- was not listed in code originally

	.device_count = MAX_RTDM_DEVICES, 	//  (maximum) number of device instances
									// which may be managed by the driver

    .device_flags = RTDM_NAMED_DEVICE,
    .context_size = sizeof(ec_rtdm_context_t),

    .ops = {
		.open = ec_rtdm_open,
    	.close = ec_rtdm_close,
		.ioctl_rt = ec_rtdm_ioctl,
		.ioctl_nrt = ec_rtdm_ioctl
	},
};


static struct rtdm_device ec_rtdm_instances[MAX_RTDM_DEVICES] = {
	[ 0 ... MAX_RTDM_DEVICES_LAST] = {
		.driver = &ec_rtdm_driver,
		.label = "EtherCAT%d",
	},
};


/** Driver open.
 *
 * \return Always zero (success).
 */
static int
ec_rtdm_open(
	struct rtdm_fd *fd, /**< User data. */
	int oflags /**< Open flags. */ )
{

	ec_rtdm_context_t *ctx = (ec_rtdm_context_t *) rtdm_fd_to_private(fd);
#if DEBUG
    ec_rtdm_dev_t *rtdm_dev = (ec_rtdm_dev_t *) rtdm_fd_device(fd)->device_data;
#endif

    ctx->fd = fd;
    ctx->ioctl_ctx.writable = oflags & O_WRONLY || oflags & O_RDWR;
    ctx->ioctl_ctx.requested = 0;
    ctx->ioctl_ctx.process_data = NULL;
    ctx->ioctl_ctx.process_data_size = 0;

#if DEBUG
    EC_MASTER_INFO(rtdm_dev->master, "RTDM device %s opened.\n",
            rtdm_fd_device(fd)->device_name);
#endif
    return 0;
}

/****************************************************************************/

/** Driver close.
 *
 * \return Always zero (success).
 */
static void
ec_rtdm_close( struct rtdm_fd *fd /**< User data. */ )
{
	ec_rtdm_context_t *ctx = (ec_rtdm_context_t *) rtdm_fd_to_private(fd);
    ec_rtdm_dev_t *rtdm_dev = (ec_rtdm_dev_t *) rtdm_fd_device(fd)->device_data;

    if (ctx->ioctl_ctx.requested) {
        ecrt_release_master(rtdm_dev->master);
	}

#if DEBUG
    EC_MASTER_INFO(rtdm_dev->master, "RTDM device %s closed.\n",
            context->device->device_name);
#endif
}
/****************************************************************************/



/** Driver ioctl.
 *
 * \return ioctl() return code.
 */
int ec_rtdm_ioctl(
    struct rtdm_fd *fd,
    unsigned int request, /**< Request. */
    void __user *arg /**< Argument. */
    )
{
	ec_rtdm_context_t *ctx = (ec_rtdm_context_t *) rtdm_fd_to_private(fd);
    ec_rtdm_dev_t *rtdm_dev = (ec_rtdm_dev_t *) rtdm_fd_device(fd)->device_data;
#if DEBUG
    EC_MASTER_INFO(rtdm_dev->master, "ioctl(request = %u, ctl = %02x)"
            " on RTDM device %s.\n", request, _IOC_NR(request),
            context->device->device_name);
#endif
    return ec_ioctl_rtdm(rtdm_dev->master, &ctx->ioctl_ctx, request, arg);
}

/****************************************************************************/












/////////////////////////////////////////////////////////////////////////
// PUBLIC INTERFACE
/////////////////////////////////////////////////////////////////////////


/** Initialize an RTDM device.
 *
 * \return Zero on success, otherwise a negative error code.
 */
int ec_rtdm_dev_init(
        ec_rtdm_dev_t *rtdm_dev, /**< EtherCAT RTDM device. */
        ec_master_t *master /**< EtherCAT master. */
        )
{
	int ret;

	if ( master->index > MAX_RTDM_DEVICES)
	{
		ret = -1;
		EC_MASTER_ERR(master,
			"Initialization of RTDM interface failed. Index out of range."
				" (return value %i).\n",
			ret);

		return ret;
	}

	rtdm_dev->master = master;
	rtdm_dev->dev = &(ec_rtdm_instances[master->index]);
	rtdm_dev->dev->device_data = rtdm_dev; /* pointer to parent */

	EC_MASTER_INFO(master, "Registering RTDM device EtherCAT%d.\n",
		master->index );

	ret = rtdm_dev_register(rtdm_dev->dev);
	if (ret) {
		EC_MASTER_ERR(master, "Initialization of RTDM interface failed"
			" (return value %i).\n", ret);
		kfree(rtdm_dev->dev);
	}

	return ret;
}

/****************************************************************************/



/** Clear an RTDM device.
 */
void ec_rtdm_dev_clear(
        ec_rtdm_dev_t *rtdm_dev /**< EtherCAT RTDM device. */
        )
{
	EC_MASTER_INFO(rtdm_dev->master, "Unregistering RTDM device EtherCAT%d.\n",
		rtdm_dev->master->index );
    rtdm_dev_unregister(rtdm_dev->dev );
}

/****************************************************************************/



/** Memory-map process data to user space.
 *
 * \return Zero on success, otherwise a negative error code.
 */
int ec_rtdm_mmap(
        ec_ioctl_context_t *ioctl_ctx, /**< Context. */
        void **user_address /**< Userspace address. */
        )
{

    ec_rtdm_context_t *ctx =
        container_of(ioctl_ctx, ec_rtdm_context_t, ioctl_ctx);
    int ret;

    ret = rtdm_mmap_to_user(ctx->fd,
            ioctl_ctx->process_data, ioctl_ctx->process_data_size,
            PROT_READ | PROT_WRITE,
            user_address,
            NULL, NULL);

    if (ret < 0) {
        return ret;
    }

    return 0;
}

/****************************************************************************/
