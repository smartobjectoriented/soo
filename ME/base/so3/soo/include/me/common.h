/*
 * Copyright (C) 2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef ME_COMMON_H
#define ME_COMMON_H

#include <soo/soo.h>

typedef struct {
	agencyUID_t uid;

	int priv_len;
	void *priv;

} host_entry_t;

typedef struct {

	struct list_head list;

	host_entry_t host_entry;
} host_t;

/*
 * Common structure to help in migration pattern and other.
 * This structure must be allocated within a page (or several contiguous pages).
 */
typedef struct {

	/* Number of visited (host) smart object */
	int soohost_nr;

	/*
	 * List of soo hosts by their agency UID. The first entry
	 * is the smart object origin (set first in pre_activate).
	 */
	uint8_t soohosts[0];

} me_common_t;

int concat_hosts(struct list_head *hosts, uint8_t *hosts_array);

/**
 * Remove a specific host from our list.
 *
 * @param agencyUID
 */
void del_host(struct list_head *hosts, agencyUID_t *agencyUID);

/**
 * Add a new entry in the host list.
 *
 * @param me_common
 * @param agencyUID
 */
void new_host(struct list_head *hosts, agencyUID_t *agencyUID, void *priv, int priv_len);

/**
 * Retrieve the list of host from an array.
 *
 * @param hosts_array
 * @param nr
 */
void expand_hosts(struct list_head *hosts, uint8_t *hosts_array, int nr);

/**
 * Reset the list of host
 */
void clear_hosts(struct list_head *hosts);

/**
 * Search for a host corresponding to a specific agencyUID
 *
 * @param agencyUID	UID to compare
 * @return		reference to the host_entry or NULL
 */
host_entry_t *find_host(struct list_head *hosts, agencyUID_t *agencyUID);

/**
 * Duplicate a list of hosts
 *
 * @param src The list to be copied
 * @param dst The copied list
 */
void duplicate_hosts(struct list_head *src, struct list_head *dst);

/**
 * Sort a list of host by its agencyUID
 *
 * @param hosts
 */
void sort_hosts(struct list_head *hosts);

/**
 * Compare two list of hosts
 *
 * @param incoming_hosts
 * @param visits
 * @return
 */
bool hosts_equals(struct list_head *incoming_hosts, struct list_head *);

/**
 * Dump the contents of a list of hosts.
 *
 * @param hosts
 */
void dump_hosts(struct list_head *hosts);

#endif /* ME_COMMON_H */
