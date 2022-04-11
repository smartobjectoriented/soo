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
#define LEDS_ARRAY          "leds"
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
        /* Not used yet */
        int value;
    };

    class Ledctrl
    {
    private:
        /* List of LEDs */
        std::vector<led_t*> leds;

        /* Http client class used to send HTTP reqeusts */
        HTTP::Request client;

        /**
         * @brief Converts a vectors of ids to a comma separated string of ids
         * 
         * @param args String for http request
         * @param ids ids to convert to string
         */
        void build_args_str(std::string& args, std::vector<int> ids);

        /**
         * @brief Activate debug mode
         * 
         */
        void start_debug();

        /**
         * @brief Stop debug mode
         * 
         */
        void stop_debug();

        /**
         * @brief Converts a string of ids to a vector of ids
         * 
         * @param ids String of ids
         * @param vect_ids Vector of ids
         */
        void extract_ids(char *ids, std::vector<int>& vect_ids);

        /**
         * @brief Process a command sent by vwagoled backend driver.
         * 
         * @param cmd command (ex. led_on, led_off, etc...)
         * @param ids string containing the concerned LEDs
         */
        void process_notify(char *cmd, char* ids);

    public:
        
        /**
         * @brief Construct a new Ledctrl object
         * 
         */
        Ledctrl();

        /**
         * @brief Destroy the Ledctrl object
         * 
         */
        ~Ledctrl();

        /**
         * @brief Initialize Ledctrl object. Reads the DALI topology
         * 
         */
        void init();

        /**
         * @brief Update the status of led_t in the LEDs vector
         * 
         */
        void update_status();

        /**
         * @brief Turn on LEDs
         * 
         * @param ids IDs of the LEDs to turn on
         */
        void turn_on(std::vector<int> ids);

        /**
         * @brief Turn off LEDs
         * 
         * @param ids IDs of the LEDs to turn off
         */
        void turn_off(std::vector<int> ids);

        /**
         * @brief Turn all LEDs on
         * 
         */
        void turn_all_on();

        /**
         * @brief Turn all LEDs off
         * 
         */
        void turn_all_off();

        /**
         * @brief Infinite processing loop
         * 
         * @return int 0 when stopped by SIGINT
         */
        int main_loop();
    };

} // end LED