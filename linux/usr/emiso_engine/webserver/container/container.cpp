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

#include "container.hpp"

namespace emiso {

    namespace container {

        ContainerApi::ContainerApi(httpserver::webserver *server, Daemon *daemon)
            : _server(server) {
            std::string path  = "/containers";

            // Create routes and handlers
            _listHandler   = new ListHandler(daemon);
            _createHandler = new CreateHandler(daemon);
            _startHandler  = new StartHandler(daemon);
            _stopHandler    = new StopHandler(daemon);
            _restartHandler  = new RestartHandler(daemon);
            _pauseHandler   = new PauseHandler(daemon);
            _unpauseHandler = new UnpauseHandler(daemon);
            _removeHandler = new RemoveHandler(daemon);
            _inspectHandler = new InspectHandler(daemon);

            _server->register_resource(path + "/json", _listHandler);
            _server->register_resource("/v[1-9]+.[0-9]+" + path + "/json", _listHandler);
            _server->register_resource(path + "/create", _createHandler);
            _server->register_resource("/v[1-9]+.[0-9]+" + path + "/create", _createHandler);

            _server->register_resource(path + "/[1-9]+/start", _startHandler);
            _server->register_resource("/v[1-9]+.[0-9]+" + path + "[0-9]+/start", _startHandler);

            _server->register_resource(path + "/[1-9]+/stop", _stopHandler);
            _server->register_resource("/v[1-9]+.[0-9]+" + path + "[0-9]+/stop", _stopHandler);

            _server->register_resource(path + "/[1-9]+/restart", _restartHandler);
            _server->register_resource("/v[1-9]+.[0-9]+" + path + "[0-9]+/restart", _restartHandler);

            _server->register_resource(path + "/[1-9]+/pause", _pauseHandler);
            _server->register_resource("/v[1-9]+.[0-9]+" + path + "[0-9]+/pause", _pauseHandler);

            _server->register_resource(path + "/[1-9]+/unpause", _unpauseHandler);
            _server->register_resource("/v[1-9]+.[0-9]+" + path + "[0-9]+/unpause", _unpauseHandler);

            _server->register_resource(path + "/[1-9]+", _removeHandler);
            _server->register_resource("/v[1-9]+.[0-9]+" + path + "[0-9]+", _removeHandler);

            _server->register_resource(path + "/{id|[0-9]+}/json", _inspectHandler);
            _server->register_resource("/v[1-9]+.[0-9]+" + path + "/{id|[0-9]+}/json", _inspectHandler);

        }

        ContainerApi::~ContainerApi() {}

    }
}
