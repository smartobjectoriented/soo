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
                   std::unordered_map<std::string, std::string> sub_topics, unsigned qos,
                   unsigned n_retry_attemps)
        : nretry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription"),
        sub_topics(sub_topics), qos(qos), n_retry_attemps(n_retry_attemps) {}

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
		std::cout << "Message arrived" << std::endl;
		std::cout << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
		std::cout << "\tpayload: '" << msg->to_string() << "'\n" << std::endl;
	}

	void callback::delivery_complete(mqtt::delivery_token_ptr token) {}
