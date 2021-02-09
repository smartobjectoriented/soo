/*
 * Copyright (C) 2020 Julien Quartier <julien.quartier@bluewin.ch>
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

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ethtool.h>
#include <linux/syscalls.h>

#include "vnetifutil_priv.h"

#include <net/arp.h>
#include <net/ip.h>
#include <net/route.h>
#include <net/ip_fib.h>
#include <net/rtnetlink.h>
#include <net/net_namespace.h>
#include <net/addrconf.h>

void _vnetifutil_up(int fd, const char * name){
	struct ifreq ifr;
	strncpy(ifr.ifr_name, name, IFNAMSIZ);

	sys_ioctl(fd, SIOCGIFFLAGS, (unsigned long)&ifr);
	ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
	sys_ioctl(fd, SIOCSIFFLAGS, (unsigned long)&ifr);
}

void _vnetifutil_down(int fd, const char * name){
	struct ifreq ifr;
	strncpy(ifr.ifr_name, name, IFNAMSIZ);

	sys_ioctl(fd, SIOCGIFFLAGS, (unsigned long)&ifr);
	ifr.ifr_flags &= ~(IFF_UP | IFF_RUNNING);
	sys_ioctl(fd, SIOCSIFFLAGS, (unsigned long)&ifr);
}

short _vnetifutil_get_flags(int fd, const char * name){
	struct ifreq ifr;
	int err = 0;
	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, name, IFNAMSIZ);

	if ((err = sys_ioctl(fd, SIOCGIFFLAGS, (unsigned long)&ifr)) < 0){
		return 0;
	}

	return ifr.ifr_flags;
}


void _vnetifutil_set_address(int fd, const char * name, uint32_t ip, unsigned int cmd){
	struct ifreq ifr;
	struct sockaddr_in addr;

	strncpy(ifr.ifr_name, name, IFNAMSIZ);

	memset(&addr, 0, sizeof(struct sockaddr));
	addr.sin_family = AF_INET;
	addr.sin_port = 0;

	addr.sin_addr.s_addr = ip;
	memcpy(((char *)&ifr.ifr_addr), (char *)&addr, sizeof(struct sockaddr));
	sys_ioctl(fd, cmd, (unsigned long)&ifr);
}

#define _vnetif_set_netmask(__fd, __name, __ip) _vnetifutil_set_address(__fd, __name, __ip, SIOCSIFNETMASK)
#define _vnetif_set_ip(__fd, __name, __ip) _vnetifutil_set_address(__fd, __name, __ip, SIOCSIFADDR)
#define _vnetif_set_destip(__fd, __name, __ip) _vnetifutil_set_address(__fd, __name, __ip, SIOCSIFDSTADDR)


int vnetifutil_if_running(const char* name) {
	int fd, running;
	fd = sys_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

	running = !!(_vnetifutil_get_flags(fd, name) & IFF_RUNNING);

	sys_close(fd);

	return running;
}

void vnetifutil_if_up(const char* name){
	int fd;
	fd = sys_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	_vnetifutil_up(fd, name);

	sys_close(fd);
}

void vnetifutil_if_down(const char* name){
	int fd;
	fd = sys_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	_vnetifutil_down(fd, name);

	sys_close(fd);
}

void vnetifutil_if_set_ips(const char* name, uint32_t network, uint32_t mask){
	int fd;
	uint32_t ip, ip_me;

	ip = (network & mask) | 0x01 << 24;
	ip_me = (network & mask) | 0x02 << 24;

	fd = sys_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

	/* The [down / edit / up] sequence is important, if not used some
	 * services won't see changes on network interface */
	_vnetifutil_down(fd, name);

	_vnetif_set_ip(fd, name, ip);
	_vnetif_set_netmask(fd, name, mask);
	_vnetif_set_destip(fd, name, ip_me);

	_vnetifutil_up(fd, name);

	sys_close(fd);
}