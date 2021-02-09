/*
 * Copyright (C) 2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2017-2019 Baptiste Delporte <bonel@bonel.net>
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

/* Socket management for SOOlink and other SOO in-kernel subsystems */

#ifndef SOCKETMGR_H
#define SOCKETMGR_H

#include <uapi/linux/in.h>

#include <linux/net.h>
#include <linux/byteorder/generic.h>

struct socket *do_socket(int family, int type, int protocol);
void do_bind(struct socket *sock, void *myaddr, int addrlen);
void do_listen(struct socket *sock, int backlog);
struct socket *do_accept(struct socket *sock, struct sockaddr *peer_sockaddr, int * peer_addrlen, int flags);
void do_socket_release(struct socket *sock);
void do_shutdown(struct socket *sock, int how);

int do_sendto(struct socket *sock, void *buff, size_t len, unsigned int flags, struct sockaddr *addr, int addr_len);
int do_recvfrom(struct socket *sock, void *buf, size_t size, unsigned int flags, struct sockaddr *addr, int *addr_len);

#endif /* SOCKETMGR_H */
