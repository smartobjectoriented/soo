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

int get_ME_state(unsigned int ME_slotID)
{
        avz_hyp_t args;

        args.cmd = AVZ_GET_ME_STATE;
        args.u.avz_me_state_args.slotID = ME_slotID;

        avz_hypercall(&args);

        return args.u.avz_me_state_args.state;
}

/*
 * Setting the ME state to the specific ME_slotID.
 * The hypercall args is passed by 2 contiguous (unsigned) int, the first one is
 * used for slotID, the second for the state
 */
void set_ME_state(unsigned int ME_slotID, ME_state_t state)
{
        avz_hyp_t args;

        args.cmd = AVZ_SET_ME_STATE;

        args.u.avz_me_state_args.slotID = ME_slotID;
	args.u.avz_me_state_args.state = state;

        avz_hypercall(&args);
}

/**
 * Retrieve the ME descriptor including the SPID, the state.
 */
void get_ME_desc(unsigned int slotID, ME_desc_t *ME_desc) {
        avz_hyp_t args;

        args.cmd = AVZ_GET_DOM_DESC;
        args.u.avz_dom_desc_args.slotID = slotID;

        avz_hypercall(&args);

	memcpy(ME_desc, &args.u.avz_dom_desc_args.dom_desc, sizeof(ME_desc_t));
}

/**
 * Get an available ME slot from the hypervisor for a ME with a specific size (<size>).
 *
 * @param size which is required
 * @return slotID or -1  if no slot available.
 */
int32_t get_ME_free_slot(uint32_t size) {
        avz_hyp_t args;

	DBG("Agency: trying to get a slot for a ME of %d bytes ...\n", val);

        args.cmd = AVZ_GET_ME_FREE_SLOT;
        args.u.avz_free_slot_args.size = size;

        avz_hypercall(&args);

	if (args.u.avz_free_slot_args.slotID == -1)
		DBG0("Agency: no slot available anymore ...");
	else
		DBG("Agency: ME slot ID %d available.\n", args.u.avz_free_slot_args.slotID);

	return args.u.avz_free_slot_args.slotID;
}

/**
 * Retrieve the ME identity information including SPID, state.
 *
 * @param slotID
 * @param ME_id
 * @return true if a ME has been found in slotID, false otherwise.
 */
bool get_ME_id(uint32_t slotID, ME_id_t *ME_id) {
	char *prop;
	char rootname[VBS_KEY_LENGTH];
	unsigned int len;

	/* The ME can be dormant but without any accessible ID information */
	ME_id->state = get_ME_state(slotID);

	if (ME_id->state == ME_state_dead)
		return false;

	sprintf(rootname, "soo/me/%d", slotID);

	/* Check if there is a ME? */
	prop = vbus_read(VBT_NIL, rootname, "spid", &len);

	if (len == 1)  { /* If no entry in vbstore, it returns 1 (byte \0) */
		return true;
	} else {
		sscanf(prop, "%llx", &ME_id->spid);
		kfree(prop);
 
		prop = vbus_read(VBT_NIL, rootname, "name", &len);

		strcpy(ME_id->name, prop);
		kfree(prop);

		prop = vbus_read(VBT_NIL, rootname, "shortdesc", &len);

		strcpy(ME_id->shortdesc, prop);
		kfree(prop);

		ME_id->slotID = slotID;
	}

	return true;
}

/**
 * Get a list of all residing MEs in this smart object.
 * The caller must have allocated an array of <MAX_ME_DOMAINS> <ME_id_t> elements.
 *
 * The ME state will be used to determine if a ME is residing or not (ME_state_dead).
 */
void get_ME_id_array(ME_id_t *ME_id_array) {
	uint32_t slotID;

	/* Walk through all entries in vbstore regarding MEs */

	for (slotID = 2; slotID < MAX_DOMAINS; slotID++)
		get_ME_id(slotID, &ME_id_array[slotID-2]);

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
	node_t *messages, *me, *name, *shortdesc;
	char spid[17]; /* 64-bit hex string + null terminator */
	char slotID[2];

	/* Adding the messages node */
	messages = roxml_add_node(NULL, 0, ROXML_ELM_NODE, "mobile-entities", NULL);

	for (pos = 0; pos < MAX_ME_DOMAINS; pos++) {

		if (ME_id_array[pos].state != ME_state_dead) {

			/* Adding the message itself */
			me = roxml_add_node(messages, 0, ROXML_ELM_NODE, "mobile-entity", NULL);

			/* Add SPID */
			sprintf(spid, "%016llx", ME_id_array[pos].spid);
			roxml_add_node(me, 0, ROXML_ATTR_NODE, "spid", spid);

			/* Add the slotID */
			sprintf(slotID, "%d", ME_id_array[pos].slotID);
			roxml_add_node(me, 0, ROXML_ATTR_NODE, "slotID", slotID);

			/* Add short name */
			name = roxml_add_node(me, 0, ROXML_ELM_NODE, "name", NULL);
			roxml_add_node(name, 0, ROXML_TXT_NODE, NULL, ME_id_array[pos].name);

			/* And the short description */
			shortdesc = roxml_add_node(me, 0, ROXML_ELM_NODE, "description", NULL);
			roxml_add_node(shortdesc, 0, ROXML_TXT_NODE, NULL, ME_id_array[pos].shortdesc);

		}

	}

	roxml_commit_changes(messages, NULL, &__buffer, 1);

	/* Allocate the buffer here, as the caller has no way to determine it */
	buffer = (char *) kzalloc(strlen(__buffer), GFP_KERNEL);
	if (buffer == NULL) {
		return NULL;
	}

	strcpy(buffer, __buffer);

	roxml_release(RELEASE_LAST);
	roxml_close(messages);

	return buffer;
}
EXPORT_SYMBOL(xml_prepare_id_array);

