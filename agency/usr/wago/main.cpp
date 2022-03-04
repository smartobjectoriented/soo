#include <iostream>
#include <unistd.h>
#include "ledcontrol.h"

#define SEC                 1000000
#define MSEC                1000
#define SLEEP               0.5 * SEC


int main(int argc, char *argv[])
{

    LED::Ledctrl ctl;

    ctl.init();
    /* HTTP::Request client(BASE_ADDR, PORT);

    std::string resp;

    // if (client.GET(GET_TOPOLOGY, resp) < 0) {
    //     std::cout << "Failed to read topology" << std::endl;
    // } else {
    //     std::cout << "get topology :\n" << resp << std::endl;
    // }

    // if (client.GET(GET_STATUS, resp, "?id=all") < 0) {
    //     std::cout << "Failed to read status" << std::endl;
    // } else {
    //     std::cout << "get status :\n" << resp << std::endl;
    // }

    client.POST(POST_LED_OFF, "?id=all");

    int id = 1;
    std::string args;

    while (1)
    {
        args = "?id=";
        for (int i = 0; i < 6; i++)
        {
            args += std::to_string(id + i) + ",";
        }
        // args = "?id=" + std::to_string(id);
        
        if (client.POST(POST_LED_ON, args) < 0) 
        {
            std::cout << "Failed to POST led on" << std::endl;
            break;
        }

        usleep(SLEEP);

        if (client.POST(POST_LED_OFF, args) < 0)
        {
            std::cout << "Failed to POST led on" << std::endl;
            break;
        }

        id += 6;
        if (id > 12) {
            id = 1;
        } 
    } */
    

    return 0;
}