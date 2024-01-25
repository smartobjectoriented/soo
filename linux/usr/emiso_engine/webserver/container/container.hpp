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

#ifndef EMISO_CONTAINER_H
#define EMISO_CONTAINER_H

#include <iostream>
#include <vector>
#include <httpserver.hpp>
#include <json/json.h>
#include <regex>

#include "../../daemon/daemon.hpp"

#include "container_inspect.hpp"
// #include "../../daemon/container.hpp"
#include "../../daemon/daemon.hpp"

namespace emiso {
namespace container {

    class ListHandler : public httpserver::http_resource {
    public:
        ListHandler(Daemon *daemon) : _daemon(daemon) {};

        const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) {
            std::string payload_str = "";
            Json::Value payload_json;

            std::cout << "[WEBERVER] '" << req.get_path()  << "' (" << req.get_method() << ") called" << std::endl;

            // Retrieve the command args -
            // only "all" and "filters/name" args are handled
            bool allArg  = false;
            bool sizeArg = false;
            std::string filtersArg = "";
            for (const auto& arg : req.get_args()) {
                if (arg.first == "all") {
                    allArg = (arg.second == "true");
                }
                if (arg.first == "filters") {
                    filtersArg = arg.second;
                }
            }

             std::vector<std::string> namesFilter;
            if (!filtersArg.empty()) {
                Json::CharReaderBuilder readerBuilder;
                Json::Value filtersJsonValue;
                std::istringstream filtersJsonStream(filtersArg);

                if (!Json::parseFromStream(readerBuilder, filtersJsonStream, &filtersJsonValue, nullptr)) {
                    // Parsing failed, handle the error as needed
                    std::cerr << "Error parsing JSON!" << std::endl;
                }

                if (filtersJsonValue.isMember("name")) {
                    for (int i = 0; i < filtersJsonValue["name"].size(); ++i) {
                        namesFilter.push_back(filtersJsonValue["name"][i].asString());
                    }
                }
            }

            // Retrieve container info
            std::map<int, ContainerInfo> info;
            _daemon->container.info(info);

            if (info.empty()) {
                payload_json = Json::arrayValue;
            } else {

                unsigned idx = 0;
                for (auto it = info.begin(); it != info.end(); ++it) {

                    // == Checks the args passed to the command ==

                    // 'all' - if true, return all containers. else only running
                    // containers are returned.
                    if ((!allArg) && (it->second.state != "running")) {
                        // Only return running containers
                        continue;
                    }

                    // 'filters' (only name filter is supported)
                    bool nameFound = false;
                    if (!namesFilter.empty()) {
                        for (const auto &pattern : namesFilter) {
                            std::regex regexPattern(pattern);
                             if (std::regex_match("/" + it->second.name, regexPattern)) {
                                nameFound = true;
                                break;
                             }
                        }
                    } else {
                        nameFound = true;
                    }

                    if (!nameFound) {
                        continue;
                    }

                    // == Return info on the container ==

                    ImageInfo imageInfo;
                    _daemon->image.info(it->second.image, imageInfo);

                    payload_json[idx]["Id"]       =  std::to_string(it->second.id);
                    payload_json[idx]["Names"][0] = "/" + it->second.name;
                    payload_json[idx]["Image"]    = it->second.image + ":latest";
                    payload_json[idx]["ImageID"]  = imageInfo.id;
                    payload_json[idx]["Command"]  = "/inject";

                    payload_json[idx]["Created"] = it->second.created;
                    payload_json[idx]["Ports"]   = Json::arrayValue;
                    payload_json[idx]["Labels"]  = Json::objectValue;
                    payload_json[idx]["State"]   = it->second.state;
                    payload_json[idx]["Status"]  = it->second.state;

                    // Network setting as the values obtained on the test PC
                    payload_json[idx]["HostConfig"]["NetworkMode"] = "bridge";
                    payload_json[idx]["NetworkSettings"]["Networks"]["bridge"]["IPAMConfig"] = Json::objectValue;
                    payload_json[idx]["NetworkSettings"]["Networks"]["bridge"]["Links"]      = Json::Value::null;
                    payload_json[idx]["NetworkSettings"]["Networks"]["bridge"]["Aliases"]    = Json::Value::null;
                    payload_json[idx]["NetworkSettings"]["Networks"]["bridge"]["NetworkID"]  = "59125e01bc5d820b2cf1a2e19760b11f25678e098bb08df6d3337ef18403ca56",
                    payload_json[idx]["NetworkSettings"]["Networks"]["bridge"]["EndpointID"] = "";
                    payload_json[idx]["NetworkSettings"]["Networks"]["bridge"]["Gateway"]    = "";
                    payload_json[idx]["NetworkSettings"]["Networks"]["bridge"]["IPAddress"]  = "";
                    payload_json[idx]["NetworkSettings"]["Networks"]["bridge"]["IPPrefixLen"] = 0;
                    payload_json[idx]["NetworkSettings"]["Networks"]["bridge"]["IPv6Gateway"] = "";
                    payload_json[idx]["NetworkSettings"]["Networks"]["bridge"]["GlobalIPv6Address"] = "";
                    payload_json[idx]["NetworkSettings"]["Networks"]["bridge"]["GlobalIPv6PrefixLen"] = 0;
                    payload_json[idx]["NetworkSettings"]["Networks"]["bridge"]["MacAddress"] = "";
                    payload_json[idx]["NetworkSettings"]["Networks"]["bridge"]["DriverOpts"] = Json::Value::null;
                    payload_json[idx]["Mounts"] =   Json::arrayValue;

                   idx++;
               }
            }

            Json::StreamWriterBuilder builder;
            payload_str = Json::writeString(builder, payload_json);
            auto response = std::make_shared<httpserver::string_response>(payload_str,
                           httpserver::http::http_utils::http_ok, "application/json");
            return response;
        }

    private:
        // daemon::Container _container;
        Daemon *_daemon;
    };


    class CreateHandler : public httpserver::http_resource {
    public:
        CreateHandler(Daemon *daemon) : _daemon(daemon) {};

        const std::shared_ptr<httpserver::http_response> render_POST(const httpserver::http_request &req) {
            std::string payload_str = "";
            Json::Value payload_json;
            int containerId;

            std::cout << "[WEBERVER] '" << req.get_path()  << "' (" << req.get_method() << ") called" << std::endl;


            // Extract info from body
            const std::string requestBody = req.get_content();

            Json::Value jsonData;
            Json::Reader jsonReader;

            if (!jsonReader.parse(requestBody, jsonData)) {
                // JSON parsing error
                // Return error message
            }

            std::string imageName = jsonData["Image"].asString();
            std::string containerName = jsonData["name"].asString();

            // Remove the image version of the name
            size_t colonPosition = imageName.find(':');
            imageName = imageName.substr(0, colonPosition);

            std::cout << "[Webserver] Create container based on '" << imageName << "' name" << std::endl;

            containerId = _daemon->container.create(imageName, containerName);

            // build the response
            payload_json["Id"] = std::to_string(containerId);
            payload_json["Warnings"] = Json::arrayValue;

            Json::StreamWriterBuilder builder;
            payload_str = Json::writeString(builder, payload_json);

            auto response = std::make_shared<httpserver::string_response>(payload_str,
                       httpserver::http::http_utils::http_created, "application/json");
            return response;
        }

    private:
        // daemon::Container _container;
        Daemon *_daemon;
    };

    class StartHandler : public httpserver::http_resource {
    public:
        StartHandler(Daemon *daemon) : _daemon(daemon) {};

        const std::shared_ptr<httpserver::http_response> render_POST(const httpserver::http_request &req) {
            std::string payload_str = "";
            Json::Value payload_json;

            std::cout << "[WEBERVER] '" << req.get_path()  << "' (" << req.get_method() << ") called" << std::endl;

            // == Retrieve container ID from the request path ==
            int containerId = stoi(req.get_arg("id"));

            _daemon->container.start(containerId);

            auto response = std::make_shared<httpserver::string_response>(payload_str,
                       httpserver::http::http_utils::http_no_content, "application/json");
            return response;
        }

    private:
        Daemon *_daemon;
    };



    class StopHandler : public httpserver::http_resource {
    public:
         StopHandler(Daemon *daemon) : _daemon(daemon) {};

        const std::shared_ptr<httpserver::http_response> render_POST(const httpserver::http_request &req) {
            std::string payload_str = "";
            Json::Value payload_json;

            std::cout << "[WEBERVER] '" << req.get_path()  << "' (" << req.get_method() << ") called" << std::endl;

            // == Retrieve container ID from the request path ==
            int containerId = stoi(req.get_arg("id"));

            _daemon->container.stop(containerId);

            auto response = std::make_shared<httpserver::string_response>(payload_str,
                       httpserver::http::http_utils::http_no_content, "application/json");
            return response;
        }

    private:
        // daemon::Container _container;
        Daemon *_daemon;
    };


    class RestartHandler : public httpserver::http_resource {
    public:
        RestartHandler(Daemon *daemon) : _daemon(daemon) {};

        const std::shared_ptr<httpserver::http_response> render_POST(const httpserver::http_request &req) {
            std::string payload_str = "";
            Json::Value payload_json;

            std::cout << "[WEBERVER] '" << req.get_path()  << "' (" << req.get_method() << ") called" << std::endl;

            // == Retrieve container ID from the request path ==
            int containerId = stoi(req.get_arg("id"));

            _daemon->container.restart(containerId);

            auto response = std::make_shared<httpserver::string_response>(payload_str,
                       httpserver::http::http_utils::http_no_content, "application/json");
            return response;
        }

    private:
        // daemon::Container _container;
        Daemon *_daemon;
    };

    class PauseHandler : public httpserver::http_resource {
    public:
        PauseHandler(Daemon *daemon) : _daemon(daemon) {};

        const std::shared_ptr<httpserver::http_response> render_POST(const httpserver::http_request &req) {
            std::string payload_str = "";
            Json::Value payload_json;

            std::cout << "[WEBERVER] '" << req.get_path()  << "' (" << req.get_method() << ") called" << std::endl;

            // == Retrieve container ID from the request path ==
            int containerId = stoi(req.get_arg("id"));

            _daemon->container.pause(containerId);

            auto response = std::make_shared<httpserver::string_response>(payload_str,
                       httpserver::http::http_utils::http_no_content, "application/json");
            return response;
        }

    private:
        Daemon *_daemon;

    };

    class UnpauseHandler : public httpserver::http_resource {
    public:
         UnpauseHandler(Daemon *daemon) : _daemon(daemon) {};

        const std::shared_ptr<httpserver::http_response> render_POST(const httpserver::http_request &req) {
            std::string payload_str = "";
            Json::Value payload_json;

            std::cout << "[WEBERVER] '" << req.get_path()  << "' (" << req.get_method() << ") called" << std::endl;

            // == Retrieve container ID from the request path ==
            int containerId = stoi(req.get_arg("id"));

            _daemon->container.unpause(containerId);

            auto response = std::make_shared<httpserver::string_response>(payload_str,
                       httpserver::http::http_utils::http_no_content, "application/json");
            return response;
        }

    private:
        Daemon *_daemon;
    };


    class RemoveHandler : public httpserver::http_resource {
    public:
        RemoveHandler(Daemon *daemon) : _daemon(daemon) {};

        const std::shared_ptr<httpserver::http_response> render_DELETE(const httpserver::http_request &req) {
            std::string payload_str = "";
            Json::Value payload_json;

            std::cout << "[WEBERVER] '" << req.get_path()  << "' (" << req.get_method() << ") called" << std::endl;

            // == Retrieve container ID from the request path ==
            int containerId = stoi(req.get_arg("id"));

            _daemon->container.remove(containerId);

            auto response = std::make_shared<httpserver::string_response>(payload_str,
                       httpserver::http::http_utils::http_no_content, "application/json");
            return response;
        }

    private:
        Daemon *_daemon;
    };

    class ContainerApi {
    public:
        // Constructor
        ContainerApi(httpserver::webserver *server, Daemon *daemon);

        // Destructor
        ~ContainerApi();

    private:
        httpserver::webserver *_server;

        // Handler for the different 'container' routes
        ListHandler    *_listHandler;
        CreateHandler  *_createHandler;
        StartHandler   *_startHandler;
        StopHandler    *_stopHandler;
        PauseHandler   *_pauseHandler;
        UnpauseHandler *_unpauseHandler;
        RestartHandler *_restartHandler;
        RemoveHandler  *_removeHandler;
        InspectHandler *_inspectHandler;
    };

} // container
} // emiso

#endif /* EMISO_CONTAINER_H */
