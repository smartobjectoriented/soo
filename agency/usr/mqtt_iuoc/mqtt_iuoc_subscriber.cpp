#include "mqtt_iuoc_subscriber.h"

action_listener::action_listener(const std::string& name) : name_(name) {}

void action_listener::on_failure(const mqtt::token& tok) {
    std::cout << name_ << " failure";
    if (tok.get_message_id() != 0)
        std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
    std::cout << std::endl;
}

void action_listener::on_success(const mqtt::token& tok) {
    std::cout << name_ << " success";
    if (tok.get_message_id() != 0)
        std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
    auto top = tok.get_topics();
    if (top && !top->empty())
        std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
    std::cout << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////////

callback::callback(mqtt::async_client& cli, mqtt::connect_options& connOpts, 
		std::unordered_map<std::string, int> sub_topics, unsigned qos,
		unsigned n_retry_attemps)
        : nretry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription"),
        sub_topics(sub_topics), qos(qos), n_retry_attemps(n_retry_attemps) {
	dev = open("/dev/soo/iuoc", O_WRONLY);
	if(dev == -1) {
		printf("[IUOC] Opening was not possible!\n");
	}
}

void callback::reconnect() {
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
void callback::on_failure(const mqtt::token& tok) {
	std::cout << "Connection attempt failed" << std::endl;
	if (++nretry_ > n_retry_attemps)
		exit(1);
	reconnect();
}

// (Re)connection success
// Either this or connected() can be used for callbacks.
void callback::on_success(const mqtt::token& tok) {}

// (Re)connection success
void callback::connected(const std::string& cause) {
	std::cout << "\nConnection success" << std::endl;
	std::cout << "\nSubscribing to topic ";
	for(auto const& topic : sub_topics) {
		std::cout << "'" << topic.first << "' ";		
	}
	
	for(auto const& topic : sub_topics) {
		cli_.subscribe(topic.first, qos, nullptr, subListener_);
	}
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
void callback::message_arrived(mqtt::const_message_ptr msg) {
	// std::cout << "Message arrived" << std::endl;
	// std::cout << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
	// std::cout << "\tpayload: '" << msg->to_string() << "'\n" << std::endl;

	iuoc_data_t me_data;
	auto me_type = sub_topics[msg->get_topic()];
	me_data.me_type = (me_type_t)me_type;

	// TODO: define where is the timestamp set
	me_data.timestamp = 555666;

	// Get data from payload with JSON reader
	Json::CharReaderBuilder builder;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
	JSONCPP_STRING err;
  	Json::Value root;

    if (!reader->parse(msg->to_string().c_str(), msg->to_string().c_str() + msg->to_string().length(), 
		&root,  &err)) {
      std::cout << "[IUOC] error converting JSON payload to string" << std::endl;
    }

	switch (me_type) {
		case IUOC_ME_BLIND: 
			me_data.data.blind_data.action = (soo_blind_action_t)root["data"]["value"].asInt();
			break;

		case IUOC_ME_SWITCH: 
			me_data.data.blind_data.action = (soo_blind_action_t)root["data"]["value"].asInt();
			break;
		default:
			break; 
	}

	ioctl(dev, UIOC_IOCTL_SEND_DATA, &me_data);

}

void callback::delivery_complete(mqtt::delivery_token_ptr token) {}
