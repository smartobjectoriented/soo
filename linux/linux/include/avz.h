#ifndef AVZ_H
#define AVZ_H

/*
 * AVZ HYPERCALLS
 */

#define __HYPERVISOR_event_channel_op      0
#define __HYPERVISOR_console_io            1
#define __HYPERVISOR_physdev_op            2
#define __HYPERVISOR_sched_op              3
#define __HYPERVISOR_domctl                4
#define __HYPERVISOR_soo_hypercall         5

#define NR_EVTCHN 128

/*
 * Commands to HYPERVISOR_console_io().
 */
#define CONSOLEIO_write_string  0
#define CONSOLEIO_process_char  1

#ifndef __ASSEMBLY__

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

typedef struct {

	/* Indicate if the ME accepts to collaborate with other ME */
	bool valid;

	/* SPAD capabilities */
	uint64_t spadcaps;

} spad_t;

typedef struct {
	unsigned int	slotID;

	ME_state_t	state;

	unsigned int	size; /* Size of the ME */
	unsigned int	pfn;

	uint64_t	spid; /* Species ID */
	spad_t		spad; /* ME Species Aptitude Descriptor */
} ME_desc_t;

typedef struct {

	/*
	 * SOO agencyUID unique 64-bit ID - Allowing to identify a SOO device.
	 * agencyUID 0 is NOT valid.
	 */

	uint64_t agencyUID; /* Agency UID */

} agency_desc_t;

typedef struct {
	union {
		agency_desc_t agency;
		ME_desc_t ME;
	} u;
} dom_desc_t;

extern void hypercall_trampoline(int hcall, long a0, long a1, long a2, long a3);

typedef uint16_t domid_t;

extern long avz_hypercall(int hcall, long a0, long a2, long a3, long a4);

typedef unsigned long addr_t;

/*
 * Shared info page, shared between AVZ and the domain.
 */
struct avz_shared {

	domid_t domID;

	/* Domain related information */
	unsigned long nr_pages;     /* Total pages allocated to this domain.  */

	/* Hypercall vector addr for direct branching without syscall */
	addr_t hypercall_vaddr;

	/* Interrupt routine in the domain */
	addr_t vectors_vaddr;

	/* Domcall routine in the domain */
	addr_t domcall_vaddr;

	addr_t fdt_paddr;

	/* Low-level print function mainly for debugging purpose */
	void (*printch)(char c);

	/* VBstore pfn */
	unsigned long vbstore_pfn;

	unsigned long dom_phys_offset;

	/* Physical and virtual address of the page table used when the domain is bootstraping */
	addr_t pagetable_paddr;
	addr_t pagetable_vaddr; /* Required when bootstrapping the domain */

	/* Address of the logbool ht_set function which can be used in the domain. */
	unsigned long logbool_ht_set_addr;

	/* We inform the domain about the hypervisor memory region so that the
	 * domain can re-map correctly.
	 */
	addr_t hypervisor_vaddr;

	/* Other fields related to domain life */

	unsigned long domain_stack;
	uint8_t evtchn_upcall_pending;

	/*
	 * A domain can create "event channels" on which it can send and receive
	 * asynchronous event notifications.
	 * Each event channel is assigned a bit in evtchn_pending and its modification has to be
	 * kept atomic.
	 */

	volatile bool evtchn_pending[NR_EVTCHN];

	atomic_t dc_event;

	/* Agency or ME descriptor */
	dom_desc_t dom_desc;

	/* Keep the physical address so that the guest can map within in its address space. */
	addr_t subdomain_shared_paddr;

	struct avz_shared *subdomain_shared;

	/* Reference to the logbool hashtable (one per each domain) */
	void *logbool_ht;

	/* Used to store a signature for consistency checking, for example after a migration/restoration */
	char signature[4];
};

typedef struct avz_shared avz_shared_t;

extern volatile avz_shared_t *avz_shared;

#define AVZ_shared ((smp_processor_id() == 1) ? (avz_shared)->subdomain_shared : avz_shared)

#define AVZ_primary_shared ((avz_shared_t *) avz_shared)

int avz_switch_console(char ch);

#endif /* !__ASSEMBLY__ */

#endif /* AVZ_H */
