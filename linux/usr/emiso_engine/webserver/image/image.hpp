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

#ifndef EMISO_IMAGE_H
#define EMISO_IMAGE_H

#include <iostream>
#include <httpserver.hpp>
#include <json/json.h>

#include "../../daemon/image.hpp"
#include "../../daemon/daemon.hpp"

namespace emiso {
namespace image {

    class ListHandler : public httpserver::http_resource {
    public:
        ListHandler(Daemon *daemon) : _daemon(daemon) {};

        const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) {
            std::string payload_str = "";
            Json::Value payload_json;

            std::cout << "[WEBERVER] '" << req.get_path()  << "' (" << req.get_method() << ") called" << std::endl;

            // Retrieve image info
            std::map<std::string, ImageInfo> info;
            _daemon->image.info(info);

            unsigned idx = 0;
            for (auto it = info.begin(); it != info.end(); ++it) {

                payload_json[idx]["Id"]          = it->second.id;
                payload_json[idx]["ParentId"]    = "";
                payload_json[idx]["RepoTags"][0] = it->second.name + ":latest";
                payload_json[idx]["Created"]     = it->second.date;
                payload_json[idx]["Size"]        = it->second.size;
                payload_json[idx]["SharedSize"]  = -1;  // Value not set or calculated
                payload_json[idx]["Containers"]  = -1; // Value not set or calculated
                payload_json[idx]["Labels"]      = Json::Value::null;

                idx++;
            }

            Json::StreamWriterBuilder builder;
            payload_str = Json::writeString(builder, payload_json);

            auto response = std::make_shared<httpserver::string_response>(payload_str,
                       httpserver::http::http_utils::http_ok, "application/json");
            return response;
        }

    private:
        // daemon::Image _image;
        Daemon *_daemon;

    };

    class ImageApi {
    public:
        // Constructor
        ImageApi(httpserver::webserver *server, Daemon *daemon);

        // Destructor
        ~ImageApi();

    private:
        httpserver::webserver *_server;

        // Handler for the different 'list' routes
        ListHandler *_listHandler;

    };
}
}

#endif /* EMISO_IMAGE_H */
