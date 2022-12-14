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

#ifndef SOO_H
#define SOO_H

#include <types.h>
#include <string.h>
#include <list.h>

#include <asm/atomic.h>

#include <soo/uapi/avz.h>

#define MAX_ME_DOMAINS				5

/* We include the (non-RT & RT) agency domain */
#define MAX_DOMAINS	(2 + MAX_ME_DOMAINS)

/*
 * Directcomm event management
 */
typedef enum {
	DC_NO_EVENT,
	DC_PRE_SUSPEND,
	DC_SUSPEND,
	DC_RESUME,
	DC_FORCE_TERMINATE,
	DC_POST_ACTIVATE,
	DC_TRIGGER_DEV_PROBE,

	DC_EVENT_MAX			/* Used to determine the number of DC events */
} dc_event_t;

extern atomic_t dc_outgoing_domID[DC_EVENT_MAX];
extern atomic_t dc_incoming_domID[DC_EVENT_MAX];

void set_pfn_offset(long pfn_offset);
long get_pfn_offset(void);

int get_ME_state(void);
void set_ME_state(ME_state_t state);

/* Keep information about slot availability
 * FREE:	the slot is available (no ME)
 * BUSY:	the slot is allocated a ME
 */
typedef enum {
	ME_SLOT_FREE,
	ME_SLOT_BUSY
} ME_slotState_t;

#define SOO_NAME_SIZE				16

extern uint64_t my_agencyUID;

ME_desc_t *get_ME_desc(void);

/* Struct embedding the version numbers of three main the agency components*/
typedef struct {
    unsigned int itb;
    unsigned int uboot;
    unsigned int rootfs;
} upgrade_versions_args_t;

/*
 * Device Capabilities (Devcaps)
 *
 * The agency holds a table of devcaps (device capabilities).

 * A device capability is a 32-bit number.
 *
 * DEVCAPS are organized in classes and attributes. For each devcaps, the 8 first higher bits are the
 * class number while attributes are encoded in the 24 lower bits.
 *
 * A devcap class represents a global functionality while devcap attributes are the *real* devcaps belongig to a specific class.
 *
 */

#define DEVCAPS_CLASS_FRAMEBUFFER	0x01000000
#define DEVCAP_FRAMEBUFFER_FB0		(1 << 0)

#define DEVCAPS_CLASS_INPUT		0x02000000
#define DEVCAP_INPUT_EVENT		(1 << 0)
#define DEVCAP_REMOTE_TABLET		(1 << 1)

#define DEVCAPS_CLASS_COMM		0x03000000
#define DEVCAP_COMM_UIHANDLER		(1 << 0)

#define DEVCAPS_CLASS_LED		0x04000000
#define DEVCAP_LED_RGB_SHIELD		(1 << 0)
#define DEVCAP_LED_6LED			(1 << 1)

#define DEVCAPS_CLASS_NET		0x05000000

#define DEVCAPS_CLASS_DOMOTICS		0x06000000
#define DEVCAP_BLIND_MOTOR		(1 << 0)
#define DEVCAP_WEATHER_DATA		(1 << 1)

/*
 * This devcap class is intended to be replaced by a generic framebuffer devcap in a near future.
 */
#define DEVCAPS_CLASS_APP		0x07000000
#define DEVCAP_APP_BLIND		(1 << 0)
#define DEVCAP_APP_OUTDOOR		(1 << 1)

#define DEVCAPS_CLASS_NR		16

/* struct agency_tx_args used in IOCTLs */
typedef struct agency_tx_args {
	void	*buffer; /* IN/OUT */
	int	ME_slotID;
	int	value;   /* IN/OUT */
} agency_tx_args_t;

/*
 * SOO hypercall management
 */

typedef struct {
	unsigned int	domID;
	dc_event_t	dc_event;
	int state;
} soo_hyp_dc_event_t;


typedef struct {
	unsigned int	pid;
	unsigned int	addr;
} dump_page_t;


/* This part is shared between the kernel and user spaces */

/*
 * ME uevent-type events
 */

#define ME_EVENT_NR			6

#define ME_FORCE_TERMINATE		0
#define ME_PRE_SUSPEND			1
#define ME_PRE_RESUME			2
#define ME_POST_ACTIVATE		3
#define ME_IMEC_SETUP_PEER		4


/* Hypercall commands */
#define AVZ_MIG_PRE_PROPAGATE		0
#define AVZ_MIG_PRE_ACTIVATE		1
#define AVZ_MIG_INIT			2
#define AVZ_MIG_PUT_ME_INFO		3
#define AVZ_GET_ME_FREE_SLOT		4
#define AVZ_MIG_PUT_ME_SLOT		5
#define AVZ_MIG_READ_MIGRATION_STRUCT	6
#define AVZ_MIG_WRITE_MIGRATION_STRUCT	7
#define AVZ_MIG_FINAL			8
#define AVZ_INJECT_ME			9
#define AVZ_KILL_ME			10
#define AVZ_DC_SET			11
#define AVZ_DC_RELEASE			12
#define AVZ_GET_ME_STATE		13
#define AVZ_SET_ME_STATE		14
#define AVZ_AGENCY_CTL			15
#define AVZ_GET_DOM_DESC		16

/*
 * General structure to use with the SOO migration hypercall
 */
typedef struct migrate_op {
	int		cmd;
	unsigned long	vaddr;
	unsigned long	paddr;
	void		*p_val1;
	void		*p_val2;
} soo_hyp_t;

/*
 * SOO domcalls management
 */

typedef struct {
	void *val;
} pre_activate_args_t;

/*
 * pre_propagate to tell the agency if the ME must be propagated or not.
 */
#define PROPAGATE_STATUS_YES 	1
#define PROPAGATE_STATUS_NO	0

typedef struct {
	int propagate_status;
} pre_propagate_args_t;

/* Cooperate roles */
#define COOPERATE_INITIATOR	0x1
#define COOPERATE_TARGET	0x2

typedef struct {
	uint32_t slotID;
	uint64_t spid;
	spad_t spad;
	addr_t pfn;
} coop_t;

typedef struct {

	int role; /* Specific to each ME (see drivers/soo/soo_core.c */

	bool alone; /* true if there is no ME in this SOO */

	union {
		coop_t target_coop; /* In terms of ME domains */
		coop_t initiator_coop;
	} u;

} cooperate_args_t;

typedef struct {
	void *val;
} pre_suspend_args_t;

typedef struct {
	void *val;
} pre_resume_args_t;

typedef struct {
	void *val;
} post_activate_args_t;

/*
 * Further agency ctl commands that may be used by MEs.
 * !! WARNING !! Must be strictly identical to the definitions in linux/soo/include/soo/uapi/soo.h
 */

#define AG_AGENCY_UPGRADE	0x10
#define AG_INJECT_ME		0x11
#define AG_KILL_ME		0x12
#define AG_COOPERATE		0x13

#define AG_AGENCY_UID		0x20
#define AG_SOO_NAME		0x21

#define AG_CHECK_DEVCAPS_CLASS	0x30
#define AG_CHECK_DEVCAPS	0x31

/* AG_SKIP_ACTIVATION args */

typedef struct {
	unsigned int delay;  /* at the moment, 0 means definitively, otherwise during a certain time (unit to be defined) */
	void *pfn; /* pfn of the ME's data buffer */
} skip_activation_args_t;

typedef struct {
	uint32_t	class;
	uint8_t		devcaps;
	bool		supported;   /* OUT */
} devcaps_args_t;

typedef struct {
	char	soo_name[SOO_NAME_SIZE];
} soo_name_args_t;

typedef struct {
    uint32_t buffer_pfn;
    unsigned long buffer_len;
} agency_upgrade_args_t;

/* agency_ctl args */

typedef struct {
	unsigned int slotID;
	unsigned int cmd;

	union {
		coop_t cooperate_args;
		uint64_t agencyUID;
		devcaps_args_t devcaps_args;
		soo_name_args_t soo_name_args;
		agency_upgrade_args_t agency_upgrade_args;
	} u;

} agency_ctl_args_t;

void agency_ctl(agency_ctl_args_t *agency_ctl_args);

/*
 * SOO callback functions.
 * The following definitions are used as argument in domcalls or in the
 * agency_ctl() function as a callback to be propagated to a specific ME.
 *
 */

#define CB_PRE_PROPAGATE	1
#define CB_PRE_ACTIVATE		2
#define CB_COOPERATE		3
#define CB_PRE_SUSPEND		4
#define CB_PRE_RESUME		5
#define CB_POST_ACTIVATE	6
#define CB_DUMP_BACKTRACE	7
#define CB_DUMP_VBSTORE		8
#define CB_AGENCY_CTL		9
#define CB_KILL_ME		10

typedef struct soo_domcall_arg {

	/* Stores the agency ctl function.
	 * Possibly, the agency_ctl function can be associated to a callback operation asked by a ME
	 */
	unsigned int			cmd;
	unsigned int			slotID; /* Origin of the domcall */

	union {
		pre_propagate_args_t	pre_propagate_args;
		pre_activate_args_t	pre_activate_args;
		cooperate_args_t	cooperate_args;

		pre_suspend_args_t	pre_suspend_args;
		pre_resume_args_t	pre_resume_args;

		post_activate_args_t	post_activate_args;
		ME_state_t		set_me_state_args;

		agency_ctl_args_t	agency_ctl_args;
	} u;

	/* Reference to the agency_ctl() function implemented in AVZ.
	 * Used for some function calls initiated by the ME. In this context,
	 * this function can be considered as a short-path-hypercall.
	 */
	void (*__agency_ctl)(agency_ctl_args_t *);

} soo_domcall_arg_t;

/*
 * Entry for a list of uevent which are propagated to the user space
 */
typedef struct {
	struct list_head list;
	unsigned int uevent_type;
	unsigned int slotID;
} soo_uevent_t;

typedef struct {
	bool		pending;
	unsigned int	uevent_type;
	unsigned int	slotID;
} pending_uevent_request_t;

void soo_hypercall(int cmd, void *vaddr, void *paddr, void *p_val1, void *p_val2);

int cb_pre_propagate(soo_domcall_arg_t *args);

/* Specific ME callbacks issued from the Agency */
int cb_pre_activate(soo_domcall_arg_t *args);

/* Callbacks initiated by agency ping */
int cb_pre_resume(soo_domcall_arg_t *args);
int cb_pre_suspend(soo_domcall_arg_t *args);

int cb_cooperate(soo_domcall_arg_t *args);
int cb_post_activate(soo_domcall_arg_t *args);
int cb_kill_me(soo_domcall_arg_t *args);

int cb_force_terminate(void);

void callbacks_init(void);

void set_dc_event(unsigned domID, dc_event_t dc_event);

int do_soo_activity(void *arg);

void soo_guest_activity_init(void);

void dc_stable(int dc_event);
void tell_dc_stable(int dc_event);

void do_sync_dom(int slotID, dc_event_t);
void do_async_dom(int slotID, dc_event_t);

void perform_task(dc_event_t dc_event);

void shutdown_ME(unsigned int ME_slotID);

/* ME ID management */
const char *get_me_shortdesc(void);
const char *get_me_name(void);
u64 get_spid(void);

void vbstore_ME_ID_populate(void);


#endif /* SOO_H */
