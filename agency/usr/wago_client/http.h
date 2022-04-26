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

#pragma once

#include <curl/curl.h>
#include <string>

namespace HTTP
{
    /**
     * @brief Class Request is used to perform HTTP request to a REST server.
     *        At the moment only GET and POST requests are implemented. More 
     *        will come as needed.
     */
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

} // end HTTP