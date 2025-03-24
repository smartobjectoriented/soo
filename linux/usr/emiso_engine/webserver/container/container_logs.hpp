/*
 * Copyright (C) 2024 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
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

#ifndef EMISO_CONTAINER_LOGS_H
#define EMISO_CONTAINER_LOGS_H

namespace emiso {
namespace container {

class LogsHandler : public httpserver::http_resource {
    public:
        LogsHandler(Daemon *daemon) : _daemon(daemon) {};

        std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) {
            // std::string payload_str = "";
            // Json::Value payloadJson;

            std::cout << "[WEBERVER] '" << std::string(req.get_path())  << "' (" << std::string(req.get_method()) << ") called" << std::endl;

			// == Retrieve container ID from the request path ==
            int containerId = std::__cxx11::stoi(req.get_arg("id"));
            int lineNr      = std::__cxx11::stoi(req.get_arg("tail"));

			std::cout << "container ID: " << containerId << std::endl;

			// == Retrieve QUERY params ==
            for (const auto& arg : req.get_args()) {
            	std::cout << "param: " << std::string(arg.first) << ", value: " << std::string(arg.second) << std::endl;
            }

            std::vector<std::string> lines;
            lines = _daemon->container.retrieveLogs(containerId, lineNr);

            std::string message;
            for (int i = 0; i < lines.size(); i++) {
                message += lines[i] + "\n";
            }

            auto response = std::make_shared<httpserver::string_response>(message,
                           httpserver::http::http_utils::http_ok, "application/json");
            return response;
        }

    private:
        Daemon *_daemon;
    };


} // container
} // emiso

#endif // EMISO_CONTAINER_LOGS_H