#include <rapidjson/document.h>
#include <iostream>
#include "ledcontrol.h"

namespace LED
{
    Ledctrl::Ledctrl() : client(BASE_ADDR, PORT)
    {
        // client = HTTP::Request(BASE_ADDR, PORT);
    }

    int Ledctrl::init()
    {
        std::string topology;
        rapidjson::Document doc;

        if (client.GET(GET_TOPOLOGY, topology) < 0) 
        {
            return -1;
        }

        doc.Parse(topology.c_str());

        assert(doc.IsObject());

        return 1;

        std::cout << topology << std::endl;
    }
} //end LED