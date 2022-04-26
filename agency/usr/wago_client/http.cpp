/*
 * Copyright (C) 2022 Mattia Gallacchi <mattia.gallacchi@heig-vd.ch>
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

#include <iostream>
#include <cstring>
#include <stdexcept>

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
        CURLcode res;

        if ((res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb)) != 0)
        {
            throw std::runtime_error(curl_easy_strerror(res));
        }

        if ((res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response)) != 0)
        {
            throw std::runtime_error(curl_easy_strerror(res));
        }
    }

    int Request::GET(std::string route, std::string &resp, std::string args)
    {
        CURLcode res;
        response.clear();

        std::string url = server_access + route + args;

        if ((res = curl_easy_setopt(curl, CURLOPT_URL, url.c_str())) != 0)
        {
            std::cout << curl_easy_strerror(res) << url << std::endl;
            return -1;
        }

        if ((res = curl_easy_setopt(curl, CURLOPT_HTTPGET, 1)) != 0)
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

        resp = response;

        return 0;
    }

    int Request::POST(std::string route, std::string args)
    {
        CURLcode res;
        response.clear();

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