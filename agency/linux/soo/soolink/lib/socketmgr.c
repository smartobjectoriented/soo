/*
 * Copyright (C) 2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/security.h>
#include <linux/audit.h>
#include <linux/file.h>
#include <linux/fs.h>

#include <net/sock.h>

#include <soo/soolink/lib/socketmgr.h>

/*
 * Create a socket to be used as client or server with the functions of this library.
 */
struct socket *do_socket(int family, int type, int protocol) {
	int retval;
	struct socket *sock;
	struct file *newfile;
	int flags;

	flags = type & ~SOCK_TYPE_MASK;
	BUG_ON(flags & ~(SOCK_CLOEXEC | SOCK_NONBLOCK));

	type &= SOCK_TYPE_MASK;

	if (SOCK_NONBLOCK != O_NONBLOCK && (flags & SOCK_NONBLOCK))
		flags = (flags & ~SOCK_NONBLOCK) | O_NONBLOCK;

	retval = sock_create(family, type, protocol, &sock);
	if (retval < 0)
		BUG();

	newfile = sock_alloc_file(sock, flags, NULL);
	BUG_ON(IS_ERR(newfile));

	return sock;
}

/*
 * Bind an address with port to this socket.
 */
void do_bind(struct socket *sock, void *myaddr, int addrlen) {
	struct sockaddr_storage address;
	int err;

	memcpy(&address, myaddr, addrlen);

	err =  audit_sockaddr(addrlen, &address);
	BUG_ON(err < 0);

	err = security_socket_bind(sock, (struct sockaddr *) &address, addrlen);
	BUG_ON(err);

	err = sock->ops->bind(sock, (struct sockaddr *) &address, addrlen);
	BUG_ON(err);
}

/*
 * Prepare to listen on a number of client connections.
 */
void do_listen(struct socket *sock, int backlog) {
	int err;
	int somaxconn;

	somaxconn = sock_net(sock->sk)->core.sysctl_somaxconn;
	if ((unsigned int)backlog > somaxconn)
		backlog = somaxconn;

	err = security_socket_listen(sock, backlog);
	BUG_ON(err);

	err = sock->ops->listen(sock, backlog);
	BUG_ON(err);
}

/*
 * As soon as a client gets connected, this function returns a new socket to manage
 * the communication with the client.
 */
struct socket *do_accept(struct socket *sock, struct sockaddr *peer_sockaddr, int *peer_addrlen, int flags) {
	struct socket *newsock;
	int err, len;
	struct sockaddr_storage address;
	struct file *newfile;

	BUG_ON(flags & ~(SOCK_CLOEXEC | SOCK_NONBLOCK));

	if (SOCK_NONBLOCK != O_NONBLOCK && (flags & SOCK_NONBLOCK))
		flags = (flags & ~SOCK_NONBLOCK) | O_NONBLOCK;

	newsock = sock_alloc();
	BUG_ON(!newsock);

	newsock->type = sock->type;
	newsock->ops = sock->ops;

	newfile = sock_alloc_file(newsock, flags, sock->sk->sk_prot_creator->name);
	BUG_ON(IS_ERR(newfile));

	err = security_socket_accept(sock, newsock);
	BUG_ON(err);

	err = sock->ops->accept(sock, newsock, sock->file->f_flags, false);
	BUG_ON(err < 0);

	if (peer_sockaddr) {
		len = newsock->ops->getname(newsock, (struct sockaddr *)&address, 2);
		if (len < 0) {
			/* Connection may be aborted. */
			fput(newfile);
			return NULL;
		}

		memcpy(&peer_sockaddr, &address, *peer_addrlen);
	}

	return newsock;
}

/*
 * Sends some byte to a peer socket connection.
 */
int do_sendto(struct socket *sock, void *buf, size_t size, unsigned int flags, struct sockaddr *addr, int addr_len) {
	int err;
	struct msghdr msg;
	struct sockaddr_storage address;
	struct iovec iov;

	if (size > MAX_RW_COUNT)
		size = MAX_RW_COUNT;

	iov.iov_base = buf;
	iov.iov_len = size;

	msg.msg_iter.type = WRITE;
	msg.msg_iter.iov = &iov;
	msg.msg_iter.iov_offset = 0;
	msg.msg_iter.nr_segs = 1;
	msg.msg_iter.count = size;

	msg.msg_name = NULL;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_namelen = 0;

	if (addr) {
		memcpy(&address, addr, addr_len);

		msg.msg_name = (struct sockaddr *)&address;
		msg.msg_namelen = addr_len;
	}
	if (sock->file->f_flags & O_NONBLOCK)
		flags |= MSG_DONTWAIT;
	msg.msg_flags = flags;
	err = sock_sendmsg(sock, &msg);

	return err;
}

/*
 * Receive some bytes from a peer socket connection.
 */
int do_recvfrom(struct socket *sock, void *buf, size_t size, unsigned int flags, struct sockaddr *addr, int *addr_len) {
	struct msghdr msg;
	struct sockaddr_storage address;
	struct iovec iov;
	int err;

	if (size > MAX_RW_COUNT)
		size = MAX_RW_COUNT;

	iov.iov_base = buf;
	iov.iov_len = size;

	msg.msg_iter.type = READ;
	msg.msg_iter.iov = &iov;
	msg.msg_iter.iov_offset = 0;
	msg.msg_iter.nr_segs = 1;
	msg.msg_iter.count = size;

	msg.msg_control = NULL;
	msg.msg_controllen = 0;

	/* Save some cycles and don't copy the address if not needed */
	msg.msg_name = addr ? (struct sockaddr *)&address : NULL;

	/* We assume all kernel code knows the size of sockaddr_storage */
	msg.msg_namelen = 0;
	msg.msg_iocb = NULL;
	msg.msg_flags = 0;
	if (sock->file->f_flags & O_NONBLOCK)
		flags |= MSG_DONTWAIT;

	err = sock_recvmsg(sock, &msg, flags);

	if (err >= 0 && addr != NULL) {
		memcpy(addr, &address, msg.msg_namelen);
		*addr_len = msg.msg_namelen;
	}
	return err;
}

/*
 * Perform a shutdown on a socket.
 */
void do_shutdown(struct socket *sock, int how) {
	int err;

	err = security_socket_shutdown(sock, how);
	BUG_ON(err);

	err = sock->ops->shutdown(sock, how);
	BUG_ON(err);

}

/*
 * Relase a socket.
 */
void do_socket_release(struct socket *sock) {
	sock_release(sock);
}

