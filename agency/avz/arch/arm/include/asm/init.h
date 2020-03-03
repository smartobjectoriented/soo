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

#ifndef INIT_H
#define INIT_H

/* For assembly routines */
#define __START		    .section	    ".text.start","ax"

#define __HEAD          .section        ".head.text","ax"
#define __INIT          .section        ".init.text","ax"
#define __FINIT         .previous

#define __INITDATA      .section        ".init.data","aw",%progbits
#define __INITRODATA    .section        ".init.rodata","a",%progbits
#define __FINITDATA     .previous

#define __DEVINIT        .section       ".devinit.text", "ax"
#define __DEVINITDATA    .section       ".devinit.data", "aw"
#define __DEVINITRODATA  .section       ".devinit.rodata", "a"

#define __CPUINIT        .section       ".cpuinit.text", "ax"
#define __CPUINITDATA    .section       ".cpuinit.data", "aw"
#define __CPUINITRODATA  .section       ".cpuinit.rodata", "a"

#define __MEMINIT        .section       ".meminit.text", "ax"
#define __MEMINITDATA    .section       ".meminit.data", "aw"
#define __MEMINITRODATA  .section       ".meminit.rodata", "a"

#endif /* INIT_H */
