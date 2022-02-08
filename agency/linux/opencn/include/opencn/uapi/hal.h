/*
 * Copyright (C) 2019-2020 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2019-2020 Peter Lichard <peter.lichard@heig-vd.ch>
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

#ifndef UAPI_HAL_H
#define UAPI_HAL_H

#include <opencn/uapi/rtapi.h>

/* The hal enums are arranged as distinct powers-of-two, so
 that accidental confusion of one type with another (which ought
 to be diagnosed by the type system) can be diagnosed as unexpected
 values.  Note how HAL_RW is an exception to the powers-of-two rule,
 as it is the bitwise OR of HAL_RO and the (nonexistent and nonsensical)
 HAL_WO param direction.
 */

/** HAL pins and signals are typed, and the HAL only allows pins
 to be attached to signals of the same type.
 All HAL types can be read or written atomically.  (Read-modify-
 write operations are not atomic.)
 Note that when a component reads or writes one of its pins, it
 is actually reading or writing the signal linked to that pin, by
 way of the pointer.
 'hal_type_t' is an enum used to identify the type of a pin, signal,
 or parameter.
 */
typedef enum {
	HAL_TYPE_UNSPECIFIED = -1, HAL_BIT = 1, HAL_FLOAT = 2, HAL_S32 = 3, HAL_U32 = 4
} hal_type_t;

const char* hal_type_to_str(hal_type_t type);

/** HAL pins have a direction attribute.  A pin may be an input to
 the HAL component, an output, or it may be bidirectional.
 Any number of HAL_IN or HAL_IO pins may be connected to the same
 signal, but only one HAL_OUT pin is permitted.  This is equivalent
 to connecting two output pins together in an electronic circuit.
 (HAL_IO pins can be thought of as tri-state outputs.)
 */

typedef enum {
	HAL_DIR_UNSPECIFIED = -1, HAL_IN = 16, HAL_OUT = 32, HAL_IO = (HAL_IN | HAL_OUT)
} hal_pin_dir_t;

/** HAL parameters also have a direction attribute.  For parameters,
 the attribute determines whether the user can write the value
 of the parameter, or simply read it.  HAL_RO parameters are
 read-only, and HAL_RW ones are writable with 'halcmd setp'.
 */

typedef enum {
	HAL_RO = 64, HAL_RW = HAL_RO | 128 /* HAL_WO */
} hal_param_dir_t;

#define HAL_NAME_LEN     	47	/* length for pin, signal, etc, names */

#define HAL_DEV_NAME	"/dev/opencn/hal/0"
#define HAL_DEV_MAJOR	101

/* HAL references. Currently only one minor is managed. */
#define HAL_DEV_MINOR	0

/* Size of the shared mem dedicated to the HAL (__core_hal_user). */
#define HAL_SIZE  (85*4096)

#define HAL_MAX_LINK_PINS 4
#define HAL_MAX_SHOW_PATTERNS 4

#if 0
#define OFILENAME (__FILE__ + OSOURCE_PATH_SIZE)
#ifndef __KERNEL__
#define ODBG(expr) fprintf(stderr, "ODGB %s@%s|%d: %s\n", \
                __FUNCTION__, __FILE__, __LINE__, #expr); expr

#else
#define ODBG(expr) lprintk("ODBG %s@%s|%d: %s\n", \
                __FUNCTION__, __FILE__, __LINE__, #expr); expr
#endif
#else
#define ODBG(expr) expr
#endif

/*
 * IOCTL codes
 */
#define HAL_IOCTL_SETP			_IOW(0x05000000, 0, char)
#define HAL_IOCTL_NET			_IOW(0x05000000, 1, char)
#define HAL_IOCTL_LINKPS		_IOW(0x05000000, 2, char)
#define HAL_IOCTL_SHOW			_IOW(0x05000000, 3, unsigned long)
#define HAL_IOCTL_LIST			_IOW(0x05000000, 4, char)
#define HAL_IOCTL_STATUS		_IOW(0x05000000, 5, char)
#define HAL_IOCTL_ADDF			_IOW(0x05000000, 6, char)
#define HAL_IOCTL_START			_IOW(0x05000000, 7, char)
#define HAL_IOCTL_GET_SHMEM_ADDR	_IOW(0x05000000, 8, char)
#define HAL_IOCTL_ANON_PIN_NEW		_IOW(0x05000000, 9, char)
#define HAL_IOCTL_ANON_PIN_DELETE	_IOW(0x05000000, 10, char)
#define HAL_IOCTL_STOP			_IOW(0x05000000, 11, char)

typedef struct {
	char name[HAL_NAME_LEN];
	char value[HAL_NAME_LEN];
} hal_setp_args_t;

typedef struct {
	char signal[HAL_NAME_LEN];
	char *pins[HAL_MAX_LINK_PINS];
	char pins_data[HAL_NAME_LEN*HAL_MAX_LINK_PINS];
} hal_net_args_t;

typedef struct {
	char pin[HAL_NAME_LEN];
	char sig[HAL_NAME_LEN];
} hal_linkps_args_t;

typedef struct {
	char type[HAL_NAME_LEN];
	char *patterns[HAL_MAX_SHOW_PATTERNS];
	char patterns_data[HAL_MAX_SHOW_PATTERNS*HAL_NAME_LEN];
} hal_show_args_t;

typedef struct {
	char type[HAL_NAME_LEN];
	char *patterns[HAL_MAX_SHOW_PATTERNS];
	char patterns_data[HAL_MAX_SHOW_PATTERNS*HAL_NAME_LEN];
} hal_list_args_t;

typedef struct {
	char type[HAL_NAME_LEN];
} hal_status_args_t;

typedef struct {
	char func[HAL_NAME_LEN];
	char thread[HAL_NAME_LEN];
	int position;
} hal_addf_args_t;

typedef struct {
	const char *name;
	hal_type_t type;
	hal_pin_dir_t dir;
} hal_anon_pin_args_t;

/* Use these for x86 machines, and anything else that can write to
 individual bytes in a machine word. */

typedef volatile bool hal_bit_t;
typedef volatile rtapi_u32 hal_u32_t;
typedef volatile rtapi_s32 hal_s32_t;
typedef double real_t __attribute__((aligned(8)));
typedef rtapi_u64 ireal_t __attribute__((aligned(8))); /* integral type as wide as real_t / hal_float_t */
#define hal_float_t volatile real_t

/** HAL "data union" structure
 ** This structure may hold any type of hal data
*/
typedef union {
    hal_bit_t b;
    hal_s32_t s;
    hal_u32_t u;
    hal_float_t f;
} hal_data_u;

/** HAL "oldname" data structure.
    When a pin or parameter gets an alias, this structure is used to
    store the original name.
*/
typedef struct {
    rtapi_intptr_t next_ptr;		/* next struct (used for free list only) */
    char name[HAL_NAME_LEN + 1];	/* the original name */
} hal_oldname_t;

/** HAL 'pin' data structure.
    This structure contains information about a 'pin' object.
*/
typedef struct {
    rtapi_intptr_t next_ptr;		/* next pin in linked list */
    int data_ptr_addr;		/* address of pin data pointer */
    int owner_ptr;		/* component that owns this pin */
    int signal;			/* signal to which pin is linked */
    hal_data_u dummysig;	/* if unlinked, data_ptr points here */
    int oldname;		/* old name if aliased, else zero */
    hal_type_t type;		/* data type */
    hal_pin_dir_t dir;		/* pin direction */
    char name[HAL_NAME_LEN + 1];	/* pin name */
} hal_pin_t;

#endif /* UAPI_HAL_H */

