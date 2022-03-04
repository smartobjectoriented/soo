#include <iostream>
#include <cstring>
#include "http.h"

namespace HTTP
{

    static size_t cb(void *ptr, size_t size, size_t nmemb, std::string* data)
    {
        data->append((char*)ptr, size * nmemb);
        return size * nmemb;
    }

    Request::Request(std::string srv_addr, std::string srv_port)
    {
        curl = curl_easy_init();
        server_access = srv_addr + ":" + srv_port + "/";
        set_callback_func();
    }

    Request::~Request()
    {
        curl_easy_cleanup(curl);
    }

    void Request::set_callback_func()
    {

        if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb) != 0)
        {
            std::cout << "Failed to set CURLOPT_WRITEFUNCTION" << std::endl;
        }

        if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response) != 0)
        {
            std::cout << "Failed to set CURLOPT_WRITEDATA" << std::endl;
        }
    }

    int Request::GET(std::string route, std::string &resp, std::string args)
    {
        CURLcode res;
        response.clear();

        std::string url = server_access + route + args;

        if (curl_easy_setopt(curl, CURLOPT_URL, url.c_str()) != 0)
        {
            std::cout << "Failed to set option URL : " << url << std::endl;
            return -1;
        }

        if (curl_easy_setopt(curl, CURLOPT_HTTPGET, 1) != 0)
        {
            std::cout << "Failed to set option HTTPGET" << std::endl;
            return -1;
        }

        res = curl_easy_perform(curl);
        if (res = CURLE_OK)
        {
            std::cout << "Failed to perform request" << std::endl;
            return -1;
        }

        resp = response;

        return 0;
    }

    int Request::POST(std::string route, std::string args)
    {
        CURLcode res;
        response.clear();

        // curl_easy_cleanup(curl);

        std::string url = server_access + route + args;

        if ((res = curl_easy_setopt(curl, CURLOPT_URL, url.c_str())) != 0)
        {
            std::cout << curl_easy_strerror(res) << url << std::endl;
            return -1;
        }
        
        if ((res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "")) != 0)
        {
            std::cout << curl_easy_strerror(res) << std::endl;
            return -1;
        }
        
        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            std::cout << curl_easy_strerror(res) << std::endl;
            return -1;
        }

        return 0;
    }
}