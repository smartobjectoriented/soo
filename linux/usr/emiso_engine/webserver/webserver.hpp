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

#ifndef EMISO_WEBSERVER_H
#define EMISO_WEBSERVER_H

#include <httpserver.hpp>

#include "system/system.hpp"
#include "container/container.hpp"
#include "image/image.hpp"
#include "network/network.hpp"
#include "volume/volume.hpp"
#include "../daemon/daemon.hpp"

// DefaultHandler - used to print info for all paths not already registered to the
//                  server
class DefaultHandler : public httpserver::http_resource {
public:
    const std::shared_ptr<httpserver::http_response> render(const httpserver::http_request& req) {
        std::cout << "Received message on port " << 2375 << ":" << std::endl;
        std::cout << "Method: " << req.get_method() << std::endl;
        std::cout << "Path: " << req.get_path() << std::endl;
        std::cout << "Headers:" << std::endl;
        for (const auto& header : req.get_headers()) {
            std::cout << "    " << header.first << ": " << header.second << std::endl;
        }
        std::cout << "Body: " << req.get_content() << std::endl;

        // Respond with a simple message
        std::string message = "Path '" + req.get_path() + "' not implemented\n";

        auto response = std::make_shared<httpserver::string_response>(message);
        return response;
    }
};

namespace emiso {

    class WebServer {
    public:
        // Constructor
        WebServer(int port, bool secure, Daemon *daemon);

        // Destructor
        ~WebServer();

        // Start the server
        void start(bool blocking = false);

        // Stop the server
        // void stop();

    private:
        int _port;
        bool _secure;
        httpserver::webserver *_server;

        // Different services
        system::SytemApi *_system;
        container::ContainerApi *_container;
        image::ImageApi *_image;
        network::NetworkApi *_network;
        volume::VolumeApi *_volume;

        // Default handler - not registered routes
        DefaultHandler  *_defaultHandler;

    };
}

#endif /* EMISO_WEBSERVER_H */
