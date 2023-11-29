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
#include <cstdlib>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <soo/uapi/soo.h>

#include "container.hpp"

#define EMISO_IMAGE_PATH     "/mnt/ME/"

namespace emiso {
namespace daemon {

Container::Container() {};

Container::~Container() {};

// Convert a ME state into the Docker Container state
//      valid container states are: running, paused, exited, restarting, dead
std::string Container::meToDockerState(int meState)
{
    switch (meState) {
    case ME_state_booting:
        return "booting";   // WARNING - not a valid Docker state
    case ME_state_preparing:
        return "booting";    // WARNING - not a valid Docker state
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

    fd = open("/dev/soo/core", O_RDWR);
    // assert(fd_core > 0);

    args.buffer = &id_array;
    ioctl(fd, AGENCY_IOCTL_GET_ME_ID_ARRAY, (unsigned long) &args);

    close(fd);

    for (i = 0; i < MAX_ME_DOMAINS; i++) {
        if (id_array[i].state != ME_state_dead) {
            ContainerInfo info;

            // Should it be 'i+2' ??
            info.id    = i; /* Use the slot number as Container ID */
            info.name  = id_array[i].name;
            info.state = this->meToDockerState(id_array[i].state);

            containerList[i] = info;
        }
    }
}


int Container::create(std::string imageName)
{
    char cmd[80];

    std::cout << "[EMISO] Creating container from '" << imageName << "'" << std::endl;

    sprintf(cmd, "/root/injector %s", imageName.c_str());

    std::cout << "[EMISO] injection cmd: " << cmd << std::endl;

    int rc = std::system(cmd);

    return 0;
}


} // daemon
} // emiso
