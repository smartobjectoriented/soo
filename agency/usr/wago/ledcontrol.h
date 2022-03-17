#pragma once

#include <vector>
#include "http.h"


/**** REST SERVER DATA ****/
#define BASE_ADDR           "192.168.1.10"
#define PORT                "8080"
#define GET_TOPOLOGY        "dali/topology"
#define GET_STATUS          "dali/led"
#define POST_LED_ON         "dali/led/on"
#define POST_LED_OFF        "dali/led/off"
#define ARGS                "?id="
#define LED_ALL             "all"

/**** JSON DATA ****/
#define DATA_OBJ            "Data"
#define DEVICE_ARR          "devices"
#define DEVICE_TYPE         "type"
#define DEVICE_LED          "Led"
#define LED_ID              "id"
#define LED_STATUS          "status"
#define LED_ON              "on"

/**** DRIVER SYSFS ACCESS ****/
#define SYSFS_BASE          "/sys/soo/backend/vwagoled/vwagoled_"
#define SYSFS_NOTIFY        SYSFS_BASE "notify"
#define SYSFS_DEBUG         SYSFS_BASE "debug"

/**** BUFFERS ****/
#define BUFFER_SIZE         1024
#define CMD_SIZE            32
#define CMD_NUM             5

#define IDS_DELIM           ','

namespace LED
{
    typedef enum 
    {
        CMD_LED_ON = 0,
        CMD_LED_OFF,
        CMD_GET_STATUS,
        CMD_GET_TOPOLOGY,
        NONE
    } wagoled_cmd_t;

    static const char notify_str [][CMD_SIZE] = {
        "led_on",
        "led_off",
        "get_status",
        "get_topology",
        "none"
    };

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
        void start_debug();
        void stop_debug();
        void extract_ids(char *ids, std::vector<int>& vect_ids);
        int process_notify(char *cmd, char* ids);

    public:
        Ledctrl();
        ~Ledctrl();
        void init();
        void update_status();
        int turn_on(std::vector<int> ids);
        int turn_off(std::vector<int> ids);
        int turn_all_on();
        int turn_all_off();

        int main_loop();
    };

} // end LED