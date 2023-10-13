/*
 * Copyright (C) 2020 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef SOOENV_H
#define SOOENV_H

#include <linux/list.h>

#include <soo/uapi/soo.h>

struct soo_discovery_env;
struct soo_plugin_env;
struct soo_winenet_env;

typedef struct {
	struct list_head list;
	unsigned int pid;
} soo_env_thread_t;

typedef struct {
        struct list_head list;
        
        /* List of attached threads */
        struct list_head threads;

        char name[80];
        unsigned int id;

        /* Helpful to say when (all) soo_env structures are initialized. */
        bool ready;

        /* agencyUID of this Smart Object */
        uint64_t agencyUID;

        struct soo_simul_env *soo_simul;

        struct soo_soolink_env *soo_soolink;
        struct soo_discovery_env *soo_discovery;
        struct soo_winenet_env *soo_winenet;
        struct soo_plugin_env *soo_plugin;

        /* Transceiver */
        struct soo_transceiver_env *soo_transceiver;

        /* Transcoder */
        struct soo_transcoder_env *soo_transcoder;

} soo_env_t;

soo_env_t *__current_soo(void);

typedef bool(*soo_iterator_t)(soo_env_t *, void *args);

typedef void (*sooenv_up_fn_t)(soo_env_t *, void *args);

#define current_soo		__current_soo()

/* Used for the simulated environment */
#define current_soo_simul	(current_soo->soo_simul)

#define current_soo_soolink	(current_soo->soo_soolink)
#define current_soo_discovery	(current_soo->soo_discovery)
#define current_soo_plugin	(current_soo->soo_plugin)
#define current_soo_winenet     (current_soo->soo_winenet)
#define current_soo_transceiver	(current_soo->soo_transceiver)
#define current_soo_transcoder	(current_soo->soo_transcoder)

void add_thread(soo_env_t *soo, unsigned int pid);

void sooenv_init(void);

void dump_soo(void);

soo_env_t *get_soo_by_name(char *name);

void register_sooenv_up(sooenv_up_fn_t sooenv_up_fn, void *args);

#endif /* SOOENV_H */
