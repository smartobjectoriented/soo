#pragma once

#include <vector>
#include "http.h"

#define BASE_ADDR           "192.168.1.10"
#define PORT                "8080"
#define GET_TOPOLOGY        "dali/topology"
#define GET_STATUS          "dali/led"
#define POST_LED_ON         "dali/led/on"
#define POST_LED_OFF        "dali/led/off"

#define DATA_OBJ            "Data"
#define DEVICE_ARR          "devices"
#define DEVICE_TYPE         "type"
#define DEVICE_LED          "Led"
#define LED_ID              "id"
#define LED_STATUS          "status"
#define LED_ON              "on"

#define ARGS                "?id="
#define LED_ALL             "all"

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
        void build_args_str(std::string& args, std::vector<int> ids);

    public:
        Ledctrl();
        ~Ledctrl();
        void init();
        void update_status();
        int turn_on(std::vector<int> ids);
        int turn_off(std::vector<int> ids);
        int turn_all_on();
        int turn_all_off();
    };

} // end LED