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


/////////////////////////////////////////
#include <soo/uapi/iuoc.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
/////////////////////////////////////////

void test_ioctl() {
	iuoc_data_t test;
	test.me_type = IUOC_ME_SWITCH;
	test.timestamp = 123456;
	int dev = open("/dev/soo/iuoc", O_WRONLY);
	if(dev == -1) {
		printf("Opening was not possible!\n");
		return;
	}

	ioctl(dev, UIOC_IOCTL_TEST, &test);
	sleep(1);
	ioctl(dev, UIOC_IOCTL_SEND_DATA, &test);
	sleep(1);
	ioctl(dev, UIOC_IOCTL_RECV_DATA, &test);
	printf("USER SPACE : ME=%d\nmessage=%d\n", test.me_type, test.timestamp);

	sleep(1);
	ioctl(dev, UIOC_IOCTL_SEND_DATA, &test);
	sleep(1);
	ioctl(dev, UIOC_IOCTL_RECV_DATA, &test);
	printf("USER SPACE : ME=%d\nmessage=%d\n", test.me_type, test.timestamp);

	ioctl(dev, UIOC_IOCTL_TEST, &test);
	sleep(1);
	printf("Opening was successfull!\n");
	close(dev);
}

std::vector<std::string> topics_publish_name(IUOC_ME_END);

std::vector<std::string> pub_debug;

std::string get_payload_from_data(iuoc_data_t *data);

std::string get_json_blind_str(soo_blind_data_t *data);
std::string get_json_switch_str(soo_switch_data_t *data);

int main(int argc, char* argv[])
{
	//test_ioctl();
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
			std::cout << "Error parsing JSON file" << std::endl;
			return EXIT_FAILURE;
		}
	}

	std::string server_address = root["server_address"].asString();
	std::string client_id = root["client_id"].asString();

	// Mapping for topics and MEs
	std::unordered_map<std::string, int> sub_topics_mapping;
	std::unordered_map<int, std::string> pub_topics_mapping;

	// sub_topics_mapping.insert()

	for(auto sub_topic : root["sub_topics"]) {
		//sub_topics_name.push_back(sub_topic["topic"].asString());
		sub_topics_mapping.insert({sub_topic["topic"].asString(), sub_topic["me_type"].asInt()});
	}

	for(auto pub_topic : root["pub_topics"]) {
		pub_debug.push_back(pub_topic["me_name"].asString());
		pub_topics_mapping.insert({pub_topic["me_type"].asInt(), pub_topic["topic"].asString()});
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
		std::cerr << "\nERROR: Unable to connect to MQTT server: '" << server_address << "'" << exc << std::endl;
		return 1;
	}
		
	mqtt::token_ptr tok;

	int dev = open("/dev/soo/iuoc", O_WRONLY);
	if(dev == -1) {
		printf("[IUOC] Opening was not possible!\n");
		return EXIT_FAILURE;
	}

	// Just block till user tells us to quit.
	while (std::tolower(std::cin.get()) != 'q') {
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

	switch (me_data->me_type) {

		case IUOC_ME_BLIND : 
			root["name"]       = "SOO-blind";
			root["cluster"]    = "REDS";
			root["location"]   = "HEIG-VD A23";
			json_data["name"]  = "action";
			json_data["type"]  = "int";
			json_data["value"] = me_data->data.blind_data.action;
			break;

		case IUOC_ME_SWITCH : 
			root["name"]       = "SOO-switch";
			root["cluster"]    = "REDS";
			root["location"]   = "HEIG-VD A23";
			json_data["name"]  = "action";
			json_data["type"]  = "int";
			json_data["value"] = me_data->data.switch_data.action;
			break;
		default:
			break;
	}

	root["data"] = json_data;

	Json::StreamWriterBuilder builder;
    json_payload = Json::writeString(builder, root);
	return json_payload;
}


