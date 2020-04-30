/*
 * Copyright (C) 2019-2020 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
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
#include <linux/tee_drv.h>
#include <linux/optee_private.h>

#include <soo/uapi/console.h>

#define ASF_TA_DEV_MAJOR	100
#define ASF_TA_DEV_MINOR	0
#define ASF_TA_DEV_NAME  	"/dev/soo/asf"

#define ASF_MSG		"ASF drv: "


#define PTA_SECSTOR_TA_MGMT_BOOTSTRAP	0


#define ASF_TA_IOCTL_HELLO_WORLD		0

#define TA_HELLO_WORLD_CMD_INC_VALUE		0



static uint8_t mgmt_ta_uuid[] = { 0x6e, 0x25, 0x6c, 0xba, 0xfc, 0x4d, 0x49, 0x41,
				                  0xad, 0x09, 0x2c, 0xa1, 0x86, 0x03, 0x42, 0xdd };


static uint8_t hello_world_uuid[] = { 0x8a, 0xaa, 0xf2, 0x00, 0x24, 0x50, 0x11, 0xe4,
		                              0xab, 0xe2, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b};


/************************************************************************
 *                    Optee communication                               *
 ************************************************************************/


/**
 * asf_open_session() - Open a context & session with ASF TEE
 *
 * @ctx: TEE context
 * @return the TEE session ID or -1 in case of error
 */
static int asf_open_session(struct tee_context **ctx, uint8_t *uuid)
{
	struct tee_ioctl_open_session_arg sess_arg;
	int ret;

	/* Initialize a context connecting us to the TEE */
	*ctx = teedev_open(optee_svc->teedev);
	if (IS_ERR(*ctx)) {
		lprintk("ASF ERROR - Open Context failed\n");
		return -1;
	}

	memset(&sess_arg, 0, sizeof(sess_arg));

	memcpy(sess_arg.uuid, uuid, TEE_IOCTL_UUID_LEN);
	sess_arg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	sess_arg.num_params = 0;

	ret = tee_client_open_session(*ctx, &sess_arg, NULL);
	if ((ret < 0) || (sess_arg.ret != 0)) {
		lprintk("ASF ERROR - Open Session failed, error: %x\n", sess_arg.ret);
		return -1;
	}

	return sess_arg.session;
}


/**
 * asf_close_session() - Close a context & session ASF TEE opened by asf_open_session
 *
 * @ctx: TEE context
 * @session_id: the TEE session ID
 * @return 0 in case of success or -1
 */
static int asf_close_session(struct tee_context *ctx, int session_id)
{
	int ret;

	ret = tee_client_close_session(ctx, session_id);
	if (ret)  {
		lprintk("ASF ERROR - Close Context failed\n");
		return -1;
	}

	tee_client_close_context(ctx);

	return 0;
}



/************************************************************************
 *                          Driver support                              *
 ************************************************************************/

int asf_ta_open(struct inode *inode, struct file *file)
{
	printk(ASF_MSG "Open\n");
	return 0;
}

int asf_ta_release(struct inode *inode, struct file *filp)
{
	printk(ASF_MSG "Release\n");
	return 0;
}


ssize_t asf_ta_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	printk(ASF_MSG "read\n");

	return len;
}


ssize_t asf_ta_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	uint32_t session_id;
	uint8_t *data_in = NULL;
	struct tee_context *ctx;
	struct tee_ioctl_invoke_arg arg;
	struct tee_param param[1];
	struct tee_shm *shm = NULL;
	int ret;

	printk(ASF_MSG "Write\n");

	/* Open Session */
	session_id = asf_open_session(&ctx, mgmt_ta_uuid);
	if (session_id < 0) {
		printk(ASF_MSG "Error - Open session failed\n");

		// len = -1;
	}

	memset(&arg, 0, sizeof(arg));
	memset(&param, 0, sizeof(param));

	arg.func = PTA_SECSTOR_TA_MGMT_BOOTSTRAP;
	arg.session = session_id;
	arg.num_params = 1;

	shm = tee_shm_alloc(ctx, len, TEE_SHM_MAPPED | TEE_SHM_DMA_BUF);
	data_in  = tee_shm_get_va(shm, 0);
	memcpy(data_in, buf, len);

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[0].u.memref.shm = shm;
	param[0].u.memref.shm_offs = 0;
	param[0].u.memref.size = len;

	ret = tee_client_invoke_func(ctx, &arg, param);

	ret = asf_close_session(ctx, session_id);
	// if (ret)
	// 	len = -1;

	return len;
}


static void hello_world_cmd(void)
{
	struct tee_context *ctx;
	uint32_t session_id;
	struct tee_ioctl_invoke_arg arg;
	struct tee_param param[1];
	int ret;

	/* Open Session */
	session_id = asf_open_session(&ctx, hello_world_uuid);
	if (session_id < 0) {
		printk(ASF_MSG "Error - Open session failed\n");

		// len = -1;
	}

	memset(&arg, 0, sizeof(arg));
	memset(&param, 0, sizeof(param));
	arg.func = TA_HELLO_WORLD_CMD_INC_VALUE;
	arg.session = session_id;
	arg.num_params = 1;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT;
	param[0].u.value.a = 42;
	ret = tee_client_invoke_func(ctx, &arg, param);

	printk(ASF_MSG "hello_world_cmd, res: %d\n", (int)param[0].u.value.a);

	ret = asf_close_session(ctx, session_id);

}


long asf_ta_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0;

	switch (cmd) {

		case ASF_TA_IOCTL_HELLO_WORLD:
			hello_world_cmd();
			break;

		default:
			printk(ASF_MSG "Command %d not supported\n", cmd);
	}


	return rc;
}



struct file_operations asf_ta_fops = {
		.owner = THIS_MODULE,
		.open = asf_ta_open,
		.release = asf_ta_release,
		.unlocked_ioctl = asf_ta_ioctl,
		.read = asf_ta_read,
		.write = asf_ta_write,
};

int asf_ta_init(void)
{
	int rc;

	printk("== ASF TA: TA with secure storage experiments\n");

	/* Registering device */
	rc = register_chrdev(ASF_TA_DEV_MAJOR, ASF_TA_DEV_NAME, &asf_ta_fops);
	if (rc < 0) {
		printk("Cannot obtain the major number %d\n", ASF_TA_DEV_MAJOR);
		BUG();
	}

	return 0;
}

late_initcall(asf_ta_init)