/*
 *  linux/include/asm-arm/byteorder.h
 *
 * ARM Endian-ness.  In little endian mode, the data bus is connected such
 * that byte accesses appear as:
 *  0 = d0...d7, 1 = d8...d15, 2 = d16...d23, 3 = d24...d31
 * and word accesses (data or instruction) appear as:
 *  d0...d31
 *
 * When in big endian mode, byte accesses appear as:
 *  0 = d24...d31, 1 = d16...d23, 2 = d8...d15, 3 = d0...d7
 * and word accesses (data or instruction) appear as:
 *  d0...d31
 */
#ifndef __ASM_ARM_BYTEORDER_H
#define __ASM_ARM_BYTEORDER_H

#include <avz/compiler.h>

#include <asm/types.h>

/*
 * Byte-swapping, independently from CPU endianness
 *     swabXX[ps]?(foo)
 *
 * Francois-Rene Rideau <fare@tunes.org> 19971205
 *    separated swab functions from cpu_to_XX,
 *    to clean up support for bizarre-endian architectures.
 */

/* casts are necessary for constants, because we never know how for sure
 * how U/UL/ULL map to __u16, __u32, __u64. At least not in a portable way.
 */
#define ___swab16(x)                                    \
({                                                      \
    __u16 __x = (x);                                    \
    ((__u16)(                                           \
        (((__u16)(__x) & (__u16)0x00ffU) << 8) |        \
        (((__u16)(__x) & (__u16)0xff00U) >> 8) ));      \
})

#define ___swab32(x)                                            \
({                                                              \
    __u32 __x = (x);                                            \
    ((__u32)(                                                   \
        (((__u32)(__x) & (__u32)0x000000ffUL) << 24) |          \
        (((__u32)(__x) & (__u32)0x0000ff00UL) <<  8) |          \
        (((__u32)(__x) & (__u32)0x00ff0000UL) >>  8) |          \
        (((__u32)(__x) & (__u32)0xff000000UL) >> 24) ));        \
})

#define ___swab64(x)                                                       \
({                                                                         \
    __u64 __x = (x);                                                       \
    ((__u64)(                                                              \
        (__u64)(((__u64)(__x) & (__u64)0x00000000000000ffULL) << 56) |     \
        (__u64)(((__u64)(__x) & (__u64)0x000000000000ff00ULL) << 40) |     \
        (__u64)(((__u64)(__x) & (__u64)0x0000000000ff0000ULL) << 24) |     \
        (__u64)(((__u64)(__x) & (__u64)0x00000000ff000000ULL) <<  8) |     \
            (__u64)(((__u64)(__x) & (__u64)0x000000ff00000000ULL) >>  8) | \
        (__u64)(((__u64)(__x) & (__u64)0x0000ff0000000000ULL) >> 24) |     \
        (__u64)(((__u64)(__x) & (__u64)0x00ff000000000000ULL) >> 40) |     \
        (__u64)(((__u64)(__x) & (__u64)0xff00000000000000ULL) >> 56) ));   \
})

#define ___constant_swab16(x)                   \
    ((__u16)(                                   \
        (((__u16)(x) & (__u16)0x00ffU) << 8) |  \
        (((__u16)(x) & (__u16)0xff00U) >> 8) ))
#define ___constant_swab32(x)                           \
    ((__u32)(                                           \
        (((__u32)(x) & (__u32)0x000000ffUL) << 24) |    \
        (((__u32)(x) & (__u32)0x0000ff00UL) <<  8) |    \
        (((__u32)(x) & (__u32)0x00ff0000UL) >>  8) |    \
        (((__u32)(x) & (__u32)0xff000000UL) >> 24) ))
#define ___constant_swab64(x)                                            \
    ((__u64)(                                                            \
        (__u64)(((__u64)(x) & (__u64)0x00000000000000ffULL) << 56) |     \
        (__u64)(((__u64)(x) & (__u64)0x000000000000ff00ULL) << 40) |     \
        (__u64)(((__u64)(x) & (__u64)0x0000000000ff0000ULL) << 24) |     \
        (__u64)(((__u64)(x) & (__u64)0x00000000ff000000ULL) <<  8) |     \
            (__u64)(((__u64)(x) & (__u64)0x000000ff00000000ULL) >>  8) | \
        (__u64)(((__u64)(x) & (__u64)0x0000ff0000000000ULL) >> 24) |     \
        (__u64)(((__u64)(x) & (__u64)0x00ff000000000000ULL) >> 40) |     \
        (__u64)(((__u64)(x) & (__u64)0xff00000000000000ULL) >> 56) ))

/*
 * provide defaults when no architecture-specific optimization is detected
 */

#  define __arch__swab16(x) ({ __u16 __tmp = (x) ; ___swab16(__tmp); })
#  define __arch__swab32(x) ({ __u32 __tmp = (x) ; ___swab32(__tmp); })
#  define __arch__swab64(x) ({ __u64 __tmp = (x) ; ___swab64(__tmp); })

#  define __arch__swab16p(x) __arch__swab16(*(x))
#  define __arch__swab32p(x) __arch__swab32(*(x))
#  define __arch__swab64p(x) __arch__swab64(*(x))

#  define __arch__swab16s(x) do { *(x) = __arch__swab16p((x)); } while (0)

#  define __arch__swab32s(x) do { *(x) = __arch__swab32p((x)); } while (0)


#  define __arch__swab64s(x) do { *(x) = __arch__swab64p((x)); } while (0)



/*
 * Allow constant folding
 */
#if defined(__GNUC__) && defined(__OPTIMIZE__)
#  define __swab16(x) \
(__builtin_constant_p((__u16)(x)) ? \
 ___swab16((x)) : \
 __fswab16((x)))
#  define __swab32(x) \
(__builtin_constant_p((__u32)(x)) ? \
 ___swab32((x)) : \
 __fswab32((x)))
#  define __swab64(x) \
(__builtin_constant_p((__u64)(x)) ? \
 ___swab64((x)) : \
 __fswab64((x)))
#else
#  define __swab16(x) __fswab16(x)
#  define __swab32(x) __fswab32(x)
#  define __swab64(x) __fswab64(x)
#endif /* OPTIMIZE */


static inline __attribute_const__ __u64 __fswab64(__u64 x)
{
#  ifdef __SWAB_64_THRU_32__
    __u32 h = x >> 32;
        __u32 l = x & ((1ULL<<32)-1);
        return (((__u64)__swab32(l)) << 32) | ((__u64)(__swab32(h)));
#  else
    return __arch__swab64(x);
#  endif
}
static inline __u64 __swab64p(const __u64 *x)
{
    return __arch__swab64p(x);
}
static inline void __swab64s(__u64 *addr)
{
    __arch__swab64s(addr);
}


#define swab16 __swab16
#define swab32 __swab32
#define swab64 __swab64
#define swab16p __swab16p
#define swab32p __swab32p
#define swab64p __swab64p
#define swab16s __swab16s
#define swab32s __swab32s
#define swab64s __swab64s
#include <avz/byteorder/little_endian.h>

#endif

