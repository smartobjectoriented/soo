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

#ifndef EMISO_DAEMON_IMAGE_H
#define EMISO_DAEMON_IMAGE_H

#include <iostream>
#include <map>
#include <filesystem>
#include <string>

namespace emiso {
namespace daemon {

    struct ImageInfo {
        std::string id;
        std::string name;
        size_t size;
        uint64_t date;
    };

    class Image {

    public:
        Image();
        ~Image();

        void info(std::map<std::string, ImageInfo> &imagesList);

        // Remove image 'name' from the disk
        void remove(std::string name);
    private:
        // The ID corresponds to the MD5 checksum
        std::string calculateId(const std::string& filePath);

        // Get the date at which the image was created (Unix timestamp)
        int createdTime(const std::string& filePath);

    };
}
}

#endif // EMISO_DAEMON_IMAGE_H
