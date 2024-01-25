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

#include <iostream>

#include "webserver.hpp"

namespace emiso {

    WebServer::WebServer(int port, bool secure, Daemon *daemon)
        : _port(port), _secure(secure)  {

        // Use builder to define webserver configuration options
        httpserver::create_webserver cw = httpserver::create_webserver(_port);
        if (secure)
            cw.use_ssl().https_mem_key("key.pem").https_mem_cert("cert.pem");

        // Create webserver using the configured options
        _server = new httpserver::webserver(cw);

        // Registration of the different APIs
        _system    = new system::SytemApi(_server, daemon);
        _container = new container::ContainerApi(_server, daemon);
        _image     = new image::ImageApi(_server, daemon);
        _network   = new network::NetworkApi(_server);
        _volume    = new volume::VolumeApi(_server);

        // Create the default path - it responds to all paths
        _defaultHandler = new DefaultHandler();
        _server->register_resource("^/.*$",  _defaultHandler);
    }

    WebServer:: ~WebServer() {}

    void WebServer::start(bool blocking)
    {
        // Start the server
        std::cout << "Server started on port " << _port;
        if (_secure)
                std::cout << " (HTTPS/TLS mode ON)";
        std::cout << std::endl;

        _server->start(blocking);
    }
}
