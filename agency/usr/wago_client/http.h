#pragma once

#include <curl/curl.h>
#include <string>

namespace HTTP
{

    class Request
    {
    private:
        /** Curl struct **/
        CURL *curl;

        /** base server address: <ip>:<port>/ **/
        std::string server_access;

        /** Response buffer **/
        std::string response;

        /**
         * @brief Set the callback func object for the curl struct
         * 
         */
        void set_callback_func();

    public:
        /**
         * @brief Construct a new Request object
         * 
         * @param srv_addr server IP address
         * @param srv_port server port number
         */
        Request(std::string srv_addr, std::string srv_port);

        /**
         * @brief Destroy the Request object
         * 
         */
        ~Request();

        /**
         * @brief Send a GET request to the server
         * 
         * @param route REST API route
         * @param resp Buffer for server response
         * @param args Request arguments
         * @return int 0 on success, -1 on error
         */
        int GET(std::string route, std::string &resp, std::string args = "");

        /**
         * @brief Send a POST request to the server
         * 
         * @param route REST API route
         * @param args Request arguments
         * @return int 0 on success, -1 on error
         */
        int POST(std::string route, std::string args);
    };

} // namespace