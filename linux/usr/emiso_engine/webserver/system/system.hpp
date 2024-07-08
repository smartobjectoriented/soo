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

#ifndef EMISO_SYSTEM_H
#define EMISO_SYSTEM_H

#include <iostream>
#include <httpserver.hpp>
#include <json/json.h>

#include <emiso/utils.hpp>
#include <emiso/config.hpp>

#include "../../daemon/image.hpp"

namespace emiso {
    namespace system {

        class PingHandler : public httpserver::http_resource {
        public:

            const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) {
                // Respond is a simple 'OK'
                auto response = std::make_shared<httpserver::string_response>("OK");
                return response;
            }

            const std::shared_ptr<httpserver::http_response> render_HEAD(const httpserver::http_request &req) {
                auto response = std::shared_ptr<httpserver::http_response>(new httpserver::string_response(""));

                response->with_header("Api-Version",         EMISO_WEB_API_VERSION);
                response->with_header("Builder-Version",     EMISO_VERSION);
                if (EMISO_WEB_EXPERIMENTAL)
                    response->with_header("Docker-Experimental", "true");
                else
                    response->with_header("Docker-Experimental", "false");
                response->with_header("Swarm",               EMISO_WEB_SWARM);
                response->with_header("Cache-Control",       "no-cache, no-store, must-revalidate");
                response->with_header("Pragma",              "no-cache");

                return response;
            }
        };

        class SysInfoHandler : public httpserver::http_resource {
        public:

            const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) {
                std::string payload_str = "";
                Json::Value payload_json;
                Utils& utils = Utils::getInstance();
                auto config = utils.getInfo();
                daemon::Image image;
                std::map<std::string, daemon::ImageInfo> info;

                image.info(info);
                auto image_nr = info.size();

                payload_json["ID"]         =  utils.getAgencyUID();
                payload_json["Containers"] = 0; // To update next
                payload_json["ContainersRunning"] = 0; // To update next
                payload_json["ContainersPaused"] = 0; // To update next
                payload_json["ContainersStopped"] = 0; // To update next
                payload_json["Images"] = image_nr; // To update next

                // WARNING - hardcoded values got from Docker answer of JMI PC
                payload_json["Driver"] = "overlay2";
                payload_json["DriverStatus"][0].append("Backing Filesystem");
                payload_json["DriverStatus"][0].append("extfs");
                payload_json["DriverStatus"][1].append("Supports d_type");
                payload_json["DriverStatus"][1].append("true");
                payload_json["DriverStatus"][2].append("Using metacopy");
                payload_json["DriverStatus"][2].append("false");
                payload_json["DriverStatus"][3].append("Native Overlay Diff");
                payload_json["DriverStatus"][3].append("true");
                payload_json["DriverStatus"][4].append("userxattr");
                payload_json["DriverStatus"][4].append("false");

                payload_json["DockerRootDir"] = "/var/lib/docker"; // default value on Linux
                payload_json["Plugins"] = Json::objectValue;

                // Set these values to false as not supported/handled by emiso
                payload_json["MemoryLimit"] = "false";
                payload_json["SwapLimit"] = "false";
                payload_json["CpuCfsPeriod"] = "false";
                payload_json["CpuCfsQuota"] = "false";
                payload_json["CPUShares"] = "false";
                payload_json["CPUSet"] = "false";
                payload_json["PidsLimit"] = "false";
                payload_json["OomKillDisable"] = "false";
                payload_json["IPv4Forwarding"] = "false";
                payload_json["BridgeNfIptables"] = "false";
                payload_json["BridgeNfIp6tables"] = "false";
                payload_json["Debug"] = "false";

                payload_json["SystemTime"] = utils.getSystemTime();
                payload_json["LoggingDriver"] = "json-file"; // WARNING - hardcoded values got from Docker answer of JMI PC
                payload_json["CgroupDriver"] = "none";
                payload_json["CgroupVersion"] = 1; // Default docker value
                payload_json["NEventsListener"] = 0;

                payload_json["KernelVersion"] = config.kernel_version;
                payload_json["OperatingSystem"] = config.os;
                payload_json["OSVersion"]    = config.os_version;
                payload_json["OSType"] = config.os_type;
                payload_json["Architecture"] = config.arch;

                payload_json["NCPU"] = 1;   // Get info from Agency ?
                payload_json["MemTotal"] = 536870912; // Get info from Agency ?
                payload_json["IndexServerAddress"] = "https://index.docker.io/v1/";
                payload_json["RegistryConfig"] = Json::objectValue;
                payload_json["GenericResources"] = Json::arrayValue;

                payload_json["HttpProxy"] =  "";    // use  'HTTP_PROXY' env variable
                payload_json["HttpsProxy"] =  "";  // use 'HTTPS_PROXY' env variable
                payload_json["NoProxy"] =  "";   // use 'NO_PROXY' env variable
                payload_json["Name"] =  "agency";   // Host name
                payload_json["Labels"] = Json::arrayValue;
                payload_json["ExperimentalBuild"] = EMISO_WEB_EXPERIMENTAL;
                payload_json["ServerVersion"] = "24.0.5";
                payload_json["ServerVersion"] = Json::objectValue;
                payload_json["DefaultRuntime"] = "";

                payload_json["Swarm"]["NodeID"] = "";
                payload_json["Swarm"]["NodeAddr"] = "";
                payload_json["Swarm"]["LocalNodeState"] = "inactive";
                payload_json["Swarm"]["ControlAvailable"] = "false";
                payload_json["Swarm"]["Error"] = "";
                payload_json["Swarm"]["RemoteManagers"] = Json::objectValue;

                payload_json["LiveRestoreEnabled"] = false;
                payload_json["Isolation"] = "VT";  // TODO - platform specific
                payload_json["SecurityOptions"] = Json::arrayValue;

                Json::StreamWriterBuilder builder;
                payload_str = Json::writeString(builder, payload_json);

                auto response = std::make_shared<httpserver::string_response>(payload_str,
                           httpserver::http::http_utils::http_ok, "application/json");
                return response;
            }
        };

        class GetVersionHandler : public httpserver::http_resource {
        public:

            const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) {

                std::string payload_str = "";
                Json::Value payload_json;
                Utils& utils = Utils::getInstance();
                int response_code = httpserver::http::http_utils::http_ok;

                auto config = utils.getInfo();

                payload_json["Platform"]["name"] = EMISO_PLATFORM_NAME;
                payload_json["Components"][0]["name"]    = EMISO_WEB_COMP_NAME;
                payload_json["Components"][0]["version"] = EMISO_VERSION;
                payload_json["Version"]       = EMISO_VERSION;
                payload_json["ApiVersion"]    = EMISO_WEB_API_VERSION;
                payload_json["MinAPIVersion"] = EMISO_WEB_API_VERSION;

                // Get info directly from the system
                payload_json["Os"]   = config.os_type;
                payload_json["Arch"] = config.arch;
                payload_json["KernelVersion"] = config.kernel_version;

                payload_json["Experimental"] = EMISO_WEB_EXPERIMENTAL;

                // not implemented yet !
                // payload_json["GitCommit"]
                // payload_json["GoVersion"]
                // payload_json["BuildTime"]

                Json::StreamWriterBuilder builder;
                payload_str = Json::writeString(builder, payload_json);

                auto response = std::make_shared<httpserver::string_response>(payload_str,
                                                  response_code, "application/json");
                return response;
            }
        };


        class SytemApi {
        public:
            // Constructor
            SytemApi(httpserver::webserver *server);

            // Destructor
            ~SytemApi();

        private:
            httpserver::webserver *_server;

            // Handler for the different routes
            PingHandler     *_pinghandler;
            SysInfoHandler  *_sysInfoHandler;
            GetVersionHandler *_getVersionHandler;
        };
    }
}

#endif /* EMISO_SYSTEM_H */