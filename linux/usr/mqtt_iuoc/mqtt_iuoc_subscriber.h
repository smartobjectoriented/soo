#ifndef MQTT_IUOC_SUBSCRIBE
#define MQTT_IUOC_SUBSCRIBE

#include <mqtt/async_client.h>
#include <vector>
#include <json/json.h>
#include <unordered_map>

#include <soo/uapi/iuoc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define KILL_IUOC_TOPIC "kill-iuoc-client"

class action_listener : public virtual mqtt::iaction_listener
{
private:  
    std::string name_;

    void on_failure(const mqtt::token& tok) override;

    void on_success(const mqtt::token& tok) override;

public:
    action_listener(const std::string& name);
};


/**
 * Local callback & listener class for use with the client connection.
 * This is primarily intended to receive messages, but it will also monitor
 * the connection to the broker. If the connection is lost, it will attempt
 * to restore the connection and re-subscribe to the topic.
 */
class callback : public virtual mqtt::callback,
                    public virtual mqtt::iaction_listener

{
    // Counter for the number of connection retries
    int nretry_;
    // The MQTT client
    mqtt::async_client& cli_;
    // Options to use if we need to reconnect
    mqtt::connect_options& connOpts_;
    // An action listener to display the result of actions.
    action_listener subListener_;

    std::unordered_map<std::string, int> sub_topics;

    unsigned qos;
    unsigned n_retry_attemps;

    void reconnect();

    // Re-connection failure
    void on_failure(const mqtt::token& tok) override;

    // (Re)connection success
    // Either this or connected() can be used for callbacks.
    void on_success(const mqtt::token& tok) override;

    // (Re)connection success
    void connected(const std::string& cause) override;

    // Callback for when the connection is lost.
    // This will initiate the attempt to manually reconnect.
    void connection_lost(const std::string& cause) override;

    // Callback for when a message arrives.
    void message_arrived(mqtt::const_message_ptr msg) override;

    void delivery_complete(mqtt::delivery_token_ptr token) override;

    int dev;
    bool running_status;

public:

    bool get_running_status();

    callback(mqtt::async_client& cli, mqtt::connect_options& connOpts,
             std::unordered_map<std::string, int> sub_topics, unsigned qos, 
             unsigned n_retry_attemps);
};


#endif
