/*
 * Copyright (C) 2001-2013 Philippe Gerum <rpm@xenomai.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */
#ifndef _XENOMAI_VERSION_H
#define _XENOMAI_VERSION_H

#ifndef __KERNEL__
#include <xeno_config.h>
#include <boilerplate/compiler.h>
#endif

#define XENO_VERSION(maj, min, rev)  (((maj)<<16)|((min)<<8)|(rev))

#define XENO_VERSION_CODE	XENO_VERSION(CONFIG_XENO_VERSION_MAJOR,	\
					     CONFIG_XENO_VERSION_MINOR,	\
					     CONFIG_XENO_REVISION_LEVEL)

#define XENO_VERSION_STRING	"SOO-1.1"

#define XENO_VERSION_NAME	"SOO-xenocobalt"

#endif /* _XENOMAI_VERSION_H */
