/*
 * Copyright (C) 2022 Daniel Rossier <daniel.rossier@heig-vd.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef HYPERCALL_H
#define HYPERCALL_H

#include <soo/uapi/domctl.h>

void do_domctl(domctl_t *args);
void do_event_channel_op(int cmd, void *args);

#endif /* HYPERCALL_H */
