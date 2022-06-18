/*
 * Copyright (C) 2016-2022 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef UAPI_SOO_H
#define UAPI_SOO_H

#include <types.h>

typedef uint16_t domid_t;

/*
 * ME states:
 * - ME_state_booting:		ME is currently booting...
 * - ME_state_preparing:	ME is being paused during the boot process, in the case of an injection, before the frontend initialization
 * - ME_state_living:		ME is full-functional and activated (all frontend devices are consistent)
 * - ME_state_suspended:	ME is suspended before migrating. This state is maintained for the resident ME instance
 * - ME_state_migrating:	ME just arrived in SOO
 * - ME_state_dormant:		ME is resident, but not living (running)
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

/*
 * Agency descriptor
 */
typedef struct {

	/*
	 * SOO agencyUID unique ID - Allowing to identify a SOO device.
	 * agencyUID 0 is NOT valid.
	 */

	uint64_t agencyUID; /* Agency UID */

} agency_desc_t;

/* This part is shared between the kernel and user spaces */

/*
 * Capabilities for the Species Aptitude Descriptor (SPAD) structure
 *
 */

#define SPADCAP_HEATING_CONTROL		(1 << 0)

/*
 * Species Aptitude Descriptor (SPAD)
 */
typedef struct {
	bool valid; /* True means that the ME accepts to collaborate with other ME */
	uint64_t spadcaps;
} spad_t;

/*
 * ME descriptor
 *
 * WARNING !! Be careful when modifying this structure. It *MUST* be aligned with
 * the same structure used in AVZ and Agency.
 */
typedef struct {
	unsigned int	slotID;

	ME_state_t	state;

	unsigned int	size; /* Size of the ME */
	unsigned int	pfn;

	uint64_t	spid; /* Species ID */
	spad_t		spad; /* ME Species Aptitude Descriptor */
} ME_desc_t;

/*
 * SOO agency & ME descriptor - This structure is used in the shared info page of the agency or ME domain.
 */

typedef struct {
	union {
		agency_desc_t agency;
		ME_desc_t ME;
	} u;
} dom_desc_t;

#endif /* UAPI_SOO_H */
