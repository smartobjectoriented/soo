/*
 * Copyright (C) 2014-2025 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <stdint.h>

#define MAX_ME_DOMAINS	5

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

/*
 * ME descriptor
 *
 * WARNING !! Be careful when modifying this structure. It *MUST* be aligned with
 * the same structure used in the ME.
 */
typedef struct {
	unsigned int	slotID;
        uint64_t        spid;

	ME_state_t	state;

	unsigned int	size; /* Size of the ME */
	unsigned int	pfn;

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

/*
 * IOCTL commands for migration.
 * This part is shared between the kernel and user spaces.
 */

/*
 * IOCTL codes
 */
#define AGENCY_IOCTL_INIT_MIGRATION		_IOWR('S', 0, agency_ioctl_args_t)
#define AGENCY_IOCTL_GET_ME_FREE_SLOT		_IOWR('S', 1, agency_ioctl_args_t)
#define AGENCY_IOCTL_READ_SNAPSHOT		_IOWR('S', 2, agency_ioctl_args_t)
#define AGENCY_IOCTL_WRITE_SNAPSHOT		_IOW('S', 3, agency_ioctl_args_t)
#define AGENCY_IOCTL_FINAL_MIGRATION    	_IOW('S', 4, agency_ioctl_args_t)
#define AGENCY_IOCTL_FORCE_TERMINATE		_IOW('S', 5, agency_ioctl_args_t)
#define AGENCY_IOCTL_INJECT_ME			_IOWR('S', 6, agency_ioctl_args_t)
#define AGENCY_IOCTL_GET_ME_ID			_IOWR('S', 7, agency_ioctl_args_t)
#define AGENCY_IOCTL_GET_ME_SNAPSHOT		_IOWR('S', 10, agency_ioctl_args_t)
#define AGENCY_IOCTL_GET_ME_ID_ARRAY		_IOR('S', 11, agency_ioctl_args_t)
#define AGENCY_IOCTL_BLACKLIST_SOO		_IOW('S', 12, agency_ioctl_args_t)

/* struct agency_ioctl_args used in IOCTLs */
typedef struct agency_ioctl_args {
	void	*buffer; /* IN/OUT */
	int	slotID;
	long	value;   /* IN/OUT */
} agency_ioctl_args_t;

#endif /* UAPI_SOO_H */
