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

#include "image.hpp"

namespace emiso {

    namespace image {

        ImageApi::ImageApi(httpserver::webserver *server, Daemon *daemon)
            : _server(server) {
                std::string path  = "/images";

            // Create routes and handlers
            _listHandler = new ListHandler(daemon);

            _server->register_resource(path + "/json", _listHandler);
            _server->register_resource("/v[1-9]+.[0-9]+" + path + "/json", _listHandler);
        }

        ImageApi::~ImageApi() {}
    }
}
