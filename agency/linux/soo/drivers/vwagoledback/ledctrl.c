/*
 * Copyright (C) 2022 Mattia Gallacchi <mattia.gallacchi@heig-vd.ch>
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

#if 0
#define DEBUG
#endif

#if 0
#define DEBUG_MODE
#endif

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <soo/core/sysfs.h>
#include <soo/vbus.h>

#include "ledctrl.h"

/* Completion use to synchronize a new command */
static struct completion notify;

/* Current command. Set by ledctrl_process_request */
static volatile wago_cmd_t current_cmd;

/* String containing the string ids to apply the command to */
static char *ids_str;

/** This mode allow to simulate a received input **/
#ifdef DEBUG_MODE

#define DELAY_MS    500
int debug_active = 0;
const char ids[] = "1,2,3,4,5,6";
struct task_struct *thread;

int debug_thread(void *data)
{
    DBG(LEDCTRL_PREFIX "Starting debug thread\n");

    while (!kthread_should_stop()) {

        DBG(LEDCTRL_PREFIX "Notify user app");
        current_cmd = (current_cmd == LED_ON) ? LED_OFF : LED_ON;
        complete(&notify);

        msleep(DELAY_MS);
    }

    DBG(LEDCTRL_PREFIX "exiting debug thread\n");

    return 0;
}

void sysfs_wagodebug_store(char *str)
{
    uint8_t val;

    if (sscanf(str, "%hhu", &val) < 0) {
        DBG(LEDCTRL_PREFIX "Error: wrong argument. Must integer\n");
        return;
    }
 
    if (val >= 1 && !debug_active) {
        debug_active = 1;
        thread = kthread_run(debug_thread, NULL, "debug_thread");
        BUG_ON(!thread);
    } else {
        debug_active = 0;
        BUG_ON(kthread_stop(thread));
    }
}

void sysfs_wagodebug_show(char *str)
{
    sprintf(str, "%d", debug_active);
}

#endif // DEBUG_MODE

void ledctrl_process_request(int cmd, int *ids, int ids_count)
{
    int i;
    char id[ID_STR_SIZE] = {0};

    current_cmd = (wago_cmd_t)cmd;
    memset(ids_str, 0, IDS_STR_MAX);

    if (!ids)
        return;

    for (i = 0; i < ids_count; i++) {
        sprintf(id, "%d,", ids[i]);
        strcat(ids_str, id);
    }

    complete(&notify);
}

void sysfs_wago_notify_show(char *str)
{
    wait_for_completion(&notify);
    sprintf(str, "%s", notify_str[current_cmd]);
}

void sysfs_wago_led_on_show(char *str)
{
    sprintf(str, "%s", ids_str);
}

void sysfs_wago_led_off_show(char *str)
{
    sprintf(str, "%s", ids_str);
}

void sysfs_wago_get_topology_store(char *str)
{
    pr_warn(LEDCTRL_PREFIX "Not yet implemented\n");
}

void sysfs_wago_get_status_store(char *str)
{
    pr_warn(LEDCTRL_PREFIX "Not yet implemented");
}

int ledctrl_init(void)
{
    int ret = 0;

    DBG(LEDCTRL_PREFIX "Starting\n");

    soo_sysfs_register(vwagoled_notify, sysfs_wago_notify_show, NULL);
    soo_sysfs_register(vwagoled_led_on, sysfs_wago_led_on_show, NULL);
    soo_sysfs_register(vwagoled_led_off, sysfs_wago_led_off_show, NULL);
    soo_sysfs_register(vwagoled_get_topology, NULL, sysfs_wago_get_topology_store);
    soo_sysfs_register(vwagoled_get_status, NULL, sysfs_wago_get_status_store);

    current_cmd = NONE;
    
    init_completion(&notify);

#ifdef DEBUG_MODE
    soo_sysfs_register(vwagoled_debug, sysfs_wagodebug_show, sysfs_wagodebug_store);
#endif

    ids_str = kzalloc(IDS_STR_MAX * sizeof(char), GFP_ATOMIC);

    if (!ids_str) {
        ret = -1;
        DBG(LEDCTRL_PREFIX " failed to allocate memory\n");
    }

    memset(ids_str, 0, IDS_STR_MAX);

    DBG(LEDCTRL_PREFIX "Start successfull\n");

    return ret;
}