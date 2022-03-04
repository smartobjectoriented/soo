#pragma once

#include <vector>
#include "http.h"

#define BASE_ADDR           "192.168.1.10"
#define PORT                "8080"
#define GET_TOPOLOGY        "dali/topology"
#define GET_STATUS          "dali/led"
#define POST_LED_ON         "dali/led/on"
#define POST_LED_OFF        "dali/led/off"

namespace LED
{

    struct led_t
    {
        int id;
        int status;
        int value;
    };

    class Ledctrl
    {
    private:
        std::vector<led_t*> leds;
        HTTP::Request client;

    public:
        Ledctrl();
        int init();
        int turn_on(int *ids);
        int turn_off(int *ids);
    };

} // end LED