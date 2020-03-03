/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <soo/soo.h>

void mig_restore_domain_migration_info(unsigned int ME_slotID, struct domain *domU);
void mig_restore_vcpu_migration_info(unsigned int ME_slotID, struct domain *domU);
void after_migrate_to_user(void);

int migration_init(soo_hyp_t *op);
int migration_final(soo_hyp_t *op);

int read_migration_structures(soo_hyp_t *op);
int write_migration_structures(soo_hyp_t *op);

int restore_migrated_domain(unsigned int ME_slotID);
int restore_injected_domain(unsigned int ME_slotID);

int inject_me(soo_hyp_t *op);

bool is_me_realtime(void);
