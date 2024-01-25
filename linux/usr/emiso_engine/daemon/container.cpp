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
#include <fstream>
#include <cstdlib>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <soo/uapi/soo.h>

#include "container.hpp"

#define EMISO_IMAGE_PATH     "/mnt/ME/"
#define SOO_CORE_DRV_PATH    ("/dev/soo/core")

namespace emiso {

std::map<int, ContainerId> Container::_containersId;

Container::Container() {};

Container::~Container() {};

// Convert a ME state into the Docker Container state
//      valid container states are: running, paused, exited, restarting, dead
std::string Container::meToDockerState(int meState)
{
    switch (meState) {
    case ME_state_booting:
        return "created";   // WARNING - not a valid Docker state
    case ME_state_preparing:
        return "created";    // WARNING - not a valid Docker state
    case ME_state_living:
        return "running";
    case ME_state_suspended:
        return "paused";
    case ME_state_migrating:
        return "error ";
    case ME_state_dormant:
        return "paused";
    case ME_state_killed:
        return "dead";
    case ME_state_terminated:
        return "exited";
    case ME_state_dead:
        return "dead";
    }

    return "(n/a)";
}

void Container::info(std::map<int, ContainerInfo> &containerList)
{
    int i, fd;
    ME_id_t id_array[MAX_ME_DOMAINS];
    agency_ioctl_args_t args;
    int ME_size;
    unsigned char *ME_buffer;

    fd = open(SOO_CORE_DRV_PATH, O_RDWR);
    // assert(fd_core > 0);

    args.buffer = &id_array;
    ioctl(fd, AGENCY_IOCTL_GET_ME_ID_ARRAY, (unsigned long) &args);

    close(fd);

    for (i = 0; i < MAX_ME_DOMAINS; i++) {
        if (id_array[i].state != ME_state_dead) {
            ContainerInfo info;
            int slotID = i + 2;

            info.id    = slotID;
            info.name  = _containersId[slotID].name;
            info.state = this->meToDockerState(id_array[i].state);

            containerList[i] = info;
        }
    }
}

int Container::create(std::string imageName, std::string containerName, int slotID)
{
    int fd;
    int ret;
    struct agency_ioctl_args args;
    struct stat filestat;
    char *containerBuf;
    int nread;
    std::streampos containerSize;

    std::cout << "[EMISO] Creating container from '" << imageName << "'" << std::endl;

    std::ifstream image(imageName.c_str(), std::ios::in | std::ios::binary | std::ios::ate) ;

    containerSize = image.tellg();
    containerBuf = new char [containerSize];
    image.seekg (0, std::ios::beg);
    image.read (containerBuf, containerSize);
    image.close();

    args.buffer = containerBuf;
    args.value = containerSize;

    fd = open(SOO_CORE_DRV_PATH, O_RDWR);

    if (slotID != -1) {
        args.slotID = slotID;
        ret = ioctl(fd, AGENCY_IOCTL_INJECT_ME_WITH_SLOTID, &args);

    } else {
        ret = ioctl(fd, AGENCY_IOCTL_INJECT_ME, &args);
    }

    if (ret < 0) {
        printf("Failed to inject ME (%d)\n", ret);
    }

    ContainerId id;
    id.name  = containerName;
    id.image = imageName;

    _containersId[args.slotID] = id;

    delete[] containerBuf;

    std::cout << "slotID: " << args.slotID << std::endl
              << "id.name: " << id.name << std::endl
              << "id.image: " << id.image << std::endl;

    return args.slotID;
}

int Container::start(unsigned contenerId)
{
    int fd;
    int ret;
    struct agency_ioctl_args args;

    args.slotID = contenerId;

     fd = open(SOO_CORE_DRV_PATH, O_RDWR);
     ret = ioctl(fd, AGENCY_IOCTL_FINAL_MIGRATION, &args);

    if (ret < 0) {
        printf("Failed to initialize migration (%d)\n", ret);
    }

    // close(fd);

    return ret;
}

int Container::stop(unsigned contenerId)
{
    int ret;
    int fd;
    struct agency_ioctl_args args;


    // == Force ME termination ==
    args.slotID = contenerId;

     fd = open(SOO_CORE_DRV_PATH, O_RDWR);
     ret = ioctl(fd, AGENCY_IOCTL_FORCE_TERMINATE, &args);

    if (ret < 0) {
        printf("Failed to force termination (%d)\n", ret);
    }

    // close(fd);

    // == inject ME ==
    std::string imageName;
    std::string containerName;

    // 1. Retrieve the image file
    auto it = _containersId.find(contenerId);

    if (it != _containersId.end()) {
        imageName     = it->second.image;
        containerName = it->second.name;

     } else {
        // BUG - the ME has to be in _containersId MAP
     }

     // experiment - let time to free the slot memory !
     sleep(0.5);

    // int slotId = this->create(imageName, containerName, contenerId);
    int slotId = this->create(imageName, containerName);

    return ret;
}


int Container::restart(unsigned contenerId)
{
    std::cout << "[DAEMON] Restart cmd - stop" << std::endl;
    this->stop(contenerId);
    std::cout << "[DAEMON] Restart cmd - start" << std::endl;
    this->start(contenerId);
    std::cout << "[DAEMON] Restart cmd - completed" << std::endl;

    return 0;

}

int Container::pause(unsigned contenerId)
{
    int fd;
    int ret;
    struct agency_ioctl_args args;

    args.slotID = contenerId;

    fd = open(SOO_CORE_DRV_PATH, O_RDWR);

    ret = ioctl(fd, AGENCY_IOCTL_INIT_MIGRATION, &args);

    if (ret < 0) {
        printf("Failed to initialize migration (%d)\n", ret);
    }

    return ret;
}

int Container::unpause(unsigned contenerId)
{
    int fd;
    int ret;
    struct agency_ioctl_args args;

    args.slotID = contenerId;

    fd = open(SOO_CORE_DRV_PATH, O_RDWR);

    ret = ioctl(fd, AGENCY_IOCTL_FINAL_MIGRATION, &args);

    if (ret < 0) {
        printf("Failed to initialize migration (%d)\n", ret);
    }

    close(fd);

    return ret;
}

int Container::remove(unsigned contenerId)
{
    int ret;
    int fd;
    struct agency_ioctl_args args;


    // == Force ME termination ==
    args.slotID = contenerId;

    fd = open(SOO_CORE_DRV_PATH, O_RDWR);
    ret = ioctl(fd, AGENCY_IOCTL_FORCE_TERMINATE, &args);
    if (ret < 0) {
        printf("Failed to force termination (%d)\n", ret);
    }

    return ret;
}

} // emiso
