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

#ifndef VNETIFUTIL_PRIV_H
#define VNETIFUTIL_PRIV_H

void vnetifutil_if_up(const char* name);
void vnetifutil_if_down(const char* name);

void vnetifutil_if_set_ips(const char* name, uint32_t network, uint32_t mask);

int vnetifutil_if_running(const char* name);

#endif //VNETIFUTIL_PRIV_H
