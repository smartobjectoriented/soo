/*
 * Copyright (C) 2023 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
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

#ifndef EMISO_VOLUME_H
#define EMISO_VOLUME_H

#include <iostream>
#include <httpserver.hpp>
#include <json/json.h>

namespace emiso {

    namespace volume {

        class ListHandler : public httpserver::http_resource {
        public:

            std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) {
                std::string payload_str = "[]";

                std::cout << "[WEBERVER] '" << req.get_path()  << "' (" << req.get_method() << ") called" << std::endl;

                auto response = std::make_shared<httpserver::string_response>(payload_str,
                           httpserver::http::http_utils::http_ok, "application/json");
                return response;
            }
        };

        class VolumeApi {
        public:
            // Constructor
            VolumeApi(httpserver::webserver *server);

            // Destructor
            ~VolumeApi();

        private:
            httpserver::webserver *_server;

            // Handler for the different 'list' routes
            ListHandler *_listHandler;

        };
    }
}

#endif /* EMISO_VOLUME_H */
