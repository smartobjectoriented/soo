#ifndef LEDCTRL_H
#define LEDCTRL_H

#define LEDCTRL_NAME		"ledctrl"
#define LEDCTRL_PREFIX		"[" LEDCTRL_NAME "] "

#define IDS_STR_MAX         256

typedef enum {
    LED_ON,
    LED_OFF,
    GET_STATUS,
    GET_TOPOLOGY,
    NONE
} wago_cmd_t;

static const char notify_str [][20] = {
    [LED_ON] = "led_on",
    [LED_OFF] = "led_off",
    [GET_STATUS] = "get_status",
    [GET_TOPOLOGY] = "get_topology",
    [NONE] = "none"
};

int ledctrl_init(void);
void ledctrl_process_request(int cmd, int *ids, int ids_count);

#endif //LEDCTRL_H