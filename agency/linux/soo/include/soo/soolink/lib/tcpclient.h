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
#include <linux/types.h>
extern int soo_space_server;
int connect_server(unsigned char* ip,unsigned port);
int TCP_send_ME_to_server(void* ME, size_t me_size);
int TCP_rcv_ME_from_server(void** new_ME,size_t* size);
void init_client_soo_space(unsigned char *ip,unsigned port);