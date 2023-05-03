/*
 * Copyright (C) 2023 A.Gabriel Catel Torres <arzur.cateltorres@heig-vd.ch>
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

#include "mqtt_iuoc_subscriber.h"
#include <json/json.h>
#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cctype>
#include <thread>
#include <chrono>
#include <mqtt/async_client.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream> 
#include <unordered_map>

#include <soo/uapi/iuoc.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>


std::vector<std::string> topics_publish_name(IUOC_ME_END);

std::vector<std::string> pub_debug;

std::string get_payload_from_data(iuoc_data_t *data);

int main(int argc, char* argv[])
{
    std::ifstream inFile;
    inFile.open("/root/iuoc_mqtt_config.json"); //open the config file

    std::stringstream strStream;
    strStream << inFile.rdbuf(); //read the file

    std::string str = strStream.str(); //str holds the content of the file

    const auto rawJsonLength = static_cast<int>(str.length());
    constexpr bool shouldUseOldWay = false;
    JSONCPP_STRING err;
    Json::Value root;

    if (shouldUseOldWay) {
        Json::Reader reader;
        reader.parse(str, root);
    } else {
        Json::CharReaderBuilder builder;
        const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        if (!reader->parse(str.c_str(), str.c_str() + rawJsonLength, &root, &err)) {
            std::cout << "Error parsing JSON file" << std::endl;
            return EXIT_FAILURE;
        }
    }

    std::string server_address = root["server_address"].asString();
    std::string client_id = root["client_id"].asString();

    // Mapping for topics and MEs
    std::unordered_map<std::string, int> sub_topics_mapping;
    std::unordered_map<int, std::string> pub_topics_mapping;

    for(auto sub_topic : root["sub_topics"]) {
        //sub_topics_name.push_back(sub_topic["topic"].asString());
        sub_topics_mapping.insert({sub_topic["topic"].asString(), sub_topic["me_type"].asInt()});
    }

    sub_topics_mapping.insert({KILL_IUOC_TOPIC, -1});

    for(auto pub_topic : root["pub_topics"]) {
        pub_debug.push_back(pub_topic["me_name"].asString());
        pub_topics_mapping.insert({pub_topic["me_type"].asInt(), pub_topic["topic"].asString()});
    }

    unsigned qos = root["qos"].asUInt();
    unsigned retry_attemps = root["retry_attemps"].asUInt();

    mqtt::async_client client(server_address, client_id);

    mqtt::connect_options connOpts;

    connOpts.set_clean_session(false);
    connOpts.set_user_name("iuocmqtt");
    connOpts.set_password("Shaft_Dreaded_Dosage");
    
    // Install the callback(s) before connecting.
    callback cb(client, connOpts, sub_topics_mapping, qos, retry_attemps);
    client.set_callback(cb);

    // Start the connection.
    // When completed, the callback will subscribe to topic.
    try {
        std::cout << "Connecting to the MQTT server..." << std::flush;
        client.connect(connOpts, nullptr, cb);
    }
    catch (const mqtt::exception& exc) {
        std::cerr << "\nERROR: Unable to connect to MQTT server: '" << server_address << "'" << exc << std::endl;
        return 1;
    }
        
    mqtt::token_ptr tok;

    int dev = open("/dev/soo/iuoc", O_WRONLY);
    if(dev == -1) {
        printf("[IUOC] Opening was not possible!\n");
        return EXIT_FAILURE;
    }

    while (cb.get_running_status() == false) { 
        sleep(1);
    }
    
    // Just block till user tells us to quit.
    while (cb.get_running_status() == true) {
        iuoc_data_t me_data;
        ioctl(dev, UIOC_IOCTL_RECV_DATA, &me_data);

        mqtt::topic top(client, pub_topics_mapping[me_data.me_type], qos);
        std::string payload = get_payload_from_data(&me_data);
        tok = top.publish(payload);
    }

    tok->wait();

    // Disconnect
    try {
        std::cout << "\nDisconnecting from the MQTT server..." << std::flush;
        client.disconnect()->wait();
        std::cout << "OK" << std::endl;
    }
    catch (const mqtt::exception& exc) {
        std::cerr << exc << std::endl;
        return 1;
    }

    close(dev);

     return 0;
}


std::string get_payload_from_data(iuoc_data_t *me_data)
{
    std::string json_payload = "";
    Json::Value root;
      Json::Value json_data;
    root["name"]       = "SOO-blind";
    root["cluster"]    = "REDS";
    root["location"]   = "HEIG-VD A23";

    for (int i = 0; i < me_data->data_array_size; i++) {
        root["data"][i]["name"] = me_data->data_array[i].name;
        root["data"][i]["type"] = me_data->data_array[i].type;
        root["data"][i]["value"] = me_data->data_array[i].value;
    }

    Json::StreamWriterBuilder builder;
    json_payload = Json::writeString(builder, root);
    return json_payload;
}


