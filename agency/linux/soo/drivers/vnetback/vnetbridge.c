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

#include "vnetbridge_priv.h"

#include <net/arp.h>
#include <net/ip.h>
#include <net/route.h>
#include <net/ip_fib.h>
#include <net/rtnetlink.h>
#include <net/net_namespace.h>
#include <net/addrconf.h>


void vnetbridge_add_if(const char* brname, const char* ifname){
	int fd = -1;
	int err;

	struct ifreq ifr;
	int ifindex;


	if((fd = sys_socket(AF_LOCAL, SOCK_STREAM, 0)) < 0){
		printk("SOCKET");
		return;
	}

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	ifr.ifr_name[IFNAMSIZ - 1] = 0;

	if (sys_ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
		printk("NODEV");
		return;
	}

	ifindex = ifr.ifr_ifindex;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, brname, strlen(brname));
	ifr.ifr_name[strlen(brname)] = 0;


	ifr.ifr_ifindex = ifindex;
	if(err = sys_ioctl(fd, SIOCBRADDIF, &ifr)){
		printk("ADDIF %d", err);
		return;
	}

	sys_close(fd);
}

void vnetbridge_remove_if(const char* brname, const char* ifname){
	int fd = -1;
	int err;

	struct ifreq ifr;
	int ifindex;


	if((fd = sys_socket(AF_LOCAL, SOCK_STREAM, 0)) < 0){
		printk("SOCKET");
		return;
	}

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	ifr.ifr_name[IFNAMSIZ - 1] = 0;

	if (sys_ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
		printk("NODEV");
		return;
	}

	ifindex = ifr.ifr_ifindex;

	strncpy(ifr.ifr_name, brname, strlen(brname));
	ifr.ifr_name[strlen(brname)] = 0;


	ifr.ifr_ifindex = ifindex;
	if(sys_ioctl(fd, SIOCBRDELIF, &ifr)){
		printk("RMIF");
		return;
	}

	sys_close(fd);
}


void vnetbridge_add(const char* brname){
	int fd = -1;
	struct ifreq ifr;
	int err, ret;


	if((fd = sys_socket(AF_LOCAL, SOCK_STREAM, 0)) < 0){
		printk("SOCKET");
		return;
	}

	ret = sys_ioctl(fd, SIOCBRADDBR, brname);

	memset(&ifr, 0, sizeof ifr);
	strncpy(ifr.ifr_name, brname, strlen(brname));

	ifr.ifr_flags |= IFF_UP;
	sys_ioctl(fd, SIOCSIFFLAGS, &ifr);


	sys_close(fd);
}

void vnetbridge_if_conf(const char* name, short flags, bool set){
	int sockfd;
	struct ifreq ifr;

	sockfd = sys_socket(AF_INET, SOCK_DGRAM, 0);

	if (sockfd < 0)
		return;


	memset(&ifr, 0, sizeof ifr);

	strncpy(ifr.ifr_name, name, IFNAMSIZ);

	sys_ioctl(sockfd, SIOCGIFFLAGS, &ifr);

	strncpy(ifr.ifr_name, name, IFNAMSIZ);

	if(set)
		ifr.ifr_flags |= flags;
	else
		ifr.ifr_flags &= ~flags;

	sys_ioctl(sockfd, SIOCSIFFLAGS, &ifr);

	sys_close(sockfd);
}

uint32_t inet_addr(const char *cp)
{
	uint32_t result = 0;

	int i;
	int j = 0;
	int k = 0;
	int l;
	char buf[4];
	char ip[4];
	int len = strlen(cp);
	char exp = 1;

	for (i = 0 ; i <= strlen(cp) ; i++) {
		if ((cp[i] == '.') || (i == len)) {
			buf[j] = '\0';
			j = 0;
			ip[k] = (char)0;
			exp = 1;
			for (l = strlen(buf) - 1 ; l >= 0 ; l--) {
				ip[k] += (char)((buf[l] - '0') * exp);
				exp *= (char)10;
			}
			k++;
		}
		else {
			buf[j] = cp[i];
			j++;
		}
	}

	result = (uint32_t)((uint32_t)(((ip[0] & 0xFF) << 24) & 0xFF000000) |
			    (uint32_t)(((ip[1] & 0xFF) << 16) & 0x00FF0000) |
			    (uint32_t)(((ip[2] & 0xFF) << 8) & 0x0000FF00) |
			    (uint32_t)((ip[3] & 0xFF) & 0x000000FF));

	return result;
}

void vnetbridge_if_set_ip(const char* name, uint32_t network, uint32_t mask){
	struct ifreq ifr;
	int ret;
	struct sockaddr_in sai;
	int sockfd;
	int selector;
	uint32_t ip = (network & mask) | 0x01 << 24;
	uint32_t ip_me = (network & mask) | 0x02 << 24;

	//iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
	/* Create a channel to the NET kernel. */
	sockfd = sys_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

	/* get interface name */
	strncpy(ifr.ifr_name, name, IFNAMSIZ);


	sys_ioctl(sockfd, SIOCGIFFLAGS, &ifr);
	ifr.ifr_flags &= ~IFF_UP;
	sys_ioctl(sockfd, SIOCSIFFLAGS, &ifr);




	memset(&sai, 0, sizeof(struct sockaddr));
	sai.sin_family = AF_INET;
	sai.sin_port = 0;

	sai.sin_addr.s_addr = ip;
	memcpy(((char *)&ifr.ifr_addr), (char *)&sai, sizeof(struct sockaddr));
	ret = sys_ioctl(sockfd, SIOCSIFADDR, &ifr);

	sai.sin_addr.s_addr = ip_me;
	memcpy(((char *)&ifr.ifr_addr), (char *)&sai, sizeof(struct sockaddr));
	ret = sys_ioctl(sockfd, SIOCSIFDSTADDR, &ifr);

	sai.sin_addr.s_addr = mask;
	memcpy(((char *)&ifr.ifr_addr), (char *)&sai, sizeof(struct sockaddr));
	ret = sys_ioctl(sockfd, SIOCSIFNETMASK, &ifr);

	//inet_addr("10.10.1.1");//0xc835a8c0;// c0a835c8u;// inet_addr("192.168.40.200");
	//memcpy(&ifr.ifr_addr, &sai, sizeof(struct sockaddr_in));
	//ret = sys_ioctl(sockfd, SIOCSIFADDR, &ifr);
	printk("\nSET IP %d\n", ret);


	sys_ioctl(sockfd, SIOCGIFFLAGS, &ifr);
	ifr.ifr_flags |= IFF_UP;
	sys_ioctl(sockfd, SIOCSIFFLAGS, &ifr);

	/*memset(&sai, 0, sizeof(struct sockaddr));
	sai.sin_family = AF_INET;
	sai.sin_port = 0;
	sai.sin_addr.s_addr = inet_addr("255.255.255.0");
	memcpy(&ifr.ifr_broadaddr, &sai, sizeof(struct sockaddr_in));
	ret = sys_ioctl(sockfd, SIOCSIFNETMASK, &ifr);
	printk("\nSET IP %d\n", ret);*/
	sys_close(sockfd);
}