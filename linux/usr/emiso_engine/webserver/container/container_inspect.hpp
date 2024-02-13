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
            std::strftime(createdDate, 32, "%Y-%m-%dT%H:%M:%SZ", ptm);

            // TODO = add start time in container info
            ptm = std::localtime((const long int*)&info.created + 10);
            std::strftime(startedDate, 32, "%Y-%m-%dT%H:%M:%SZ", ptm);

            // ptm = std::localtime((const long int*)&info.created + 500);
            // std::strftime(endDate, 32, "%Y-%M-%dT%H.%M.%S", ptm);

            // == Build the response message ==
            payloadJson["Id"] =  std::to_string(containerId);
            payloadJson["Created"] = createdDate;
            payloadJson["Path"] = "/inject";
            payloadJson["Args"] = Json::arrayValue;
            payloadJson["State"]["Status"] = info.state;
            payloadJson["State"]["Running"] = (info.state == "running") ? true : false;
            payloadJson["State"]["Paused"] = (info.state == "paused") ? true : false;
            payloadJson["State"]["Restarting"] = (info.state == "restarting") ? true : false;
            payloadJson["State"]["OOMKilled"] = false;
            payloadJson["State"]["Dead"] = (info.state == "dead") ? true : false;
            payloadJson["State"]["Pid"] = 0;
            payloadJson["State"]["ExitCode"] = 0;
            payloadJson["State"]["Error"] = "";
            payloadJson["State"]["StartedAt"] = createdDate;  // to update
            payloadJson["State"]["FinishedAt"] = "";
            payloadJson["Image"] = imageInfo.id;
            payloadJson["ResolvConfPath"] = "/var/lib/docker/containers/ed856fa2c435daac513a0eff537018034fd0ce2843d84eae4dbfd22203bd10de/resolv.conf";
            payloadJson["HostnamePath"] = "/var/lib/docker/containers/ed856fa2c435daac513a0eff537018034fd0ce2843d84eae4dbfd22203bd10de/hostname";
            payloadJson["HostsPath"] = "/var/lib/docker/containers/ed856fa2c435daac513a0eff537018034fd0ce2843d84eae4dbfd22203bd10de/hosts";
            payloadJson["LogPath"] = "/var/lib/docker/containers/ed856fa2c435daac513a0eff537018034fd0ce2843d84eae4dbfd22203bd10de/ed856fa2c435daac513a0eff537018034fd0ce2843d84eae4dbfd22203bd10de-json.log";
            payloadJson["Name"] = "/" + info.name;
            payloadJson["RestartCount"] = 0;
            payloadJson["Driver"] = "overlay2";
            payloadJson["Platform"] = "linux";
            payloadJson["MountLabel"] = "";
            payloadJson["ProcessLabel"] = "";
            payloadJson["AppArmorProfile"] = "";
            payloadJson["ExecIDs"] = Json::nullValue;
            payloadJson["HostConfig"]["Binds"] = Json::nullValue;
            payloadJson["HostConfig"]["ContainerIDFile"] = "";
            payloadJson["HostConfig"]["LogConfig"]["Type"] = "json-file";
            payloadJson["HostConfig"]["LogConfig"]["Config"] = Json::objectValue;
            payloadJson["HostConfig"]["LogConfig"]["NetworkMode"] = "default";
            payloadJson["HostConfig"]["LogConfig"]["PortBindings"] = Json::objectValue;
            payloadJson["HostConfig"]["RestartPolicy"]["Name"] = "no";
            payloadJson["HostConfig"]["RestartPolicy"]["MaximumRetryCount"] = 0;
            payloadJson["HostConfig"]["AutoRemove"] = false;
            payloadJson["HostConfig"]["VolumeDriver"] = "";
            payloadJson["HostConfig"]["VolumesFrom"] = Json::nullValue;
            payloadJson["HostConfig"]["ConsoleSize"][0] = 24;
            payloadJson["HostConfig"]["ConsoleSize"][0] = 80;
            payloadJson["HostConfig"]["CapAdd"] = Json::nullValue;
            payloadJson["HostConfig"]["CapDrop"] = Json::nullValue;
            payloadJson["HostConfig"]["CgroupnsMode"] = "host";
            payloadJson["HostConfig"]["Dns"] = Json::arrayValue;
            payloadJson["HostConfig"]["DnsOptions"] = Json::arrayValue;
            payloadJson["HostConfig"]["DnsSearch"] = Json::arrayValue;
            payloadJson["HostConfig"]["ExtraHosts"] = Json::nullValue;
            payloadJson["HostConfig"]["GroupAdd"] = Json::nullValue;
            payloadJson["HostConfig"]["IpcMode"] = "private";
            payloadJson["HostConfig"]["Cgroup"] = "";
            payloadJson["HostConfig"]["Links"] = Json::nullValue;
            payloadJson["HostConfig"]["OomScoreAdj"] = 0;
            payloadJson["HostConfig"]["PidMode"] = "";
            payloadJson["HostConfig"]["Privileged"] = false;
            payloadJson["HostConfig"]["PublishAllPorts"] = false;
            payloadJson["HostConfig"]["ReadonlyRootfs"] = false;
            payloadJson["HostConfig"]["SecurityOpt"] = Json::nullValue;
            payloadJson["HostConfig"]["UTSMode"] = "";
            payloadJson["HostConfig"]["UsernsMode"] = "";
            payloadJson["HostConfig"]["ShmSize"] = 67108864;
            payloadJson["HostConfig"]["Runtime"] = "runc";
            payloadJson["HostConfig"]["Isolation"] = "";
            payloadJson["HostConfig"]["CpuShares"] = 0;
            payloadJson["HostConfig"]["Memory"] = 0;
            payloadJson["HostConfig"]["NanoCpus"] = 0;
            payloadJson["HostConfig"]["CgroupParent"] = "";
            payloadJson["HostConfig"]["BlkioWeight"] = 0;
            payloadJson["HostConfig"]["BlkioWeightDevice"] = Json::arrayValue;
            payloadJson["HostConfig"]["BlkioDeviceReadBps"] = Json::arrayValue;
            payloadJson["HostConfig"]["BlkioDeviceWriteBps"] = Json::arrayValue;
            payloadJson["HostConfig"]["BlkioDeviceReadIOps"] = Json::arrayValue;
            payloadJson["HostConfig"]["BlkioDeviceWriteIOps"] = Json::arrayValue;
            payloadJson["HostConfig"]["CpuPeriod"] = 0;
            payloadJson["HostConfig"]["CpuQuota"] = 0;
            payloadJson["HostConfig"]["CpuRealtimePeriod"] = 0;
            payloadJson["HostConfig"]["CpuRealtimeRuntime"] = 0;
            payloadJson["HostConfig"]["CpusetCpus"] = "";
            payloadJson["HostConfig"]["CpusetMems"] = "";
            payloadJson["HostConfig"]["Devices"] = Json::arrayValue;
            payloadJson["HostConfig"]["DeviceCgroupRules"] = Json::nullValue;
            payloadJson["HostConfig"]["DeviceRequests"] = Json::nullValue;
            payloadJson["HostConfig"]["MemoryReservation"] = 0;
            payloadJson["HostConfig"]["MemorySwap"] = 0;
            payloadJson["HostConfig"]["MemorySwappiness"] = Json::nullValue;
            payloadJson["HostConfig"]["OomKillDisable"] = Json::nullValue;
            payloadJson["HostConfig"]["PidsLimit"] = Json::nullValue;
            payloadJson["HostConfig"]["Ulimits"] = Json::nullValue;
            payloadJson["HostConfig"]["CpuCount"] = 0;
            payloadJson["HostConfig"]["CpuPercent"] = 0;
            payloadJson["HostConfig"]["IOMaximumIOps"] = 0;
            payloadJson["HostConfig"]["IOMaximumBandwidth"] = 0;
            payloadJson["HostConfig"]["MaskedPaths"][0] = "/proc/asound";
            payloadJson["HostConfig"]["MaskedPaths"][1] = "/proc/acpi";
            payloadJson["HostConfig"]["MaskedPaths"][2] = "/proc/kcore";
            payloadJson["HostConfig"]["MaskedPaths"][3] = "/proc/keys";
            payloadJson["HostConfig"]["MaskedPaths"][4] = "/proc/latency_stats";
            payloadJson["HostConfig"]["MaskedPaths"][5] = "/proc/timer_list";
            payloadJson["HostConfig"]["MaskedPaths"][6] = "/proc/timer_stats";
            payloadJson["HostConfig"]["MaskedPaths"][7] = "/proc/sched_debug";
            payloadJson["HostConfig"]["MaskedPaths"][8] = "/proc/scsi";
            payloadJson["HostConfig"]["MaskedPaths"][9] = "/sys/firmware";
            payloadJson["HostConfig"]["ReadonlyPaths"][0] = "/proc/bus";
            payloadJson["HostConfig"]["ReadonlyPaths"][1] = "/proc/fs";
            payloadJson["HostConfig"]["ReadonlyPaths"][2] = "/proc/irq";
            payloadJson["HostConfig"]["ReadonlyPaths"][3] = "/proc/sys";
            payloadJson["HostConfig"]["ReadonlyPaths"][4] = "/proc/sysrq-trigger";
            payloadJson["GraphDriver"]["Data"]["LowerDir"] = "/var/lib/docker/overlay2/5132a5d000dc493f64e1076a4fbefcfd6d04705bb52590b639668d9d82c6652f-init/diff:/var/lib/docker/overlay2/0ffcf17c5b7310dcbced19f13a63e8b7566325eaabf34ded9bc67b7c56057fa2/diff";
            payloadJson["GraphDriver"]["Data"]["MergedDir"] = "/etc/emiso/merged";
            payloadJson["GraphDriver"]["Data"]["UpperDir"] = "/etc/emiso/diff";
            payloadJson["GraphDriver"]["Data"]["WorkDir"] = "/etc/emiso/work";
            payloadJson["GraphDriver"]["Name"] = "overlay2";
            payloadJson["Mounts"] = Json::arrayValue;
            payloadJson["Config"]["Hostname"] = "ed856fa2c435";
            payloadJson["Config"]["Domainname"] = "";
            payloadJson["Config"]["User"] = "";
            payloadJson["Config"]["AttachStdin"] = false;
            payloadJson["Config"]["AttachStdout"] = true;
            payloadJson["Config"]["AttachStderr"] = true;
            payloadJson["Config"]["Tty"] = false;
            payloadJson["Config"]["OpenStdin"] = false;
            payloadJson["Config"]["StdinOnce"] = false;
            payloadJson["Config"]["Env"][0] = "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
            payloadJson["Config"]["Cmd"][0] = "";
            payloadJson["Config"]["Image"] = info.image;
            payloadJson["Config"]["Volumes"] = Json::nullValue;
            payloadJson["Config"]["WorkingDir"] = "";
            payloadJson["Config"]["Entrypoint"] = Json::nullValue;
            payloadJson["Config"]["OnBuild"] = Json::nullValue;
            payloadJson["Config"]["Labels"] = Json::objectValue;
            payloadJson["NetworkSettings"]["Bridge"] = "";
            payloadJson["NetworkSettings"]["SandboxID"] = "a7c9c2fe37e94ade66a9969d7070fe5d3e894753bb20fe14953692074af9ef00";
            payloadJson["NetworkSettings"]["HairpinMode"] = false;
            payloadJson["NetworkSettings"]["LinkLocalIPv6Address"] = "";
            payloadJson["NetworkSettings"]["LinkLocalIPv6PrefixLen"] = 0;
            payloadJson["NetworkSettings"]["Ports"] = Json::objectValue;
            payloadJson["NetworkSettings"]["SandboxKey"] = "/var/run/docker/netns/a7c9c2fe37e9";
            payloadJson["NetworkSettings"]["SecondaryIPAddresses"] = Json::nullValue;
            payloadJson["NetworkSettings"]["SecondaryIPv6Addresses"] = Json::nullValue;
            payloadJson["NetworkSettings"]["EndpointID"] = "";
            payloadJson["NetworkSettings"]["Gateway"] = "";
            payloadJson["NetworkSettings"]["GlobalIPv6Address"] = "";
            payloadJson["NetworkSettings"]["GlobalIPv6PrefixLen"] = 0;
            payloadJson["NetworkSettings"]["IPAddress"] = "";
            payloadJson["NetworkSettings"]["IPPrefixLen"] = 0;
            payloadJson["NetworkSettings"]["IPv6Gateway"] = "";
            payloadJson["NetworkSettings"]["MacAddress"] = "";
            payloadJson["NetworkSettings"] ["Networks"]["bridge"]["IPAMConfig"] = Json::nullValue;
            payloadJson["NetworkSettings"] ["Networks"]["bridge"]["Links"] = Json::nullValue;
            payloadJson["NetworkSettings"] ["Networks"]["bridge"]["Aliases"] = Json::nullValue;
            payloadJson["NetworkSettings"] ["Networks"]["bridge"]["NetworkID"] = "1cacb396f2c32029fe35d73d8d061575bc69baadc15c1cc8fa596aa2ac3c1eaa";
            payloadJson["NetworkSettings"] ["Networks"]["bridge"]["EndpointID"] = "";
            payloadJson["NetworkSettings"] ["Networks"]["bridge"]["Gateway"] = "";
            payloadJson["NetworkSettings"] ["Networks"]["bridge"]["IPAddress"] = "";
            payloadJson["NetworkSettings"] ["Networks"]["bridge"]["IPPrefixLen"] = 0;
            payloadJson["NetworkSettings"] ["Networks"]["bridge"]["IPv6Gateway"] = "";
            payloadJson["NetworkSettings"] ["Networks"]["bridge"]["GlobalIPv6Address"] = "";
            payloadJson["NetworkSettings"] ["Networks"]["bridge"]["GlobalIPv6PrefixLen"] = 0;
            payloadJson["NetworkSettings"] ["Networks"]["bridge"]["MacAddress"] = "";
            payloadJson["NetworkSettings"] ["Networks"]["bridge"]["DriverOpts"] = Json::nullValue;

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