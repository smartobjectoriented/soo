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
#include <iostream>

action_listener::action_listener(const std::string& name) : name_(name) {}

void action_listener::on_failure(const mqtt::token& tok) 
{
	std::cout << name_ << " failure";
	if (tok.get_message_id() != 0)
		std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
	std::cout << std::endl;
}

void action_listener::on_success(const mqtt::token& tok) 
{
	std::cout << name_ << " success";
	if (tok.get_message_id() != 0)
		std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
	auto top = tok.get_topics();
	if (top && !top->empty())
		std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
	std::cout << std::endl;
}


callback::callback(mqtt::async_client& cli, mqtt::connect_options& connOpts, 
		std::unordered_map<std::string, int> sub_topics, unsigned qos,
		unsigned n_retry_attemps)
		: nretry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription"),
		sub_topics(sub_topics), qos(qos), n_retry_attemps(n_retry_attemps), running_status(false)
	{
	dev = open("/dev/soo/iuoc", O_WRONLY);
	if(dev == -1) {
		printf("[IUOC] Opening was not possible!\n");
	}
}

void callback::reconnect() 
{
	std::this_thread::sleep_for(std::chrono::milliseconds(2500));
	try {
		cli_.connect(connOpts_, nullptr, *this);
	}
	catch (const mqtt::exception& exc) {
		std::cerr << "Error: " << exc.what() << std::endl;
		exit(1);
	}
}

// Re-connection failure
void callback::on_failure(const mqtt::token& tok) 
{
	std::cout << "Connection attempt failed" << std::endl;
	if (++nretry_ > n_retry_attemps)
		exit(1);
	reconnect();
}

// (Re)connection success
// Either this or connected() can be used for callbacks.
void callback::on_success(const mqtt::token& tok) {}

// (Re)connection success
void callback::connected(const std::string& cause) 
{
	std::cout << "\nConnection success" << std::endl;
	std::cout << "\nSubscribing to topic ";
	for(auto const& topic : sub_topics) {
		std::cout << "'" << topic.first << "' ";		
	}
	
	for(auto const& topic : sub_topics) {
		cli_.subscribe(topic.first, qos, nullptr, subListener_);
	}
	running_status = true;
}

// Callback for when the connection is lost.
// This will initiate the attempt to manually reconnect.
void callback::connection_lost(const std::string& cause) {
	std::cout << "\nConnection lost" << std::endl;
	if (!cause.empty())
		std::cout << "\tcause: " << cause << std::endl;

	std::cout << "Reconnecting..." << std::endl;
	nretry_ = 0;
	reconnect();
}



// Callback for when a message arrives.
void callback::message_arrived(mqtt::const_message_ptr msg) 
{
 
	if (msg->get_topic() == KILL_IUOC_TOPIC) {
		running_status = false;
		return;
	} 

	iuoc_data_t me_data;
	auto me_type = sub_topics[msg->get_topic()];
	me_data.me_type = (me_type_t)me_type;

	std::cout << "New message recieved : " << std::endl;
	std::cout << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
	std::cout << "\tpayload: '" << msg->to_string() << "'\n" << std::endl;

	// TODO: define where is the timestamp set
	me_data.timestamp = 555666;

	// Get data from payload with JSON reader
	Json::CharReaderBuilder builder;
	const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
	JSONCPP_STRING err;
	  Json::Value root;

	if (!reader->parse(msg->to_string().c_str(), msg->to_string().c_str() + msg->to_string().length(), 
		&root,  &err)) {
	//   std::cout << "[IUOC] error converting JSON payload to string" << std::endl;
	}

	field_data_t field_data;
	if (me_type == IUOC_ME_BLIND) {
		me_data.data_array_size = 2;
		strcpy(me_data.data_array[0].name, "direction");
		strcpy(me_data.data_array[0].type, "int");
		strcpy(me_data.data_array[1].name, "action_mode");
		strcpy(me_data.data_array[1].type, "int");

		if (root["data"][0]["value"] == "down") {
			std::cout << "[USR] going down" << std::endl;
			me_data.data_array[0].value = 1;
			me_data.data_array[1].value = 0;
			ioctl(dev, UIOC_IOCTL_SEND_DATA, &me_data);
		} else if (root["data"][0]["value"] == "down step") {
			std::cout << "[USR] going down step" << std::endl;
			me_data.data_array[0].value = 1;
			me_data.data_array[1].value = 1;
			ioctl(dev, UIOC_IOCTL_SEND_DATA, &me_data);
		} else if (root["data"][0]["value"] == "up") {
			std::cout << "[USR] going up" << std::endl;
			me_data.data_array[0].value = 0;
			me_data.data_array[1].value = 0;
			ioctl(dev, UIOC_IOCTL_SEND_DATA, &me_data);
		} else if (root["data"][0]["value"] == "up step") {
			std::cout << "[USR] going up step" << std::endl;
			me_data.data_array[0].value = 0;
			me_data.data_array[1].value = 1;
			ioctl(dev, UIOC_IOCTL_SEND_DATA, &me_data);
		}
		
	}
		
	return;

	// me_data.data_array_size = root["data"].size();
	// for(int i = 0; i < root["data"].size(); i++) {
	// 	field_data_t field_data;
	// 	strcpy(field_data.name, root["data"][i]["name"].asString().c_str());
	// 	strcpy(field_data.type, root["data"][i]["type"].asString().c_str());
	// 	field_data.value = root["data"][i]["value"].asInt();
	// 	me_data.data_array[i] = field_data;
	// }
	// ioctl(dev, UIOC_IOCTL_SEND_DATA, &me_data);
}

bool callback::get_running_status()
{
	return running_status;
}


void callback::delivery_complete(mqtt::delivery_token_ptr token) {}
