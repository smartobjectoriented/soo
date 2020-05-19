
/*
 * Copyright (C) 2004 Tim Deegan and Andrew Warfield
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef __RING_H__
#define __RING_H__

typedef unsigned int RING_IDX;

/* Round a 32-bit unsigned constant down to the nearest power of two. */
#define __RD2(_x)  (((_x) & 0x00000002) ? 0x2		       : ((_x) & 0x1))
#define __RD4(_x)  (((_x) & 0x0000000c) ? __RD2((_x)>>2)<<2    : __RD2(_x))
#define __RD8(_x)  (((_x) & 0x000000f0) ? __RD4((_x)>>4)<<4    : __RD4(_x))
#define __RD16(_x) (((_x) & 0x0000ff00) ? __RD8((_x)>>8)<<8    : __RD8(_x))
#define __RD32(_x) (((_x) & 0xffff0000) ? __RD16((_x)>>16)<<16 : __RD16(_x))

/*
 * Calculate size of a shared ring, given the total available space for the
 * ring and indexes (_sz), and the name tag of the request/response structure.
 * A ring contains as many entries as will fit, rounded down to the nearest
 * power of two (so we can mask with (size-1) to loop around).
 */
#define __CONST_RING_SIZE(_s, _sz)				\
	(__RD32(((_sz) - offsetof(struct _s##_sring, ring)) /	\
		sizeof(((struct _s##_sring *)0)->ring[0])))

/*
 * The same for passing in an actual pointer instead of a name tag.
 */
#define __RING_SIZE(_s, _sz)						\
	(__RD32(((_sz) - (long)&(_s)->ring + (long)(_s)) / sizeof((_s)->ring[0])))

/*
 * Macros to make the correct C datatypes for a new kind of ring.
 *
 * To make a new ring datatype, you need to have two message structures,
 * let's say request_t, and response_t already defined.
 *
 * In a header where you want the ring datatype declared, you then do:
 *
 *     DEFINE_RING_TYPES(mytag, request_t, response_t);
 *
 * These expand out to give you a set of types, as you can see below.
 * The most important of these are:
 * 
 *     mytag_sring_t      - The shared ring (found in mytag_front_ring_t/mytag_back_ring_t)
 *     mytag_front_ring_t - The 'front' half of the ring.
 *     mytag_back_ring_t  - The 'back' half of the ring.
 *
 * To initialize a ring in your code you need to know the location and size
 * of the shared memory area (PAGE_SIZE, for instance). To initialise
 * the front half:
 *
 *     mytag_front_ring_t front_ring;
 *     SHARED_RING_INIT((mytag_sring_t *)shared_page);
 *     FRONT_RING_INIT(&front_ring, (mytag_sring_t *)shared_page, PAGE_SIZE);
 *
 * Initializing the back follows similarly (note that only the front
 * initializes the shared ring):
 *
 *     mytag_back_ring_t back_ring;
 *     BACK_RING_INIT(&back_ring, (mytag_sring_t *)shared_page, PAGE_SIZE);
 */

#define DEFINE_RING_TYPES(__name, __req_t, __rsp_t)                     \
                                                                        \
/* Shared ring entry */                                                 \
union __name##_sring_entry {                                            \
    __req_t req;                                                        \
    __rsp_t rsp;                                                        \
};                                                                      \
                                                                        \
/* Shared ring page */                                                  \
struct __name##_sring {                                                 \
    RING_IDX req_prod, req_cons, req_event; 							\
    RING_IDX rsp_prod, rsp_cons, rsp_event;                             \
    uint8_t  pad[40];                                                   \
    union __name##_sring_entry ring[1]; /* variable-length */           \
};                                                                      \
                                                                        \
/* "Front" end's private variables */                                   \
struct __name##_front_ring {                                            \
    RING_IDX req_prod_pvt;                                              \
                                                   \
    unsigned int nr_ents;                                               \
    struct __name##_sring *sring;                                       \
};                                                                      \
                                                                        \
/* "Back" end's private variables */                                    \
struct __name##_back_ring {                                             \
    RING_IDX rsp_prod_pvt;                                              \
                                                     \
    unsigned int nr_ents;                                               \
    struct __name##_sring *sring;                                       \
};                                                                      \
                                                                        \
/* Syntactic sugar */                                                   \
typedef struct __name##_sring __name##_sring_t;                         \
typedef struct __name##_front_ring __name##_front_ring_t;               \
typedef struct __name##_back_ring __name##_back_ring_t

/*
 * Macros for manipulating rings.
 * 
 * FRONT_RING_whatever works on the "front end" of a ring: here 
 * requests are pushed on to the ring and responses taken off it.
 * 
 * BACK_RING_whatever works on the "back end" of a ring: here 
 * requests are taken off the ring and responses put on.
 * 
 * N.B. these macros do NO INTERLOCKS OR FLOW CONTROL. 
 * This is OK in 1-for-1 request-response situations where the 
 * requestor (front end) never has more than RING_SIZE()-1
 * outstanding requests.
 */

/* Initialising empty rings */
#define SHARED_RING_INIT(_s) do {                                       \
	(_s)->req_prod  = (_s)->rsp_prod  = 0;                              \
	(_s)->req_cons  = (_s)->rsp_cons  = 0;                              \
    (_s)->req_event = (_s)->rsp_event = 1;                              \
    (void)memset((_s)->pad, 0, sizeof((_s)->pad));                      \
} while(0)

#define FRONT_RING_INIT(_r, _s, __size) do {                            \
    (_r)->req_prod_pvt = 0;                                             \
    (_r)->nr_ents = __RING_SIZE(_s, __size);                            \
    (_r)->sring = (_s);                                                 \
} while (0)

#define BACK_RING_INIT(_r, _s, __size) do {                             \
    (_r)->rsp_prod_pvt = 0;                                             \
    (_r)->nr_ents = __RING_SIZE(_s, __size);                            \
    (_r)->sring = (_s);                                                 \
} while (0)

/* How big is this ring? */
#define RING_SIZE(_r)                                                   \
    ((_r)->nr_ents)

/* Number of free requests (for use on front side only
 * with non 1-to-1 (non injective) communication). */
#define RING_FREE_REQUESTS(_r)                            \
    (RING_SIZE(_r) - ((_r)->req_prod_pvt - (_r)->sring->req_cons))


/* Test if there is an empty slot available on the front ring.
 * (This is only meaningful from the front. )
 */
#define RING_FULL(_r)                                                   \
    (RING_FREE_REQUESTS(_r) == 0)

#define RING_REQ_FULL(_r)                                                   \
    (RING_FREE_REQUESTS(_r) == 0)

/* Test if there are outstanding messages to be processed on a ring. */
#define RING_HAS_UNCONSUMED_RESPONSES(_r)                               \
    ((_r)->sring->rsp_prod - (_r)->sring->rsp_cons)

#define RING_HAS_UNCONSUMED_REQUESTS(_r)                               \
		 ((_r)->sring->req_prod - (_r)->sring->req_cons)

/* Direct access to individual ring elements, by index. */
#define RING_GET_REQUEST(_r, _idx)                                      \
    (&((_r)->sring->ring[((_idx) & (RING_SIZE(_r) - 1))].req))

#define RING_GET_RESPONSE(_r, _idx)                                     \
    (&((_r)->sring->ring[((_idx) & (RING_SIZE(_r) - 1))].rsp))

#define RING_PUSH_REQUESTS(_r) do {                                     \
    dmb(); /* back sees requests /before/ updated producer index */     \
    (_r)->sring->req_prod = (_r)->req_prod_pvt;                         \
} while (0)

#define RING_PUSH_RESPONSES(_r) do {                                    \
    dmb(); /* front sees responses /before/ updated producer index */   \
    (_r)->sring->rsp_prod = (_r)->rsp_prod_pvt;                         \
} while (0)

#define RING_FREE_RESPONSES(_r)                            \
    (RING_SIZE(_r) - ((_r)->rsp_prod_pvt - (_r)->sring->rsp_cons))

#define RING_RESP_FULL(_r)                                                   \
    (RING_FREE_RESPONSES(_r) == 0)

/*
 * Notification hold-off (req_event and rsp_event):
 * 
 * When queueing requests or responses on a shared ring, it may not always be
 * necessary to notify the remote end. For example, if requests are in flight
 * in a backend, the front may be able to queue further requests without
 * notifying the back (if the back checks for new requests when it queues
 * responses).
 * 
 * When enqueuing requests or responses:
 * 
 *  Use RING_PUSH_{REQUESTS,RESPONSES}_AND_CHECK_NOTIFY(). The second argument
 *  is a boolean return value. True indicates that the receiver requires an
 *  asynchronous notification.
 * 
 * After dequeuing requests or responses (before sleeping the connection):
 * 
 *  Use RING_FINAL_CHECK_FOR_REQUESTS() or RING_FINAL_CHECK_FOR_RESPONSES().
 *  The second argument is a boolean return value. True indicates that there
 *  are pending messages on the ring (i.e., the connection should not be put
 *  to sleep).
 * 
 *  These macros will set the req_event/rsp_event field to trigger a
 *  notification on the very next message that is enqueued. If you want to
 *  create batches of work (i.e., only receive a notification after several
 *  messages have been enqueued) then you will need to create a customised
 *  version of the FINAL_CHECK macro in your own code, which sets the event
 *  field appropriately.
 */

#define RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(_r, _notify) do {           \
    RING_IDX __old = (_r)->sring->req_prod;                             \
    RING_IDX __new = (_r)->req_prod_pvt;                                \
    dmb(); /* back sees requests /before/ updated producer index */     \
    (_r)->sring->req_prod = __new;                                      \
    dmb(); /* back sees new requests /before/ we check req_event */      \
    (_notify) = ((RING_IDX)(__new - (_r)->sring->req_event) <           \
                 (RING_IDX)(__new - __old));                            \
} while (0)

#define RING_PUSH_RESPONSES_AND_CHECK_NOTIFY(_r, _notify) do {          \
    RING_IDX __old = (_r)->sring->rsp_prod;                             \
    RING_IDX __new = (_r)->rsp_prod_pvt;                                \
    dmb(); /* front sees responses /before/ updated producer index */   \
    (_r)->sring->rsp_prod = __new;                                      \
    dmb(); /* front sees new responses /before/ we check rsp_event */    \
    (_notify) = ((RING_IDX)(__new - (_r)->sring->rsp_event) <           \
                 (RING_IDX)(__new - __old));                            \
} while (0)

#define RING_FINAL_CHECK_FOR_REQUESTS(_r, _work_to_do) do {             \
    (_work_to_do) = RING_HAS_UNCONSUMED_REQUESTS(_r);                   \
    if (_work_to_do) break;                                             \
    (_r)->sring->req_event = (_r)->sring->req_cons + 1;                        \
    dmb();                                                               \
    (_work_to_do) = RING_HAS_UNCONSUMED_REQUESTS(_r);                   \
} while (0)

#define RING_FINAL_CHECK_FOR_RESPONSES(_r, _work_to_do) do {            \
    (_work_to_do) = RING_HAS_UNCONSUMED_RESPONSES(_r);                  \
    if (_work_to_do) break;                                             \
    (_r)->sring->rsp_event = (_r)->sring->rsp_cons + 1;                        \
    dmb();                                                               \
    (_work_to_do) = RING_HAS_UNCONSUMED_RESPONSES(_r);                  \
} while (0)

#endif /* __RING_H__ */
