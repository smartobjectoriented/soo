/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2016, 2018 Baptiste Delporte <bonel@bonel.net>
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

#ifndef VBUS_ME_H
#define VBUS_ME_H

#include <linux/module.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/init.h>

#include <xenomai/rtdm/driver.h>

#include <soo/hypervisor.h>
#include <soo/vbus.h>

#include <soo/uapi/avz.h>

#include <soo/grant_table.h>
#include <soo/vbstore.h>

extern unsigned int rtdm_dc_evtchn;

/* VBS_KEY_LENGTH as it is managed by vbstore */
#define VBS_KEY_LENGTH		50
#define VBUS_DEV_TYPE		32

/* VBUS_KEY_LENGTH as it is managed by vbstore */
#define VBUS_ID_SIZE		VBS_KEY_LENGTH

struct vbus_me_type
{
	char *root;
	unsigned int levels;
	int (*get_bus_id)(char bus_id[VBUS_ID_SIZE], const char *nodename);

	int (*probe)(struct vbus_type *bus, const char *type, const char *dir);
	void (*otherend_changed)(struct vbus_watch *watch);

	void (*suspend)(void);
	void (*resume)(void);
};

/* A vbus device. */
struct vbus_me_device {
	char devicetype[VBUS_DEV_TYPE];
	char nodename[VBS_KEY_LENGTH];
	char otherend[VBS_KEY_LENGTH];
	int otherend_id;

	struct vbus_watch otherend_watch;

	struct vbus_me_type *vbus;
	struct vbus_me_driver *vdrv;

	/* Used by the main frontend device list */
	struct list_head list;

	enum vbus_state state;

	/* So far, only the ME use this completion struct. On the agency side,
	 * the device can not be shutdown on live.
	 */
	struct completion down;

	/* This completion lock is used for synchronizing interactions during connecting, suspending and resuming activities. */
	struct completion sync_backfront;

	int resuming;
};

/* A vbus driver. */
struct vbus_me_driver {
	char *name;

	char devicetype[32];

	void (*probe)(struct vbus_me_device *dev);
	void (*otherend_changed)(struct vbus_me_device *dev, enum vbus_state backend_state);

	int (*shutdown)(struct vbus_me_device *dev);

	int (*suspend)(struct vbus_me_device *dev);
	int (*resume)(struct vbus_me_device *dev);
	int (*resumed)(struct vbus_me_device *dev);

	/* Used as an entry of the main vbus driver list */
	struct list_head list;

	void (*read_otherend_details)(struct vbus_me_device *dev);

	void *priv;
};

void vbus_register_frontend(struct vbus_me_driver *vdrv);

void vbus_me_unregister_driver(struct vbus_me_driver *drv);

void vbus_me_free_otherend_watch(struct vbus_me_device *dev, bool with_vbus);

void vbus_probe_frontend_init(void);

void vbus_me_dev_shutdown(struct vbus_me_device *vdev);

bool is_vbstore_populated(void);

extern unsigned int dc_evtchn[];

extern struct completion backend_initialized;

void vbus_probe_frontend_init(void);

int vbus_me_dev_probe(struct vbus_me_device *vdev);

int vbus_me_register_driver_common(struct vbus_me_driver *vdrv);

void vbus_me_dev_changed(const char *node, char *type, struct vbus_me_type *bus, const char *compat);

void vbus_me_watch_pathfmt(struct vbus_me_device *dev, struct vbus_watch *watch, void (*callback)(struct vbus_watch *), const char *pathfmt, ...);

extern void vbus_me_otherend_changed(struct vbus_watch *watch);

extern void vbus_me_read_otherend_details(struct vbus_me_device *vdev, char *id_node, char *path_node);

int vdev_me_probe(char *node, const char *compat);

#define VBUS_IS_ERR_READ(str) ({			\
	if (!IS_ERR(str) && strlen(str) == 0) {		\
		kfree(str);				\
		str = ERR_PTR(-ERANGE);			\
	}						\
	IS_ERR(str);					\
})

#define VBUS_EXIST_ERR(err) ((err) == -ENOENT || (err) == -ERANGE)

int vbus_me_grant_ring(struct vbus_me_device *dev, unsigned long ring_mfn);
int vbus_me_map_ring_valloc(struct vbus_me_device *dev, int gnt_ref, void **vaddr);
int vbus_me_map_ring(struct vbus_me_device *dev, int gnt_ref, grant_handle_t *handle, void *vaddr);

int vbus_me_unmap_ring_vfree(struct vbus_me_device *dev, void *vaddr);
int vbus_me_unmap_ring(struct vbus_me_device *dev, grant_handle_t handle, void *vaddr);

int vbus_me_alloc_evtchn(struct vbus_me_device *dev, uint32_t *port);
int vbus_me_bind_evtchn(struct vbus_me_device *dev, uint32_t remote_port, uint32_t *port);
int vbus_me_free_evtchn(struct vbus_me_device *dev, uint32_t port);

void remove_vbstore_entries(void);

void vbuswatch_thread_sync(void);
int get_vbstore_evtchn(void);

void ping_remote_domain(int domID, void (*ping_callback)(void));

void vbs_prepare_to_suspend(struct work_struct *unused);
void vbs_prepare_to_resume(struct work_struct *unused);

void vbs_suspend(void);
void vbs_resume(void);

void presetup_register_watch(struct vbus_watch *watch);
void presetup_unregister_watch(struct vbus_watch *watch);

void vbstore_devices_populate(void);
void vbstore_trigger_dev_probe(void);
int vbstore_uart_remove(unsigned int domID);

void postmig_setup(void);
int gnttab_remove(bool with_vbus);

void register_dc_event_callback(dc_event_t dc_event, dc_event_fn_t *callback);

void postmig_vbstore_setup(struct DOMCALL_sync_domain_interactions_args *args);

void add_new_dev(struct vbus_me_device *vdev);
int vbus_me_dev_remove(struct vbus_me_device *vdev);

void frontend_for_each(void *data, int (*fn)(struct vbus_me_device *, void *));
void frontend_device_shutdown(void);

void gnttab_me_init(void);
int gnttab_me_remove(bool with_vbus);

#ifdef DEBUG
#undef DBG
#define DBG(fmt, ...) \
    do { \
        lprintk("%s:%d > "fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while(0)
#else
#define DBG(fmt, ...)
#endif

#endif /* VBUS_ME_H */
