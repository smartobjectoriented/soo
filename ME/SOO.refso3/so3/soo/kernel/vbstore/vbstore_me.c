/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
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

#if 0
#define DEBUG
#endif

#include <soo/avz.h>
#include <soo/soo.h>
#include <soo/hypervisor.h>
#include <soo/vbstore.h>
#include <soo/evtchn.h>
#include <soo/vbstore_me.h>

#include <soo/debug.h>
#include <soo/debug/logbool.h>

static void vbs_me_rmdir(const char *dir, const char *node) {
	vbus_rm(VBT_NIL, dir, node);
}

static void vbs_me_mkdir(const char *dir, const char *node) {
	vbus_mkdir(VBT_NIL, dir, node);
}

static void vbs_me_write(const char *dir, const char *node, const char *string) {
	vbus_write(VBT_NIL, dir, node, string);
}

/*
 * The following vbstore node creation does not require to be within a transaction
 * since the backend has no visibility on these nodes until it gets the DC_TRIGGER_DEV_PROBE event.
 *
 * <realtime> tells if the device is realtime.
 */
static void vbstore_dev_init(unsigned int domID, const char *devname, bool realtime) {

	char rootname[VBS_KEY_LENGTH];  /* Root name depending on domID */
	char propname[VBS_KEY_LENGTH];  /* Property name depending on domID */
	char devrootname[VBS_KEY_LENGTH];
	unsigned int dir_exists = 0; /* Data used to check if a directory exists */

	DBG("%s: creating vbstore entries for domain %d and dev %s\n", __func__, domID, devname);

	/*
	 * We must check here if the /backend/% entry exists.
	 * If not, this means that there is no backend available for this virtual interface. If so, just abort.
	 */
	strcpy(devrootname, "backend/");
	strcat(devrootname, devname);
	dir_exists = vbus_directory_exists(VBT_NIL, devrootname, "");
	if (!dir_exists) {
		lprintk("Cannot find backend node: %s\n", devrootname);
		BUG();
	}

	/* Virtualized interface of dev config */
	sprintf(propname, "%d", domID);
	vbs_me_mkdir("device", propname);

	strcpy(devrootname, "device/%d");

	sprintf(rootname, devrootname, domID);
	vbs_me_mkdir(rootname, devname);

	strcat(devrootname, "/");
	strcat(devrootname, devname);

	sprintf(rootname, devrootname, domID);   /* "/device/%d/vuart"  */
	vbs_me_mkdir(rootname, "0");

	strcat(devrootname, "/0");    /*  "/device/%d/vuart/0"   */

	sprintf(rootname, devrootname, domID);
	vbs_me_write(rootname, "state", "1");  /* = VBusStateInitialising */

	switch (realtime) {

	case true:
		vbs_me_write(rootname, "realtime", "1");
		break;

	case false:
		vbs_me_write(rootname, "realtime", "0");
	}

	vbs_me_write(rootname, "backend-id", "0");

	strcpy(devrootname, "backend/");
	strcat(devrootname, devname);
	strcat(devrootname, "/%d/0");    /* "backend/vuart/%d/0" */

	sprintf(propname, devrootname, domID);
	vbs_me_write(rootname, "backend", propname);

	/* Create the vbstore entries for the backend side */

	sprintf(propname, "%d", domID);
	strcpy(devrootname, "backend/");
	strcat(devrootname, devname);  /* "/backend/vuart"  */

	vbs_me_mkdir(devrootname, propname);

	strcat(devrootname, "/%d");   /* "/backend/vuart/%d" */

	sprintf(rootname, devrootname, domID);
	vbs_me_mkdir(rootname, "0");

	strcat(devrootname, "/0");   /* "/backend/vuart/%d/state/1" */
	sprintf(rootname, devrootname, domID);
	vbs_me_write(rootname, "state", "1");

	switch (realtime) {

	case true:
		vbs_me_write(rootname, "realtime", "1");

		/* The two next entries are used to synchronize RT and non-RT vbus/vbstore. */
		vbs_me_write(rootname, "sync_backfront", "0");

		vbs_me_write(rootname, "sync_backfront_rt", "0");

		break;

	case false:
		vbs_me_write(rootname, "realtime", "0");
	}

	strcpy(devrootname, "device/%d/");
	strcat(devrootname, devname);
	strcat(devrootname, "/0");  /* "device/%d/vuart/0" */

	sprintf(propname, devrootname, domID);
	vbs_me_write(rootname, "frontend", propname);

	sprintf(propname, "%d", domID);
	vbs_me_write(rootname, "frontend-id", propname);

	/* Now, we create the corresponding device on the frontend side */
	strcpy(devrootname, "device/%d/");
	strcat(devrootname, devname);
	strcat(devrootname, "/0");  /* "device/%d/vuart/0" */
	sprintf(propname, devrootname, domID);

	/* Create device structure and gets ready to begin interactions with the backend */
	vdev_probe(propname);
}

static void vbstore_dev_remove(unsigned int domID, const char *devname) {

	char propname[VBS_KEY_LENGTH];  /* Property name depending on domID */
	char devrootname[VBS_KEY_LENGTH];
	unsigned int dir_exists = 0; /* Data used to check if a directory exists */

	DBG("%s: removing vbstore entries for domain %d\n", __func__, domID);

	/*
	 * We must check here if the /backend/% entry exists.
	 * If not, this means that there is no backend available for this virtual interface. If so, just abort.
	 */
	strcpy(devrootname, "backend/");
	strcat(devrootname, devname);
	dir_exists = vbus_directory_exists(VBT_NIL, devrootname, "");
	if (!dir_exists) {
		DBG0("Cannot find backend node: %s\n", devrootname);
		BUG();
	}


	/* Remove virtualized interface of vuart config */
	sprintf(propname, "%d/", domID);

	strcpy(devrootname, "device/");
	strcat(devrootname, propname);
	strcat(devrootname, devname); /* "/device/vuart/" */

	vbs_me_rmdir(devrootname, "0");

	/* Remove the vbstore entries for the backend side */

	sprintf(propname, "/%d", domID);

	strcpy(devrootname, "backend/");
	strcat(devrootname, devname);   /* "/backend/vuart/" */
	strcat(devrootname, propname);  /* "/backend/vuart/2" */

	vbs_me_rmdir(devrootname, "0");
}

/*
 * Remove all vbstore entries related to this ME.
 */
void remove_vbstore_entries(void) {
	int fdt_node;

	fdt_node = fdt_find_compatible_node("vleds,frontend");
	if (fdt_device_is_available(fdt_node)) {
		DBG("%s: Removing vLEDS from vbstore...\n", __func__);
		vbstore_dev_remove(ME_domID(), "vleds");
	}

	fdt_node = fdt_find_compatible_node("vuihandler,frontend");
	if (fdt_device_is_available(fdt_node)) {
		DBG("%s: Removing vUIHandler from vbstore...\n", __func__);
		vbstore_dev_remove(ME_domID(), "vuihandler");
	}

	fdt_node = fdt_find_compatible_node("vuart,frontend");
	if (fdt_device_is_available(fdt_node)) {
		DBG("%s: removing vuart from vbstore...\n", __func__);
		vbstore_dev_remove(ME_domID(), "vuart");
	}

	fdt_node = fdt_find_compatible_node("vweather,frontend");
	if (fdt_device_is_available(fdt_node)) {
		DBG("%s: removing vweather from vbstore...\n", __func__);
		vbstore_dev_remove(ME_domID(), "vweather");
	}

	fdt_node = fdt_find_compatible_node("vdoga12v6nm,frontend");
	if (fdt_device_is_available(fdt_node)) {
		DBG("%s: removing vdoga12v6nm from vbstore...\n", __func__);
		vbstore_dev_remove(ME_domID(), "vdoga12v6nm");
	}

	fdt_node = fdt_find_compatible_node("vdummy,frontend");
	if (fdt_device_is_available(fdt_node)) {
		DBG("%s: removing vdummy from vbstore...\n", __func__);
		vbstore_dev_remove(ME_domID(), "vdummy");
	}
}

/*
 * Populate vbstore with all necessary properties required by the frontend drivers.
 */
void vbstore_devices_populate(void) {
	int fdt_node;

	DBG0("Populate vbstore\n");

	fdt_node = fdt_find_compatible_node("vdummy,frontend");
	if (fdt_device_is_available(fdt_node)) {
		DBG("%s: init vdummy...\n", __func__);
		vbstore_dev_init(ME_domID(), "vdummy", false);
	}

	fdt_node = fdt_find_compatible_node("vleds,frontend");
	if (fdt_device_is_available(fdt_node)) {
		DBG("%s: Init vLEDS...\n", __func__);
		vbstore_dev_init(ME_domID(), "vleds", false);
	}

	fdt_node = fdt_find_compatible_node("vuihandler,frontend");
	if (fdt_device_is_available(fdt_node)) {
		DBG("%s: Init vUIHandler...\n", __func__);
		vbstore_dev_init(ME_domID(), "vuihandler", false);
	}

	fdt_node = fdt_find_compatible_node("vuart,frontend");
	if (fdt_device_is_available(fdt_node)) {
		DBG("%s: init vuart...\n", __func__);
		vbstore_dev_init(ME_domID(), "vuart", false);
	}

	fdt_node = fdt_find_compatible_node("vfb,frontend");
	if (fdt_device_is_available(fdt_node)) {
		DBG("%s: init vfb...\n", __func__);
		vbstore_dev_init(ME_domID(), "vfb", false);
	}

	fdt_node = fdt_find_compatible_node("vweather,frontend");
	if (fdt_device_is_available(fdt_node)) {
		DBG("%s: init vweather...\n", __func__);
		vbstore_dev_init(ME_domID(), "vweather", false);
	}

	fdt_node = fdt_find_compatible_node("vdoga12v6nm,frontend");
	if (fdt_device_is_available(fdt_node)) {
		DBG("%s: init vdoga12v6nm...\n", __func__);
		vbstore_dev_init(ME_domID(), "vdoga12v6nm", false);
	}

}

void vbstore_trigger_dev_probe(void) {
	DBG("%s: sending DC_TRIGGER_DEV_PROBE to the agency...\n", __func__);

	/* Trigger the probe on the backend side. */
	do_sync_dom(DOMID_AGENCY, DC_TRIGGER_DEV_PROBE);
}

/*
 * Prepare the vbstore entries used by this ME.
 */
void vbstore_me_init(void) {

	vbus_probe_frontend_init();

	/* Regarding MEs, avz_start_info->store_mfn is mapped statically at 0xffe01000
	 * in trap_init() earlier in the startup sequence */

	__intf = (vbstore_intf_t *) HYPERVISOR_VBSTORE_VADDR;

	/* Initialize the interface to vbstore. */

	vbus_vbstore_init();

}

void vbstore_init_dev_populate(void) {
	DBG0("Starting vbstore populating...\n");

	/*
	 * Now, the ME requests to be paused by setting its state to ME_state_preparing. As a consequence,
	 * the agency will pause it.
	 */
	set_ME_state(ME_state_preparing);

	/*
	 * There are two scenarios.
	 * 1. Classical injection scheme: Wait for the agency to perform the pause+unpause. It should set the ME
	 * state to ME_state_booting to allow the ME to continue.
	 * 2. ME that has migrated on a Smart Object: The ME state is ME_state_migrating, so it is different from
	 * ME_state_preparing.
	 */

	while (1) {
		schedule();

		if (get_ME_state() != ME_state_preparing) {
			DBG("ME state changed: %d, continuing...\n", get_ME_state());
			break;
		}
	}

	DBG0("Now ready to register vbstore entries\n");

	/* Now, we are ready to register vbstore entries */
	vbstore_devices_populate();

	if (get_ME_state() == ME_state_booting) {
		/*
		 * If the ME is in ME_state_booting state, this means that the ME has been locally injected.
		 * There is no need to adjust the grant tables. The TRIGGER_DEV_PROBE event can be sent.
		 */
		vbstore_trigger_dev_probe();
	}
}



