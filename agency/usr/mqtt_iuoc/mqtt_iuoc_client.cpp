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

std::vector<std::string> pub_debug;

int main(int argc, char* argv[])
{

	std::ifstream inFile;
    inFile.open("iuoc_mqtt_config.json"); //open the config file

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
			std::cout << "error" << std::endl;
			return EXIT_FAILURE;
		}
	}

	std::string server_address = root["server_address"].asString();
	std::string client_id = root["client_id"].asString();

	// Mapping for topics and MEs
	std::unordered_map<std::string, std::string> sub_topics_mapping;
	std::unordered_map<std::string, std::string> pub_topics_mapping;

	// sub_topics_mapping.insert()

	for(auto sub_topic : root["sub_topics"]) {
		//sub_topics_name.push_back(sub_topic["topic"].asString());
		sub_topics_mapping.insert({sub_topic["topic"].asString(), sub_topic["me_name"].asString()});
	}

	for(auto pub_topic : root["pub_topics"]) {
		pub_debug.push_back(pub_topic["me_name"].asString());
		pub_topics_mapping.insert({pub_topic["me_name"].asString(), pub_topic["topic"].asString()});
	}

	unsigned qos = root["qos"].asUInt();
	unsigned retry_attemps = root["retry_attemps"].asUInt();

	mqtt::async_client client(server_address, client_id);

	mqtt::connect_options connOpts;
	connOpts.set_clean_session(false);

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
		std::cerr << "\nERROR: Unable to connect to MQTT server: '"
			<< server_address << "'" << exc << std::endl;
		return 1;
	}

  // test purpose 
  int count = 0;
  	
  mqtt::token_ptr tok;

	// Just block till user tells us to quit.
	while (std::tolower(std::cin.get()) != 'q') {
		mqtt::topic top(client, pub_topics_mapping[pub_debug.at((count++) % pub_debug.size())], qos);
		tok = top.publish("It's a message !");
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

 	return 0;
}
