/*
 * Copyright (C) 2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/uaccess.h>
#include <linux/slab.h>

#include <soo/vbus.h>
#include <soo/roxml.h>

#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/me_access.h>


int get_ME_state(unsigned int ME_slotID)
{
	int rc;
	int val;

	val = ME_slotID;

	rc = soo_hypercall(AVZ_GET_ME_STATE, NULL, NULL, &val, NULL);
	if (rc != 0) {
		printk("%s: failed to get the ME state from the hypervisor (%d)\n", __func__, rc);
		return rc;
	}

	return val;
}

/*
 * Setting the ME state to the specific ME_slotID.
 * The hypercall args is passed by 2 contiguous (unsigned) int, the first one is
 * used for slotID, the second for the state
 */
int set_ME_state(unsigned int ME_slotID, ME_state_t state)
{
	int rc;
	int _state[2];

	_state[0] = ME_slotID;
	_state[1] = state;

	rc = soo_hypercall(AVZ_SET_ME_STATE, NULL, NULL, _state, NULL);
	if (rc != 0) {
		printk("%s: failed to set the ME state from the hypervisor (%d)\n", __func__, rc);
		return rc;
	}

	return rc;
}

/**
 * Retrieve the ME descriptor including the SPID, the state and the SPAD.
 */
void get_ME_desc(unsigned int slotID, ME_desc_t *ME_desc) {
	int rc;
	dom_desc_t dom_desc;

	rc = soo_hypercall(AVZ_GET_DOM_DESC, NULL, NULL, &slotID, &dom_desc);
	if (rc != 0) {
		printk("%s: failed to retrieve the SOO descriptor for slot ID %d.\n", __func__, rc);
		BUG();
	}

	memcpy(ME_desc, &dom_desc.u.ME, sizeof(ME_desc_t));
}

/*
 * Retrieve the SPID of a ME.
 *
 * Return 0 if success.
 */
void get_ME_spid(unsigned int slotID, unsigned char *spid) {
	ME_desc_t ME_desc;

	get_ME_desc(slotID, &ME_desc);
	memcpy(spid, ME_desc.spid, SPID_SIZE);
}

/**
 * Get an available ME slot from the hypervisor for a ME with a specific size (<size>).
 * If no slot is available, the value field of the agency_tx_args_t structure will be set to -1.
 */
int ioctl_get_ME_free_slot(unsigned long arg) {
	int rc;
	agency_tx_args_t args;
	int val;

	if ((rc = copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		return rc;
	}

	val = args.value;

	DBG("Agency: trying to get a slot for a ME of %d bytes ...\n", val);

	if ((rc = soo_hypercall(AVZ_GET_ME_FREE_SLOT, NULL, NULL, &val, NULL)) != 0) {
		lprintk("Agency: %s:%d Failed to get ME slot from hypervisor (%d)\n", __func__, __LINE__, rc);
		return rc;
	}

	args.ME_slotID = val;

	if ((rc = copy_to_user((void *) arg, (const void *) &args, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to set args into userspace\n", __func__, __LINE__);
		return rc;
	}

	if (val == -1) {
		DBG0("Agency: no slot available anymore ...");
	} else {
		DBG("Agency: ME slot ID %d available.\n", val);
	}

	return 0;
}

/**
 * Retrieve the ME descriptor including the SPID, the state and the SPAD.
 */
int ioctl_get_ME_desc(unsigned long arg) {
	int rc;
	agency_tx_args_t args;
	ME_desc_t ME_desc;

	if ((rc = copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	DBG("ME_slotID=%d\n", args.ME_slotID);

	get_ME_desc(args.ME_slotID, &ME_desc);

	if ((rc = copy_to_user(args.buffer, &ME_desc, sizeof(ME_desc_t))) != 0) {
		lprintk("Agency: %s:%d Failed to set args into userspace\n", __func__, __LINE__);
		BUG();
	}

	if ((rc = copy_to_user((void *) arg, &args, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to set args into userspace\n", __func__, __LINE__);
		BUG();
	}

	return 0;
}

/**
 * Get a list of all residing MEs in this smart object.
 * The caller must have allocated an array of <MAX_ME_DOMAINS> <ME_id_t> elements.
 *
 * The ME state will be used to determine if a ME is residing or not (ME_state_dead).
 */
void get_ME_id_array(ME_id_t *ME_id_array) {
	uint32_t slotID;
	char *prop;
	char rootname[VBS_KEY_LENGTH];
	unsigned int len;

	/* Walk through all entries in vbstore regarding MEs */

	for (slotID = 2; slotID < MAX_DOMAINS; slotID++) {

		sprintf(rootname, "soo/me/%d", slotID);

		/* Check if there is a ME? */
		prop = vbus_read(VBT_NIL, rootname, "spid", &len);

		if (len == 1)  { /* If no entry in vbstore, it returns 1 (byte \0) */
			ME_id_array[slotID-2].state = ME_state_dead;
		} else {
			sscanf(prop, "%llx", &ME_id_array[slotID-2].spid);
			kfree(prop);

			ME_id_array[slotID-2].state = get_ME_state(slotID);
			prop = vbus_read(VBT_NIL, rootname, "name", &len);

			strcpy(ME_id_array[slotID-2].name, prop);
			kfree(prop);

			prop = vbus_read(VBT_NIL, rootname, "shortdesc", &len);

			strcpy(ME_id_array[slotID-2].shortdesc, prop);
			kfree(prop);
		}
	}

}
EXPORT_SYMBOL(get_ME_id_array);

/**
 * Prepare a XML message which contains the list of MEs with their specific ID information
 * (SPID, name, short desc)
 * 
 *
 * @return buffer	Buffer allocated by the which contains the XML message. Caller must free it.
 * @param ME_id_array	Array of ME_id_t entries (got with get_ME_id_array())
 */
char *xml_prepare_id_array(ME_id_t *ME_id_array) {
	uint32_t pos;
	char *__buffer;
	char *buffer; /* Output buffer */
	node_t *root, *messages, *me, *name, *shortdesc;
	char spid[SPID_SIZE];

	/* Adding attributes to xml node */
	root = roxml_add_node(NULL, 0, ROXML_ELM_NODE, "xml", NULL);
	roxml_add_node(root, 0, ROXML_ATTR_NODE, "version", "1.0");
	roxml_add_node(root, 0, ROXML_ATTR_NODE, "encoding", "UTF-8");

	/* Adding the messages node */
	messages = roxml_add_node(root, 0, ROXML_ELM_NODE, "mobile-entities", NULL);

	for (pos = 0; pos < MAX_ME_DOMAINS; pos++) {

		if (ME_id_array[pos].state != ME_state_dead) {

			/* Adding the message itself */
			me = roxml_add_node(messages, 0, ROXML_ELM_NODE, "mobile-entity", NULL);

			/* Add SPID */
			sprintf(spid, "%llx", ME_id_array[pos].spid);
			roxml_add_node(me, 0, ROXML_ATTR_NODE, "spid", spid);

			/* Add short name */
			name = roxml_add_node(me, 0, ROXML_ELM_NODE, "name", NULL);
			roxml_add_node(name, 0, ROXML_TXT_NODE, NULL, ME_id_array[pos].name);

			/* And the short description */
			shortdesc = roxml_add_node(me, 0, ROXML_ELM_NODE, "description", NULL);
			roxml_add_node(shortdesc, 0, ROXML_TXT_NODE, NULL, ME_id_array[pos].shortdesc);
		}

	}

	roxml_commit_changes(root, NULL, &__buffer, 1);

	/* Allocate the buffer here, as the caller has no way to determine it */
	buffer = (char *) kzalloc(strlen(__buffer), GFP_KERNEL);
	if (buffer == NULL) {
		return NULL;
	}

	strcpy(buffer, __buffer);

	roxml_release(RELEASE_LAST);
	roxml_close(root);

	return buffer;
}
EXPORT_SYMBOL(xml_prepare_id_array);

