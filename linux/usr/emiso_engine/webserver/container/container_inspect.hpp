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

#ifndef EMISO_CONTAINER_INSPECT_H
#define EMISO_CONTAINER_INSPECT_H


namespace emiso {
namespace container {

    class InspectHandler : public httpserver::http_resource {
    public:
        InspectHandler(Daemon *daemon) : _daemon(daemon) {};

        const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) {
            std::string payload_str = "";
            Json::Value payloadJson;

            std::cout << "[WEBERVER] '" << req.get_path()  << "' (" << req.get_method() << ") called" << std::endl;

            // == Retrieve container ID from the request path ==
            int containerId = stoi(req.get_arg("id"));

            ContainerInfo info;
            ImageInfo imageInfo;

            _daemon->container.info(containerId, info);
            _daemon->image.info(info.image, imageInfo);

            std::tm * ptm;
            char createdDate[32];
            char startedDate[32];
            // char endDate[32];

            ptm = std::localtime((const long int*)&info.created);
            std::strftime(createdDate, 32, "%Y-%M-%d %H.%M.%SZ", ptm);

            ptm = std::localtime((const long int*)&info.created + 10);
            std::strftime(startedDate, 32, "%Y-%M-%d %H.%M.%SZ", ptm);

            // ptm = std::localtime((const long int*)&info.created + 500);
            // std::strftime(endDate, 32, "%Y-%M-%dT%H.%M.%S", ptm);

            // == Build the response message ==
            payloadJson["Id"] = containerId;
            payloadJson["Created"] =  createdDate; // TODO / update format
            payloadJson["Path"] = "/inject";
            payloadJson["Args"] = Json::arrayValue;

            payloadJson["State"]["Status"]     = info.state;
            payloadJson["State"]["Running"]    = (info.state == "running") ? true : false;
            payloadJson["State"]["Paused"]     = (info.state == "paused") ? true : false;
            payloadJson["State"]["Restarting"] = (info.state == "restarting") ? true : false;
            payloadJson["State"]["OOMKilled"]  =  false;
            payloadJson["State"]["Dead"]       = (info.state == "dead") ? true : false;
            payloadJson["State"]["Pid"]        = 42077; // TODO  / Find a value
            payloadJson["State"]["ExitCode"]   =  0;
            payloadJson["State"]["Error"]      =  "";
            payloadJson["State"]["StartedAt"]  =  startedDate;
            // payloadJson["State"]["FinishedAt"] = endDate;
            payloadJson["State"]["Health"]  =  Json::nullValue;

            payloadJson["Image"]          =  imageInfo.id + ":latest";
            payloadJson["ResolvConfPath"] = "/etc/resolv.conf";
            payloadJson["HostnamePath"]   = "/etc/hostname";
            payloadJson["HostsPath"]      = "/etc/hosts";
            payloadJson["LogPath"] =  "/etc/emiso/emiso.log",

            payloadJson["Name"] = "/" + info.name;
            payloadJson["RestartCount"] = 0;
            payloadJson["Driver"] = "overlay2";

            payloadJson["Platform"] = "emiso";

            payloadJson["MountLabel"] = "";
            payloadJson["ProcessLabel"] = "";
            payloadJson["AppArmorProfile"] = "";
            payloadJson["ExecIDs"] = Json::nullValue;

            payloadJson["Networks"]["bridge"]["NetworkID"]  = "59125e01bc5d820b2cf1a2e19760b11f25678e098bb08df6d3337ef18403ca56";
            payloadJson["Networks"]["bridge"]["EndpointID"] = "";
            payloadJson["Networks"]["bridge"]["Gateway"]    = "";
            payloadJson["Networks"]["bridge"]["IPAddress"]  = "";
            payloadJson["Networks"]["bridge"]["IPPrefixLen"] =  0;
            payloadJson["Networks"]["bridge"]["IPv6Gateway"] = "",
            payloadJson["Networks"]["bridge"]["GlobalIPv6Address"] = "";
            payloadJson["Networks"]["bridge"]["GlobalIPv6PrefixLen"] = 0;
            payloadJson["Networks"]["bridge"]["MacAddress"] = "";

            payloadJson["Config"]["AttachStderr"]  = false;
            payloadJson["Config"]["AttachStdin"] = false;
            payloadJson["Config"]["AttachStdout"] = false;
            payloadJson["Config"]["Cmd"] = "";
            payloadJson["Config"]["Domainname"] = "";
            payloadJson["Config"]["Entrypoint"][0] = "/" + info.name; // Test - don't known if ok
            payloadJson["Config"]["Env"][0] = "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
            payloadJson["Config"]["ExposedPorts"] = Json::nullValue;
            payloadJson["Config"]["Hostname"] = ""; // RFC 1123 hostname - may be initialized at create time ??
            payloadJson["Config"]["Image"] = imageInfo.id + ":latest";
            payloadJson["Config"]["Labels"] = "";
            payloadJson["Config"]["OpenStdin"] = false;
            payloadJson["Config"]["StdinOnce"] = false;
            payloadJson["Config"]["Tty"] = false;
            payloadJson["Config"]["User"] = "";

            payloadJson["GraphDriver"]["Name"] = "overlay2";
            payloadJson["GraphDriver"]["Data"]["MergedDir"] = "/etc/emiso/merged";
            payloadJson["GraphDriver"]["Data"]["UpperDir"] = "/etc/emiso/diff";
            payloadJson["GraphDriver"]["Data"]["WorkDir"] = "/etc/emiso/work";

            payloadJson["HostConfig"]["AutoRemove"] = false;
            payloadJson["HostConfig"]["Binds"] = Json::arrayValue;
            payloadJson["HostConfig"]["BlkioDeviceReadBps"] = Json::arrayValue;
            payloadJson["HostConfig"]["BlkioDeviceReadIOps"] = Json::arrayValue;
            payloadJson["HostConfig"]["BlkioDeviceWriteBps"] = Json::arrayValue;
            payloadJson["HostConfig"]["BlkioDeviceWriteIOps"] = Json::arrayValue;
            payloadJson["HostConfig"]["BlkioWeight"] = 0;
            payloadJson["HostConfig"]["BlkioWeightDevice"] = Json::arrayValue;

            payloadJson["HostConfig"]["CapAdd"] = "";
            payloadJson["HostConfig"]["CapDrop"] = "";
            payloadJson["HostConfig"]["Cgroup"] = "";
            payloadJson["HostConfig"]["CgroupParent"] = "";
            payloadJson["HostConfig"]["CgroupnsMode"] = "private";
            payloadJson["HostConfig"]["ConsoleSize"][0] = 65;
            payloadJson["HostConfig"]["ConsoleSize"][1] = 282;
            payloadJson["HostConfig"]["ContainerIDFile"] = "";
            payloadJson["HostConfig"]["CpuCount"] = 0;
            payloadJson["HostConfig"]["CpuPercent"] = 0;
            payloadJson["HostConfig"]["CpuPeriod"] = 0;
            payloadJson["HostConfig"]["CpuQuota"] = 0;
            payloadJson["HostConfig"]["CpuRealtimePeriod"] = 0;
            payloadJson["HostConfig"]["CpuRealtimeRuntime"] = 0;
            payloadJson["HostConfig"]["CpuShares"] = 0;
            payloadJson["HostConfig"]["CpusetCpus"] = "";
            payloadJson["HostConfig"]["CpusetMems"] = "";
            payloadJson["HostConfig"]["DeviceCgroupRules"] = "";
            payloadJson["HostConfig"]["DeviceRequests"] = "";
            payloadJson["HostConfig"]["Devices"] = Json::arrayValue;
            payloadJson["HostConfig"]["Dns"] = Json::arrayValue;
            payloadJson["HostConfig"]["DnsOptions"] = Json::arrayValue;
            payloadJson["HostConfig"]["DnsSearch"] = Json::arrayValue;
            payloadJson["HostConfig"]["ExtraHosts"] = "";
            payloadJson["HostConfig"]["GroupAdd"] = "";
            payloadJson["HostConfig"]["IOMaximumBandwidth"] = 0;
            payloadJson["HostConfig"]["IOMaximumIOps"] = 0;
            payloadJson["HostConfig"]["IpcMode"] = "private";
            payloadJson["HostConfig"]["Isolation"] = "";
            payloadJson["HostConfig"]["Links"] = "";
            payloadJson["HostConfig"]["LogConfig"]["Type"] = "syslog";
            payloadJson["HostConfig"]["Memory"] = 0;
            payloadJson["HostConfig"]["MemoryReservation"] = 0;
            payloadJson["HostConfig"]["MemorySwap"] = 0;
            payloadJson["HostConfig"]["MemorySwappiness"] = "";
            payloadJson["HostConfig"]["NanoCpus"] = 0;
            payloadJson["HostConfig"]["NetworkMode"] = "default";
            payloadJson["HostConfig"]["OomKillDisable"] = "";
            payloadJson["HostConfig"]["OomScoreAdj"] = 0;
            payloadJson["HostConfig"]["PidMode"] = "";
            payloadJson["HostConfig"]["PidsLimit"] = "";
            payloadJson["HostConfig"]["PortBindings"] = Json::nullValue;
            payloadJson["HostConfig"]["Privileged"] = false;
            payloadJson["HostConfig"]["PublishAllPorts"] = false;

            Json::StreamWriterBuilder builder;
            payload_str = Json::writeString(builder, payloadJson);
            auto response = std::make_shared<httpserver::string_response>(payload_str,
                           httpserver::http::http_utils::http_ok, "application/json");
            return response;
        }

    private:
        // daemon::Container _container;
        Daemon *_daemon;
    };

} // container
} // emiso

#endif /* EMISO_CONTAINER_INSPECT_H */