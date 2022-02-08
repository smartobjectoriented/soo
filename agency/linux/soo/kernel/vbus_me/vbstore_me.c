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

#if 1
#define DEBUG
#endif

#include <linux/of.h>

#include <soo/hypervisor.h>
#include <soo/vbstore.h>
#include <soo/evtchn.h>
#include <soo/vbstore_me.h>
#include <soo/vbus_me.h>
#include <soo/guest_api.h>

#include <soo/uapi/soo.h>
#include <soo/uapi/avz.h>
#include <soo/uapi/debug.h>

static void vbs_rmdir(const char *dir, const char *node) {
	vbus_rm(VBT_NIL, dir, node);
}

static void vbs_mkdir(const char *dir, const char *node) {
	vbus_mkdir(VBT_NIL, dir, node);
}

static void vbs_write(const char *dir, const char *node, const char *string) {
	vbus_write(VBT_NIL, dir, node, string);
}

/*
 * The following vbstore node creation does not require to be within a transaction
 * since the backend has no visibility on these nodes until it gets the DC_TRIGGER_DEV_PROBE event.
 *
 */
static void vbstore_dev_init(unsigned int domID, const char *devname, const char *compat) {

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
	vbs_mkdir("device", propname);

	strcpy(devrootname, "device/%d");

	sprintf(rootname, devrootname, domID);
	vbs_mkdir(rootname, devname);

	strcat(devrootname, "/");
	strcat(devrootname, devname);

	sprintf(rootname, devrootname, domID);   /* "/device/%d/vuart"  */
	vbs_mkdir(rootname, "0");

	strcat(devrootname, "/0");    /*  "/device/%d/vuart/0"   */

	sprintf(rootname, devrootname, domID);
	vbs_write(rootname, "state", "1");  /* = VBusStateInitialising */

	vbs_write(rootname, "backend-id", "0");

	strcpy(devrootname, "backend/");
	strcat(devrootname, devname);
	strcat(devrootname, "/%d/0");    /* "backend/vuart/%d/0" */

	sprintf(propname, devrootname, domID);
	vbs_write(rootname, "backend", propname);

	/* Create the vbstore entries for the backend side */

	sprintf(propname, "%d", domID);
	strcpy(devrootname, "backend/");
	strcat(devrootname, devname);  /* "/backend/vuart"  */

	vbs_mkdir(devrootname, propname);

	strcat(devrootname, "/%d");   /* "/backend/vuart/%d" */

	sprintf(rootname, devrootname, domID);
	vbs_mkdir(rootname, "0");

	strcat(devrootname, "/0");   /* "/backend/vuart/%d/state/1" */
	sprintf(rootname, devrootname, domID);
	vbs_write(rootname, "state", "1");

	strcpy(devrootname, "device/%d/");
	strcat(devrootname, devname);
	strcat(devrootname, "/0");  /* "device/%d/vuart/0" */

	sprintf(propname, devrootname, domID);
	vbs_write(rootname, "frontend", propname);

	sprintf(propname, "%d", domID);
	vbs_write(rootname, "frontend-id", propname);

	/* Now, we create the corresponding device on the frontend side */
	strcpy(devrootname, "device/%d/");
	strcat(devrootname, devname);
	strcat(devrootname, "/0");  /* "device/%d/vuart/0" */
	sprintf(propname, devrootname, domID);

	/* Create device structure and gets ready to begin interactions with the backend */
	vdev_me_probe(propname, compat);
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
		DBG("Cannot find backend node: %s\n", devrootname);
		BUG();
	}


	/* Remove virtualized interface of vuart config */
	sprintf(propname, "%d/", domID);

	strcpy(devrootname, "device/");
	strcat(devrootname, propname);
	strcat(devrootname, devname); /* "/device/vuart/" */

	vbs_rmdir(devrootname, "0");

	/* Remove the vbstore entries for the backend side */

	sprintf(propname, "/%d", domID);

	strcpy(devrootname, "backend/");
	strcat(devrootname, devname);   /* "/backend/vuart/" */
	strcat(devrootname, propname);  /* "/backend/vuart/2" */

	vbs_rmdir(devrootname, "0");
}

/*
 * Remove all vbstore entries related to this ME.
 */
void remove_vbstore_entries(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vdummy,frontend");
	if (of_device_is_available(np)) {
		DBG("%s: init vdummy...\n", __func__);
		vbstore_dev_remove(AGENCY_RT_CPU, "vdummy");
	}

	np = of_find_compatible_node(NULL, NULL, "vuart,frontend");
	if (of_device_is_available(np)) {
		DBG("%s: init vuart...\n", __func__);
		vbstore_dev_remove(AGENCY_RT_CPU, "vuart");
	}
}

/*
 * Populate vbstore with all necessary properties required by the frontend drivers.
 */
void vbstore_devices_populate(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vdummy,frontend");
	if (of_device_is_available(np)) {
		DBG("%s: init vdummy...\n", __func__);
		vbstore_dev_init(AGENCY_RT_CPU, "vdummy", "vdummy,frontend");
	}

	np = of_find_compatible_node(NULL, NULL, "vuart,frontend");
	if (of_device_is_available(np)) {
		DBG("%s: init vuart...\n", __func__);
		vbstore_dev_init(AGENCY_RT_CPU, "vuart", "vuart,frontend");
	}
}

void vbstore_trigger_dev_probe(void) {
	DBG("%s: sending DC_TRIGGER_DEV_PROBE to the agency...\n", __func__);

	/* Trigger the probe on the backend side. */
	do_sync_dom(DOMID_AGENCY, DC_TRIGGER_DEV_PROBE);
}

void vbstore_init_dev_populate(dc_event_t dc_event) {

	DBG("Now ready to register vbstore entries\n");

	/* We also create the gnttab entry which is necessary to handle gnttab ref
	 * required for the vbstore exchange.
	 */

	gnttab_me_init();

	DBG("Starting vbstore populating...\n");

	/* Now, we are ready to register vbstore entries */
	vbstore_devices_populate();

	/* The function was already called along a DC_EVENT from the non-RT domain. */

	tell_dc_stable(DC_TRIGGER_DEV_PROBE);
}

/*
 * Called in the non-RT domain
 */
static int vbstore_me_init(void) {

	DBG("%s: sending DC_DEV_POPULATE to the RT domain...\n", __func__);

	rtdm_register_dc_event_callback(DC_TRIGGER_DEV_PROBE, vbstore_init_dev_populate);

	/* Trigger the probe on the backend side. */
	do_sync_dom(DOMID_AGENCY_RT, DC_TRIGGER_DEV_PROBE);

	/* Pursue with initialization on backend side */
	vbus_probe_backend(DOMID_AGENCY_RT);


	return 0;
}


late_initcall(vbstore_me_init);

