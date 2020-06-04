/*
 * Copyright (C) 2016,2017 Daniel Rossier <daniel.rossier@soo.tech>
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

#ifndef LIB_H
#define LIB_H

#include <avz/stdarg.h>
#include <avz/config.h>
#include <avz/types.h>
#include <avz/xmalloc.h>
#include <avz/string.h>

#include <asm/fault.h>
#include <asm/bug.h>

#define BUG_ON(p)  do { if (unlikely(p)) BUG();  } while (0)
#define WARN_ON(p) do { if (unlikely(p)) WARN(); } while (0)

/* Force a compilation error if condition is true */
#define BUILD_BUG_ON(condition) ((void)sizeof(struct { int:-!!(condition); }))

/* Force a compilation error if condition is true, but also produce a
   result (of value 0 and type size_t), so the expression can be used
   e.g. in a structure initializer (or where-ever else comma expressions
   aren't permitted). */
#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int:-!!(e); }))

#ifndef assert_failed
#define assert_failed(p)                                        \
do {                                                            \
    printk("Assertion '%s' failed, line %d, file %s\n", p ,     \
                   __LINE__, __FILE__);                         \
    BUG();                                                      \
} while (0)
#endif

#ifndef NDEBUG
#define ASSERT(p) \
    do { if ( unlikely(!(p)) ) assert_failed(#p); } while (0)
#else
#define ASSERT(p) ((void)0)
#endif

#define ABS(_x) ({                              \
    typeof(_x) __x = (_x);                      \
    (__x < 0) ? -__x : __x;                     \
})

#define SWAP(_a, _b) \
   do { typeof(_a) _t = (_a); (_a) = (_b); (_b) = _t; } while ( 0 )

#define DIV_ROUND(x, y) (((x) + (y) / 2) / (y))

#define DIV_ROUND_CLOSEST(x, divisor)(                  \
 {                                                       \
         typeof(x) __x = x;                              \
         typeof(divisor) __d = divisor;                  \
         (((typeof(x))-1) > 0 ||                         \
          ((typeof(divisor))-1) > 0 || (__x) > 0) ?      \
                 (((__x) + ((__d) / 2)) / (__d)) :       \
                 (((__x) - ((__d) / 2)) / (__d));        \
 }                                                       \
)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]) + __must_be_array(x))


struct domain;

/* Allows us to use '%p' as general-purpose machine-word format char. */
#define _p(_x) ((void *)(unsigned long)(_x))
extern void printk(const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));
extern void panic(const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));


/* vsprintf.c */

extern int snprintf(char * buf, size_t size, const char * fmt, ...)
    __attribute__ ((format (printf, 3, 4)));
extern int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
    __attribute__ ((format (printf, 3, 0)));
extern int scnprintf(char * buf, size_t size, const char * fmt, ...)
    __attribute__ ((format (printf, 3, 4)));
extern int vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
    __attribute__ ((format (printf, 3, 0)));
extern int sscanf(const char * buf, const char * fmt, ...)
    __attribute__ ((format (scanf, 2, 3)));
extern int vsscanf(const char * buf, const char * fmt, va_list args)
    __attribute__ ((format (scanf, 2, 0)));

long simple_strtol(
    const char *cp,const char **endp, unsigned int base);
unsigned long simple_strtoul(
    const char *cp,const char **endp, unsigned int base);
long long simple_strtoll(
    const char *cp,const char **endp, unsigned int base);
unsigned long long simple_strtoull(
    const char *cp,const char **endp, unsigned int base);

unsigned long long parse_size_and_unit(const char *s, const char **ps);

uint64_t muldiv64(uint64_t a, uint32_t b, uint32_t c);

#endif /* LIB_H */
