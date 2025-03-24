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

#include "system.hpp"

namespace emiso {
    namespace system {

        SytemApi::SytemApi(httpserver::webserver *server, Daemon *daemon)
            : _server(server) {

            // Create routes and handlers
            _pinghandler    = new PingHandler();
            _sysInfoHandler = new SysInfoHandler(daemon);
            _getVersionHandler = new GetVersionHandler();

            _server->register_resource("/_ping", _pinghandler);
            _server->register_resource("/v[1-9]+.[0-9]+/_ping", _pinghandler);
            _server->register_resource("/v[1-9]+.[0-9]+/info",  _sysInfoHandler);
            _server->register_resource("/info",  _sysInfoHandler);
            _server->register_resource("/version", _getVersionHandler);
            _server->register_resource("/v[1-9]+.[0-9]+/version", _getVersionHandler);
        }

        SytemApi::~SytemApi() {}
    }
}
