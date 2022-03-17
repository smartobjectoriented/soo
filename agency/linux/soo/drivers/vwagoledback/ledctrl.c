
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <soo/core/sysfs.h>
#include <soo/vbus.h>

#include "ledctrl.h"

#if 1
#define DEBUG
#endif

struct completion notify;
struct task_struct *thread;
volatile wagoled_cmd_t current_cmd;

#ifdef DEBUG

#define DELAY_MS    200
#define SLEEP_MS    10
int debug_active = 0;
const char ids[] = "1,2,3,4,5,6";

int debug_thread(void *data)
{
    uint32_t count_ms = 0;
    DBG(LEDCTRL_PREFIX "Starting debug thread\n");

    while (!kthread_should_stop()) {

        if (count_ms >= DELAY_MS) {
            DBG(LEDCTRL_PREFIX "Notify user app");
            current_cmd = (current_cmd == LED_ON) ? LED_OFF : LED_ON;
            count_ms = 0;
            complete(&notify);
        } 
        
        count_ms += SLEEP_MS;
        msleep(SLEEP_MS);
    }

    DBG(LEDCTRL_PREFIX "exiting debug thread\n")

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

#endif //DEBUG

void sysfs_wagonotify_show(char *str)
{
    wait_for_completion(&notify);
    sprintf(str, "%s", notify_str[current_cmd]);
}

void sysfs_wago_led_on_show(char *str)
{
    sprintf(str, "%s", ids);
}

void sysfs_wago_led_off_show(char *str)
{
    sprintf(str, "%s", ids);
}

void sysfs_wago_get_topology_store(char *str)
{

}

void sysfs_wago_get_status_store(char *str)
{

}

int ledctrl_init(void)
{
    int ret = 0;

    DBG(LEDCTRL_PREFIX "Starting\n");

    soo_sysfs_register(vwagoled_notify, sysfs_wagonotify_show, NULL);
    soo_sysfs_register(vwagoled_led_on, sysfs_wago_led_on_show, NULL);
    soo_sysfs_register(vwagoled_led_off, sysfs_wago_led_off_show, NULL);
    soo_sysfs_register(vwagoled_get_topology, NULL, sysfs_wago_get_topology_store);
    soo_sysfs_register(vwagoled_get_status, NULL, sysfs_wago_get_status_store);

    current_cmd = NONE;
    
    init_completion(&notify);

#ifdef DEBUG
    soo_sysfs_register(vwagoled_debug, sysfs_wagodebug_show, sysfs_wagodebug_store);
#endif

    DBG(LEDCTRL_PREFIX "Start successfull\n");

    return ret;
}

EXPORT_SYMBOL(ledctrl_init);