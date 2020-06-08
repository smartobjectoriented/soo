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

#ifndef VBUS_H
#define VBUS_H

#include <linux/module.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/init.h>

#include <xenomai/rtdm/driver.h>

#include <soo/hypervisor.h>

#include <soo/uapi/avz.h>

#include <soo/grant_table.h>
#include <soo/vbstore.h>

/*
 * The following states are used to synchronize backend and frontend activities.
 * Detailed description is given in the SOO Framework technical reference.
 */

enum vbus_state
{
	VbusStateUnknown      = 0,
	VbusStateInitialising = 1,
	VbusStateInitWait     = 2,
	VbusStateInitialised  = 3,
	VbusStateConnected    = 4,
	VbusStateClosing      = 5,
	VbusStateClosed       = 6,
	VbusStateReconfiguring = 7,
	VbusStateReconfigured  = 8,
	VbusStateSuspending    = 9,
	VbusStateSuspended     = 10,
	VbusStateResuming      = 11

};

extern unsigned int rtdm_dc_evtchn;

/* VBS_KEY_LENGTH as it is managed by vbstore */
#define VBS_KEY_LENGTH		50
#define VBUS_DEV_TYPE		32

/* Register callback to watch this node. */
struct vbus_watch
{
	struct list_head list;

	struct vbus_device *dev;

	/* Path being watched. */
	char *node;

	/* Callback (executed in a process context with no locks held). */
	void (*callback)(struct vbus_watch *);

	volatile bool pending;
};

struct vbus_type
{
	char *root;
	unsigned int levels;
	void (*get_bus_id)(char bus_id[VBS_KEY_LENGTH], const char *nodename);

	int (*probe)(struct vbus_type *bus, const char *type, const char *dir);
	void (*otherend_changed)(struct vbus_watch *watch);

	void (*suspend)(void);
	void (*resume)(void);

	struct bus_type bus;
};


/* A vbus device. */
struct vbus_device {
	char devicetype[VBUS_DEV_TYPE];
	char nodename[VBS_KEY_LENGTH];
	char otherend[VBS_KEY_LENGTH];
	int otherend_id;

	struct vbus_watch otherend_watch;

	struct vbus_watch event_from_rt;
	struct vbus_watch event_from_nonrt;

	struct device dev;

	enum vbus_state state;

	/*
	 * State of the associated frontend.
	 * As soon as the frontend state, we get a watch event on the state
	 * property from vbstore, and we are up-to-date.
	 */
	enum vbus_state fe_state;

	/* So far, only the ME use this completion struct. On the agency side,
	 * the device can not be shutdown on live.
	 */
	struct completion down;

	/* This completion lock is used for synchronizing interactions during connecting, suspending and resuming activities. */
	struct completion sync_backfront;

	int resuming;

	bool realtime;  /* Tell if this device is a RT device */
};

static inline struct vbus_device *to_vbus_device(struct device *dev)
{
	return container_of(dev, struct vbus_device, dev);
}

/* A vbus driver. */
struct vbus_driver {
	char *name;
	struct module *owner;
	char devicetype[32];

	struct device_driver driver;

	void (*probe)(struct vbus_device *dev);
	void (*remove)(struct vbus_device *dev);

	void (*otherend_changed)(struct vbus_device *dev, enum vbus_state backend_state);

	int (*suspend)(struct vbus_device *dev);
	int (*resume)(struct vbus_device *dev);

	int (*uevent)(struct vbus_device *, struct kobj_uevent_env *);

	void (*read_otherend_details)(struct vbus_device *dev);
};

static inline struct vbus_driver *to_vbus_driver(struct device_driver *drv)
{
	return container_of(drv, struct vbus_driver, driver);
}

void __vbus_register_frontend(struct vbus_driver *drv, struct module *owner, const char *mod_name);

static inline void vbus_register_frontend(struct vbus_driver *drv)
{
	WARN_ON(drv->owner != THIS_MODULE);
	return __vbus_register_frontend(drv, THIS_MODULE, KBUILD_MODNAME);
}

void __vbus_register_backend(struct vbus_driver *drv, struct module *owner, const char *mod_name);

static inline void vbus_register_backend(struct vbus_driver *drv)
{
	WARN_ON(drv->owner != THIS_MODULE);
	return __vbus_register_backend(drv, THIS_MODULE, KBUILD_MODNAME);
}

void vbus_unregister_driver(struct vbus_driver *drv);

struct vbus_transaction
{
	u32 id; /* Unique non-zereo value to identify a transaction */
};

/* Nil transaction ID. */
#define VBT_NIL ((struct vbus_transaction) { 0 })

char **vbus_directory(struct vbus_transaction t, const char *dir, const char *node, unsigned int *num);
int vbus_directory_exists(struct vbus_transaction t, const char *dir, const char *node);
void *vbus_read(struct vbus_transaction t, const char *dir, const char *node, unsigned int *len);
void vbus_write(struct vbus_transaction t, const char *dir, const char *node, const char *string);
void vbus_mkdir(struct vbus_transaction t, const char *dir, const char *node);
int vbus_exists(struct vbus_transaction t, const char *dir, const char *node);
void vbus_rm(struct vbus_transaction t, const char *dir, const char *node);

void vbus_transaction_start(struct vbus_transaction *t);
void vbus_transaction_end(struct vbus_transaction t);

/* Single read and scanf: returns -errno or num scanned if > 0. */
int vbus_scanf(struct vbus_transaction t, const char *dir, const char *node, const char *fmt, ...)
	__attribute__((format(scanf, 4, 5)));

/* Single printf and write: returns -errno or 0. */
void vbus_printf(struct vbus_transaction t, const char *dir, const char *node, const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));

/* Generic read function: NULL-terminated triples of name,
 * sprintf-style type string, and pointer. Returns 0 or errno.*/
bool vbus_gather(struct vbus_transaction t, const char *dir, ...);

#ifdef CONFIG_SMP
void vbus_write_rt(struct vbus_transaction t, const char *dir, const char *node, const char *string);
void *vbus_read_rt(struct vbus_transaction t, const char *dir, const char *node, unsigned int *len);
bool rtdm_vbus_gather(struct vbus_transaction t, const char *dir, ...);
#endif

void free_otherend_watch(struct vbus_device *dev, bool with_vbus);

extern void vbstore_init(void);
extern void vbstore_me_init(void);

bool is_vbstore_populated(void);

void register_vbus_watch(struct vbus_watch *watch);
void unregister_vbus_watch_without_vbus(struct vbus_watch *watch);
void unregister_vbus_watch(struct vbus_watch *watch);

void remove_device(const char *path);
void vbus_cleanup_device(const char *path, struct bus_type *bus);

struct work_struct;

extern unsigned int dc_evtchn[];

extern struct completion backend_initialized;


extern int vbus_match(struct device *_dev, struct device_driver *_drv);
extern int vbus_dev_probe(struct device *_dev);

int vbus_probe_backend(int domid);
int vbus_probe_backend_init(void);

extern int vbus_dev_probe_frontend(struct device *_dev);
extern int vbus_dev_remove(struct device *_dev);
extern void vbus_register_driver_common(struct vbus_driver *drv, struct vbus_type *bus, struct module *owner, const char *mod_name);

extern int vbus_probe_devices(struct vbus_type *bus);

extern void vbus_dev_changed(const char *node, char *type, struct vbus_type *bus);

extern void vbus_dev_shutdown(struct device *_dev);

extern int vbus_dev_suspend(struct device *dev, pm_message_t state);
extern int vbus_dev_resume(struct device *dev);

extern void vbus_otherend_changed(struct vbus_watch *watch);

extern void vbus_read_otherend_details(struct vbus_device *vdev, char *id_node, char *path_node);
extern int vbus_suspend_dev(struct bus_type *bus, unsigned int domID);
extern int vbus_resume_dev(struct bus_type *bus, unsigned int domID);

/* Prepare for domain suspend: then resume or cancel the suspend. */
int vbus_suspend_devices(unsigned int domID);
int vbus_resume_devices(unsigned int domID);

int vdev_probe(char *node);
void vbus_probe_frontend_init(void);

#define VBUS_IS_ERR_READ(str) ({			\
	if (!IS_ERR(str) && strlen(str) == 0) {		\
		kfree(str);				\
		str = ERR_PTR(-ERANGE);			\
	}						\
	IS_ERR(str);					\
})

#define VBUS_EXIST_ERR(err) ((err) == -ENOENT || (err) == -ERANGE)

void vbus_watch_path(struct vbus_device *dev, char *path, struct vbus_watch *watch, void (*callback)(struct vbus_watch *));
void vbus_watch_pathfmt(struct vbus_device *dev, struct vbus_watch *watch, void (*callback)(struct vbus_watch *), const char *pathfmt, ...)
	__attribute__ ((format (printf, 4, 5)));

int vbus_grant_ring(struct vbus_device *dev, unsigned long ring_mfn);
int vbus_map_ring_valloc(struct vbus_device *dev, int gnt_ref, void **vaddr);
int vbus_map_ring(struct vbus_device *dev, int gnt_ref, grant_handle_t *handle, void *vaddr);

int vbus_unmap_ring_vfree(struct vbus_device *dev, void *vaddr);
int vbus_unmap_ring(struct vbus_device *dev, grant_handle_t handle, void *vaddr);

int vbus_alloc_evtchn(struct vbus_device *dev, uint32_t *port);
int vbus_bind_evtchn(struct vbus_device *dev, uint32_t remote_port, uint32_t *port);
int vbus_free_evtchn(struct vbus_device *dev, uint32_t port);

enum vbus_state vbus_read_driver_state(const char *path);
bool vbus_read_driver_realtime(const char *path);

const char *vbus_strstate(enum vbus_state state);
int vbus_dev_is_online(struct vbus_device *dev);

int vbus_frontend_closed(struct vbus_device *dev);
int vbus_frontend_terminated(struct vbus_device *dev);
void vbus_frontend_suspended(struct vbus_device *dev);
void vbus_frontend_resumed(struct vbus_device *dev);

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

void remove_devices(void);

void register_dc_event_callback(dc_event_t dc_event, dc_event_fn_t *callback);

void postmig_vbstore_setup(struct DOMCALL_sync_domain_interactions_args *args);

#ifdef DEBUG
#undef DBG
#define DBG(fmt, ...) \
    do { \
        printk("%s:%i > "fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while(0)
#else
#define DBG(fmt, ...)
#endif

#endif /* VBUS_H */
