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

#ifndef CONFIG_H
#define CONFIG_H

#include <asm/config.h>

#define EXPORT_SYMBOL(var)
#define EXPORT_SYMBOL_GPL(var)

#define KERN_ERR       "<Error>"
#define KERN_CRIT      "<Critical>"
#define KERN_EMERG     "<Emergency>"
#define KERN_WARNING   "<Warning>"
#define KERN_NOTICE    "<Notice>"
#define KERN_INFO      "<Info>"
#define KERN_DEBUG     "<Debug>"

/* Linux 'checker' project. */
#define __iomem
#define __user
#define __force
#define __bitwise

#ifndef __ASSEMBLY__

int current_domain_id(void);
#define dprintk(_l, _f, _a...)                              \
    printk(_l "%s:%d: " _f, __FILE__ , __LINE__ , ## _a )
#define gdprintk(_l, _f, _a...)                             \
    printk(_l "%s:%d:d%d " _f, __FILE__,       \
           __LINE__, current_domain_id() , ## _a )

#include <avz/compiler.h>

#endif /* !__ASSEMBLY__ */

#define __STR(...) #__VA_ARGS__
#define STR(...) __STR(__VA_ARGS__)

#ifndef __ASSEMBLY__
/* Turn a plain number into a C unsigned long constant. */
#define __mk_unsigned_long(x) x ## UL
#define mk_unsigned_long(x) __mk_unsigned_long(x)
#else /* __ASSEMBLY__ */
/* In assembly code we cannot use C numeric constant suffixes. */
#define mk_unsigned_long(x) x
#endif /* !__ASSEMBLY__ */

#define fastcall
#define __cpuinitdata
#define __cpuinit

#endif /* CONFIG_H */
