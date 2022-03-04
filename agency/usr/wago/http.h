#pragma once

#include <curl/curl.h>
#include <string>

namespace HTTP
{

    class Request
    {
    private:
        CURL *curl;
        std::string server_access;
        std::string response;

        void set_callback_func();

    public:
        Request(std::string srv_addr, std::string srv_port);
        ~Request();

        int GET(std::string route, std::string &resp, std::string args = "");
        int POST(std::string route, std::string args);
    };

} // namespace