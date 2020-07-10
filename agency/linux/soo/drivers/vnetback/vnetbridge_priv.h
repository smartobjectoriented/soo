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

#ifndef VNETBRIDGE_PRIV_H
#define VNETBRIDGE_PRIV_H

#define SOO_BRIDGE_NAME "soo-br0"

void vnetbridge_add_if(const char* brname, const char* ifname);
void vnetbridge_remove_if(const char* brname, const char* ifname);
void vnetbridge_add(const char* brname);
void vnetbridge_if_conf(const char* name, short flags, bool set);
void vnetbridge_if_set_ip(const char* name);

#endif //VNETBRIDGE_PRIV_H
