#ifndef LEDCTRL_H
#define LEDCTRL_H

#define LEDCTRL_NAME		"ledctrl"
#define LEDCTRL_PREFIX		"[" LEDCTRL_NAME "] "

typedef enum {
    LED_ON,
    LED_OFF,
    GET_STATUS,
    GET_TOPOLOGY,
    NONE
} wagoled_cmd_t;

static const char notify_str [][20] = {
    [LED_ON] = "led_on",
    [LED_OFF] = "led_off",
    [GET_STATUS] = "get_status",
    [GET_TOPOLOGY] = "get_topology",
    [NONE] = "none"
};

int ledctrl_init(void);

#endif //LEDCTRL_H