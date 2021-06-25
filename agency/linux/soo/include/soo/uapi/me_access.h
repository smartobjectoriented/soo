/*
 * Copyright (C) 2016-2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef ME_ACCESS_H
#define ME_ACCESS_H

#ifndef __KERNEL__
#include <stdbool.h>
#include <stdint.h>
#endif

#include <linux/types.h>

/*
 * Capabilities for the Species Aptitude Descriptor (SPAD) structure
 * The SPAD contains a table of 16 chars called "capabilities".
 * A capability refers to a functionality.
 * - The numbers correspond to the index of the char dedicated to a particular
 *   SPAD capability class in the SPAD capability table.
 * - The bit shiftings designate a particular SPAD capability.
 *
 */

#define SPAD_CAP_HEATING_CONTROL		(1 << 0)
#define SPAD_CAP_SOUND_PRESENCE_DETECTION	(1 << 1)

#define SPAD_CAP_SOUND_MIX			(1 << 2)
#define SPAD_CAP_SOUND_STREAM			(1 << 3)

#define SPAD_CAPS_SIZE				16

/*
 * Species Aptitude Descriptor (SPAD)
 */
typedef struct {

	/* Indicate if the ME accepts to collaborate with other ME */
	bool		valid;

	unsigned char	caps[SPAD_CAPS_SIZE];
} spad_t;

#define SPID_SIZE	16

/* This structure is used as the first field of the ME buffer frame header */
typedef struct {

	size_t	ME_size;
	size_t	size_mig_structure;

	unsigned int crc32;

} ME_info_transfer_t;

/*
 * ME states:
 * - ME_state_booting:		ME is currently booting...
 * - ME_state_preparing:	ME is being paused during the boot process, in the case of an injection, before the frontend initialization
 * - ME_state_living:		ME is full-functional and activated (all frontend devices are consistent)
 * - ME_state_suspended:	ME is suspended before migrating. This state is maintained for the resident ME instance
 * - ME_state_migrating:	ME just arrived in SOO
 * - ME_state_dormant:		ME is resident, but not living (running) - all frontends are closed/shutdown
 * - ME_state_killed:		ME has been killed before to be resumed
 * - ME_state_terminated:	ME has been terminated (by a force_terminate)
 * - ME_state_dead:		ME does not exist
 */
typedef enum {
	ME_state_booting,
	ME_state_preparing,
	ME_state_living,
	ME_state_suspended,
	ME_state_migrating,
	ME_state_dormant,
	ME_state_killed,
	ME_state_terminated,
	ME_state_dead
} ME_state_t;

/* Keep information about slot availability
 * FREE:	the slot is available (no ME)
 * BUSY:	the slot is allocated a ME
 */
typedef enum {
	ME_SLOT_FREE,
	ME_SLOT_BUSY
} ME_slotState_t;

/*
 * ME descriptor
 *
 * WARNING !! Be careful when modifying this structure. It *MUST* be aligned with
 * the same structure used in the ME.
 */
typedef struct {
	ME_state_t	state;

	unsigned int	slotID;
	unsigned int	size; /* Size of the ME */
	unsigned int	pfn;

	unsigned char	spid[SPID_SIZE]; /* Species ID */
	spad_t		spad; /* ME Species Aptitude Descriptor */
} ME_desc_t;

/* ME ID related information */
#define ME_NAME_SIZE				40
#define ME_SHORTDESC_SIZE			1024

/*
 * Definition of ME ID information used by functions which need
 * to get a list of running MEs with their information.
 */
typedef struct {
	uint32_t slotID;
	ME_state_t state;

	uint64_t spid;
	char name[ME_NAME_SIZE];
	char shortdesc[ME_SHORTDESC_SIZE];
} ME_id_t;

/* Fixed size for the header of the ME buffer frame (max.) */
#define ME_EXTRA_BUFFER_SIZE (1024 * 1024)

#ifdef __KERNEL__

int get_ME_state(unsigned int ME_slotID);
int set_ME_state(unsigned int ME_slotID, ME_state_t state);

int ioctl_get_ME_free_slot(unsigned long arg);
int ioctl_get_ME_desc(unsigned long arg);

void get_ME_id_array(ME_id_t *ME_id_array);
void get_ME_desc(unsigned int slotID, ME_desc_t *ME_desc);

void get_ME_spid(unsigned int slotID, unsigned char *spid);


#endif /* __KERNEL__ */

#endif /* ME_ACCESS_H */


