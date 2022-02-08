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

#ifndef SOO_H
#define SOO_H

/* We include the (non-RT & RT) agency domain */
#define MAX_DOMAINS	    2

/* To maintain the compatibility with SOO, we keep AGENCY_CPU as equal to AGENCY_CPU0 */

#define AGENCY_CPU	0
#define AGENCY_CPU0	0

#define AGENCY_RT_CPU	1

#define AGENCY_CPU2	2
#define AGENCY_CPU3	3

#ifndef __ASSEMBLY__
#ifdef __KERNEL__

/* For struct list_head */
#if !defined(__AVZ__)
#include <linux/types.h>
#include <linux/string.h>
#else
#include <list.h>
#endif

#include <asm/atomic.h>

#else /* __KERNEL__ */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef unsigned short uint16_t;

#endif /* !__KERNEL__ */

struct work_struct;
struct semaphore;

typedef uint16_t domid_t;

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
	DC_LOCALINFO_UPDATE,
	DC_TRIGGER_DEV_PROBE,

	/* OpenCN events */
	DC_VLOG_INIT,
	DC_VLOG_FLUSH,
	DC_LCEC_INIT,
	DC_EC_IOCTL_SOE_READ,

	DC_EVENT_MAX			/* Used to determine the number of DC events */
} dc_event_t;

/*
 * Callback function associated to dc_event.
 */
typedef void(dc_event_fn_t)(dc_event_t dc_event);

#ifdef __KERNEL__

extern atomic_t dc_outgoing_domID[DC_EVENT_MAX];
extern atomic_t dc_incoming_domID[DC_EVENT_MAX];

#endif /* __KERNEL__ */

#ifdef __KERNEL__

void set_pfn_offset(int pfn_offset);
int get_pfn_offset(void);

#endif /* __KERNEL__ */

/*
 * IOCTL commands for migration.
 * This part is shared between the kernel and user spaces.
 */

#define SOO_AGENCY_UID_SIZE			16

#define SOO_NAME_SIZE				16

#ifdef __KERNEL__

/*
 * SOO agencyUID unique ID - Allowing to identify a SOO device.
 * agencyUID 0 is NOT valid.
 */
typedef struct {

	/*
	 * As id is the first attribute, it can be accessed directly by using
	 * a pointer to the agencyUID_t.
	 */
	unsigned char id[SOO_AGENCY_UID_SIZE];

} agencyUID_t;

/*
 * Agency descriptor
 */
typedef struct {
	agencyUID_t agencyUID; /* Agency UID */
} agency_desc_t;


#endif /* __KERNEL__ */

/* This part is shared between the kernel and user spaces */



#ifdef __KERNEL__

/*
 * Device Capabilities (Devcaps)
 * The agency holds a table of devcaps (device capabilities).
 * A device capability refers to a peripheral.
 * DEVCAPS are organized in classes and attributes.
 * A devcap class represents a global functionality while devcap attributes are the *real* devcaps belongig to a specific class.
 *
 * - The bit shiftings designate a particular device capability.
 *
 */
#define DEVCAPS_CLASS_AUDIO		0x0100
#define DEVCAP_AUDIO_CAPTURE		(1 << 0)
#define DEVCAP_AUDIO_PLAYBACK		(1 << 1)
#define DEVCAP_AUDIO_RECORDING		(1 << 2)
#define DEVCAP_AUDIO_RT_STREAM		(1 << 3)

#define DEVCAPS_CLASS_LOCALINFO		0x0200
#define DEVCAP_LOCALINFO_BUFFER		(1 << 0)

#define DEVCAPS_CLASS_FRAMEBUFFER	0x0300
#define DEVCAP_FRAMEBUFFER_FB0		(1 << 0)

#define DEVCAPS_CLASS_INPUT		0x0400
#define DEVCAP_INPUT_EVENT		(1 << 0)
#define DEVCAP_REMOTE_TABLET		(1 << 1)

#define DEVCAPS_CLASS_COMM		0x0500
#define DEVCAP_COMM_UIHANDLER		(1 << 0)

#define DEVCAPS_CLASS_LED		0x0600
#define DEVCAP_LED_RGB_SHIELD		(1 << 0)
#define DEVCAP_LED_6LED			(1 << 1)

#define DEVCAPS_CLASS_NET		0x0700
#define DEVCAP_NET_STREAMING		(1 << 0)
#define DEVCAP_NET_MESSAGING		(1 << 1)

#define DEVCAPS_CLASS_DOMOTICS		0x0800
#define DEVCAP_BLIND_MOTOR		(1 << 0)
#define DEVCAP_WEATHER_DATA		(1 << 1)

/*
 * This devcap class is intended to be replaced by a generic framebuffer devcap in a near future.
 */
#define DEVCAPS_CLASS_APP		0x0900
#define DEVCAP_APP_BLIND		(1 << 0)
#define DEVCAP_APP_OUTDOOR		(1 << 1)

#define DEVCAPS_CLASS_NR		16

/*
 * SOO agency & ME descriptor - This structure is used in the shared info page of the agency or ME domain.
 */

typedef struct {
	union {
		agency_desc_t agency;
	} u;
} dom_desc_t;

#endif /* __KERNEL__ */


#ifdef __KERNEL__

/*
 * SOO hypercall management
 */

typedef struct {
	unsigned int	domID;
	dc_event_t	dc_event;
} soo_hyp_dc_event_t;


typedef struct {
	unsigned int	pid;
	unsigned int	addr;
} dump_page_t;


#endif /* __KERNEL__ */

/* This part is shared between the kernel and user spaces */

#ifdef __KERNEL__

#define NSECS           	1000000000ull
#define SECONDS(_s)     	((u64)((_s)  * 1000000000ull))
#define MILLISECS(_ms)  	((u64)((_ms) * 1000000ull))
#define MICROSECS(_us)  	((u64)((_us) * 1000ull))

/* Periods are expressed in ns as it is used by the function APIs */

#define SL_TX_REQUEST_TASK_PRIO		50
#define SL_SEND_TASK_PRIO		50
#define SL_RECV_TASK_PRIO		50

#define SL_PLUGIN_WLAN_TASK_PRIO	50
#define SL_PLUGIN_ETHERNET_TASK_PRIO	50
#define SL_PLUGIN_TCP_TASK_PRIO		50
#define SL_PLUGIN_BLUETOOTH_TASK_PRIO	50
#define SL_PLUGIN_LOOPBACK_TASK_PRIO	50

#define VBUS_TASK_PRIO			50

/*
 * The priority of the Directcomm thread must be higher than the priority of the SDIO
 * thread to make the Directcomm thread process a DC event and release it before any new
 * request by the SDIO's side thus avoiding a deadlock.
 */
#define DC_ISR_TASK_PRIO		55

#define SDIO_IRQ_TASK_PRIO		50
#define SDHCI_FINISH_TASK_PRIO		50

/* Soolink definition */

/* Discovery */

#define DISCOVERY_TASK_PRIO		50
#define DISCOVERY_TASK_PERIOD_MS	1000

/* Soolink Coder */

#define CODER_TASK_PRIO			50

/* Soolink Decoder */

#define DECODER_WATCHDOG_TASK_PRIO	50
#define DECODER_WATCHDOG_TASK_PERIOD_MS 1000

/* Soolink Winenet Datalink */

#define WINENET_TASK_PRIO               50

/* vNetstream virtual interface */

#define VNETSTREAM_TASK_PRIO            50

/* Tests related */
#define PLUGIN_TEST_TASK_PRIO		50
#define TRANSCODER_TEST_TASK_PRIO	50
#define DISCOVERY_TEST_TASK_PRIO	50

/* WLan driver (Marvell) */
#define MAIN_WORK_TASK_PRIO		50
#define RX_WORK_TASK_PRIO		50
#define HANG_WORK_TASK_PRIO		50
#define CSA_WORK_TASK_PRIO		50

/* BCM2835 driver (Raspberry Pi 3) */
#define DMA_COMPLETE_WORK_TASK_PRIO	50
#define FWEH_EVENT_WORK_TASK_PRIO 	50
#define SDIO_EVENT_WORK_TASK_PRIO 	50

#ifndef __ASSEMBLY__

/* Hypercall commands */
#define AVZ_DC_SET			11

/*
 * General structure to use with the SOO hypercall
 */
typedef struct {
	int		cmd;
	unsigned long	vaddr;
	unsigned long	paddr;
	void		*p_val1;
	void		*p_val2;
} soo_hyp_t;

extern volatile bool __cobalt_ready;

void rtdm_register_dc_event_callback(dc_event_t dc_event, dc_event_fn_t *callback);

#endif /* __ASSEMBLY__ */

typedef struct {
	char	soo_name[SOO_NAME_SIZE];
} soo_name_args_t;

int soo_hypercall(int cmd, void *vaddr, void *paddr, void *p_val1, void *p_val2);

void callbacks_init(void);

void set_dc_event(domid_t domid, dc_event_t dc_event);

int do_soo_activity(void *arg);

void soo_guest_activity_init(void);

void dc_stable(int dc_event);
void tell_dc_stable(int dc_event);

void do_sync_dom(int slotID, dc_event_t);
void do_async_dom(int slotID, dc_event_t);

void perform_task(dc_event_t dc_event);

void cache_flush_all(void);

#endif /* __KERNEL__ */
#endif /* __ASSEMBLY__ */

#endif /* SOO_H */
