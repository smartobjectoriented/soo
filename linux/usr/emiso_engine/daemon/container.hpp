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

#ifndef EMISO_DAEMON_CONTAINER_H
#define EMISO_DAEMON_CONTAINER_H

#include <string>
#include <map>

namespace emiso {
namespace daemon {

    struct ContainerInfo {
        int id;
        std::string name;
        std::string state;
    };

    class Container {

    public:
        Container();
        ~Container();

        void info(std::map<int, ContainerInfo> &containerList);
        int create(std::string imageName);

    private:
        std::string meToDockerState(int meState);

    };
}
}

#endif // EMISO_DAEMON_CONTAINER_H
