#ifndef __TYPES_H__
#define __TYPES_H__

#include <avz/config.h>
#include <asm/types.h>

#define BITS_TO_LONGS(bits) \
    (((bits)+BITS_PER_LONG-1)/BITS_PER_LONG)
#define DECLARE_BITMAP(name,bits) \
    unsigned long name[BITS_TO_LONGS(bits)]

#ifndef NULL
#define NULL ((void*)0)
#endif

#define true					1
#define false					0

#ifdef CONFIG_PHYS_ADDR_T_64BIT
typedef u64 phys_addr_t;
#else
typedef u32 phys_addr_t;
#endif

#define INT_MAX         ((int)(~0U>>1))
#define INT_MIN         (-INT_MAX - 1)
#define UINT_MAX        (~0U)
#define LONG_MAX        ((long)(~0UL>>1))
#define LONG_MIN        (-LONG_MAX - 1)
#define ULONG_MAX       (~0UL)

/* bsd */
typedef unsigned char           u_char;
typedef unsigned short          u_short;
typedef unsigned int            u_int;
typedef unsigned long           u_long;

typedef _Bool                   bool;

/* sysv */
typedef unsigned char           unchar;
typedef unsigned short          ushort;
typedef unsigned int            uint;
typedef unsigned long           ulong;

typedef         __u8            uint8_t;
typedef         __u8            u_int8_t;
typedef         __s8            int8_t;

typedef         __u16           uint16_t;
typedef         __u16           u_int16_t;
typedef         __s16           int16_t;

/* DEB */
#if !defined(uint32_t) && defined(__UINT32_TYPE__)
typedef         __UINT32_TYPE__ uint32_t;
#endif
typedef         __u32           u_int32_t;
/* DEB */
#if !defined(int32_t) && defined(__INT32_TYPE__)
typedef         __INT32_TYPE__ int32_t;
#endif

typedef 	__u32		uintptr_t;

typedef         __u64           uint64_t;
typedef         __u64           u_int64_t;
typedef         __s64           int64_t;

typedef	long		time_t;

struct domain;
struct vcpu;

typedef __u16 __le16;
typedef __u16 __be16;
typedef __u32 __le32;
typedef __u32 __be32;
typedef __u64 __le64;
typedef __u64 __be64;

#endif /* __TYPES_H__ */
