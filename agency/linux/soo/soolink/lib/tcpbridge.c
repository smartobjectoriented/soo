
/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#if 1
#define DEBUG
#endif

#include <linux/init.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/kthread.h>

#include <soo/uapi/debug.h>
#include <soo/soolink/plugin/ethernet.h>

#include <soo/soolink/lib/socketmgr.h>

/* TCP/IP port on which the tcpmgr server will listen to requests from remote connection on a IP network. */
#define NET_PORT	5008

/* Only one client is accepted at a time */
#define N_BACKLOG	1

/* Maximal TCP/IP packet size */
#define MAX_PKT_SIZE	1440

/* Currently only one remote connection is managed. */
static struct socket *sock = NULL;
static struct mutex sock_lock;

void tcpbridge_sendto(void *buff, size_t len) {
	static struct socket *tmp_sock;

	mutex_lock(&sock_lock);
	tmp_sock = sock;
	mutex_unlock(&sock_lock);

	/* If sock is NULL, this means that no client is connected */
	if (unlikely(!tmp_sock))
		return ;

	do_sendto(sock, buff, len, 0, NULL, 0);
}


/**
 * TCP/IP Server management for processing GUI request issued from a smartphone or tablet.
 * Currently, manage only one remote connection at a time.
 */
static int tcp_server_task_fn(void *arg) {
	struct socket *first_sock, *new_sock;
	struct sockaddr_in srv_addr;
	int len;
	char *buf = (char *) kmalloc(MAX_PKT_SIZE, GFP_KERNEL);

	/* Get a new socket for handling TCP/IP connection */
	first_sock = do_socket(AF_INET, SOCK_STREAM, 0);
	BUG_ON(!first_sock);

	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	srv_addr.sin_port = htons(NET_PORT);

	/* Bind our address and port to this socket */
	DBG0("Socket bind\n");
	do_bind(first_sock, (struct sockaddr *) &srv_addr, sizeof(struct sockaddr_in));

	/* Accept N_BACKLOG connections */
	DBG0("Socket listen\n");
	do_listen(first_sock, N_BACKLOG);

	while (1) {
		/* First infinite loop: accept incoming connections */
		new_sock = do_accept(first_sock, NULL, NULL, 0);
		DBG0("Socket accept\n");

		/* Store the sock globally */
		mutex_lock(&sock_lock);
		sock = new_sock;
		mutex_unlock(&sock_lock);

		while (1) {
			/* Second infinite loop: accept incoming packets */
			len = do_recvfrom(new_sock, buf, MAX_PKT_SIZE, 0, NULL, NULL);

			/* Close the socket on error */
			if (len <= 0) {

				do_socket_release(new_sock);
				DBG0("Socket release\n");

				mutex_lock(&sock_lock);
				sock = NULL;
				mutex_unlock(&sock_lock);

				break;
			}

			DBG("Size: %d\n", len);
			plugin_tcp_rx(buf, len);
		}
	}

	/* This should never be reached */
	BUG();
}


int tcpbridge_init(void) {
	mutex_init(&sock_lock);

	kthread_run(tcp_server_task_fn, NULL, "tcp_server");

	return 0;
}

late_initcall(tcpbridge_init);
