
#ifndef UAPI_RTAPI_H
#define UAPI_RTAPI_H

#include <stdarg.h>

#if defined(__KERNEL__)
#include <linux/types.h>
#elif !defined(__cplusplus)

/* a note in gcc's stdbool.h says "supporting <stdbool.h> in C++ is a GCC extension" */

#include <stdbool.h>
#endif

#define HAL_RTAPI_LOG_PIN 	"hal.rtapi_msg.enable"

#ifdef __KERNEL__
#include <asm/types.h>
typedef s8 rtapi_s8;
typedef s16 rtapi_s16;
typedef s32 rtapi_s32;
typedef s64 rtapi_s64;
typedef long rtapi_intptr_t;
typedef u8 rtapi_u8;
typedef u16 rtapi_u16;
typedef u32 rtapi_u32;
typedef u64 rtapi_u64;
typedef unsigned long rtapi_uintptr_t;

#define RTAPI_INT8_MAX (127)
#define RTAPI_INT8_MIN (-128)
#define RTAPI_UINT8_MAX (255)

#define RTAPI_INT16_MAX (32767)
#define RTAPI_INT16_MIN (-32768)
#define RTAPI_UINT16_MAX (65535)

#define RTAPI_INT32_MAX (2147483647)
#define RTAPI_INT32_MIN (-2147483647-1)
#define RTAPI_UINT32_MAX (4294967295ul)

#define RTAPI_INT64_MAX (9223372036854775807)
#define RTAPI_INT64_MIN (-9223372036854775807-1)
#define RTAPI_UINT64_MAX (18446744073709551615ull)
#else
#include <inttypes.h>

typedef int8_t rtapi_s8;
typedef int16_t rtapi_s16;
typedef int32_t rtapi_s32;
typedef int64_t rtapi_s64;
typedef intptr_t rtapi_intptr_t;
typedef uint8_t rtapi_u8;
typedef uint16_t rtapi_u16;
typedef uint32_t rtapi_u32;
typedef uint64_t rtapi_u64;
typedef uintptr_t rtapi_uintptr_t;

#define RTAPI_INT8_MAX INT8_MAX
#define RTAPI_INT8_MIN INT8_MIN
#define RTAPI_UINT8_MAX UINT8_MAX

#define RTAPI_INT16_MAX INT16_MAX
#define RTAPI_INT16_MIN INT16_MIN
#define RTAPI_UINT16_MAX UINT16_MAX

#define RTAPI_INT32_MAX INT32_MAX
#define RTAPI_INT32_MIN INT32_MIN
#define RTAPI_UINT32_MAX UINT32_MAX

#define RTAPI_INT64_MAX INT64_MAX
#define RTAPI_INT64_MIN INT64_MIN
#define RTAPI_UINT64_MAX UINT64_MAX
#endif

/** 'rtapi_print_msg()' prints a printf-style message when the level
 is less than or equal to the current message level set by
 rtapi_set_msg_level().  May be called from user, init/cleanup,
 and realtime code.
 */
typedef enum {
	RTAPI_MSG_NONE = 0, RTAPI_MSG_ERR, RTAPI_MSG_WARN, RTAPI_MSG_INFO, RTAPI_MSG_DBG, RTAPI_MSG_ALL
} msg_level_t;

#if 0
#define RTAPI_PRINT_MSG(lvl,fmt,...)
#else
#define RTAPI_PRINT_MSG(lvl,fmt,args...) rtapi_print_msg(lvl,fmt,##args)
#endif

extern void rtapi_print_msg(msg_level_t level, const char *fmt, ...) __attribute__((format(printf,2,3)));

/** 'rtapi_print()' prints a printf style message.  Depending on the
 RTOS and whether the program is being compiled for user space
 or realtime, the message may be printed to stdout, stderr, or
 to a kernel message log, etc.  The calling syntax and format
 string is similar to printf except that floating point and
 longlongs are NOT supported in realtime and may not be supported
 in user space.  For some RTOS's, a 80 byte buffer is used, so the
 format line and arguments should not produce a line more than
 80 bytes long.  (The buffer is protected against overflow.)
 Does not block, but  can take a fairly long time, depending on
 the format string and OS.  May be called from user, init/cleanup,
 and realtime code.
 */
extern void rtapi_print(const char *fmt, ...) __attribute__((format(printf,1,2)));

/** Set the maximum level of message to print.  In userspace code,
 each component has its own independent message level.  In realtime
 code, all components share a single message level.  Returns 0 for
 success or -EINVAL if the level is out of range. */

extern int rtapi_set_msg_level(int level);
/** Retrieve the message level set by the last call to rtapi_set_msg_level */

extern int rtapi_get_msg_level(void);

/** 'rtapi_get_msg_handler' and 'rtapi_set_msg_handler' access the function
 pointer used by rtapi_print and rtapi_print_msg.  By default, messages
 appear in the kernel log, but by replacing the handler a user of the rtapi
 library can send the messages to another destination.  Calling
 rtapi_set_msg_handler with NULL restores the default handler. Call from
 real-time init/cleanup code only.  When called from rtapi_print(),
 'level' is RTAPI_MSG_ALL, a level which should not normally be used
 with rtapi_print_msg().
 */
typedef void (*rtapi_msg_handler_t)(msg_level_t level, const char *fmt, va_list ap);

#endif /* UAPI_RTAPI_H */
