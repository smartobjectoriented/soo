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

#include <functional>
#include <fstream>

#include "image.hpp"

#define EMISO_IMAGE_PATH     "/mnt/ME/"
#define EMISO_IMAGE_ID_MODE  "md5"

namespace emiso {

Image::Image() {};

Image::~Image() {};

namespace fs = std::filesystem;

std::string Image::calculateId(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file '" << filePath << "'" << std::endl;
        return "";
    }

    // Create an MD5 hash function
    std::hash<std::string> md5Hash;

    // Read the file content and calculate the MD5 checksum
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string fileContent = buffer.str();
    std::string id = std::to_string(md5Hash(fileContent));

    return id;
}


int Image::createdTime(const std::string& filePath)
{
    try {
        // Check if the file exists and retrieve its status
        if (fs::exists(filePath)) {
            fs::file_time_type fileTime = fs::last_write_time(filePath);

            // Convert file time to Unix timestamp (seconds since epoch)
            auto fileTimestamp = std::chrono::duration_cast<std::chrono::seconds>(fileTime.time_since_epoch()).count();

            return fileTimestamp;
        } else {
            std::cout << "[WARNING] File '" << filePath << "' does not exist." << std::endl;
            return -1;
        }

    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "[ERROR]: " << e.what() << std::endl;
        return -1;
    }
}


void Image::info(std::map<std::string, ImageInfo> &imagesList)
{
    // Check if the folder exists
    if (std::filesystem::exists(EMISO_IMAGE_PATH) && std::filesystem::is_directory(EMISO_IMAGE_PATH)) {
        // Iterate through the files in the folder
        for (const auto& entry : std::filesystem::directory_iterator(EMISO_IMAGE_PATH)) {
            if (std::filesystem::is_regular_file(entry)) {
                ImageInfo info;

                info.name = entry.path().filename();
                info.size = std::filesystem::file_size(entry);
                info.date = this->createdTime(EMISO_IMAGE_PATH + info.name);

                auto id   = this->calculateId(EMISO_IMAGE_PATH + info.name);
                std::string mode = EMISO_IMAGE_ID_MODE;
                info.id   = mode + ":" + id;

                imagesList[info.name] = info;
            }
        }
    } else {
        std::cerr << "Folder '" << EMISO_IMAGE_PATH << "' does not exist or is not a directory." << std::endl;
    }
}

void Image::info(std::string name, ImageInfo &info)
{
    info.name = name;
    info.size = std::filesystem::file_size(name);
    info.date = this->createdTime(name);

    auto id   = this->calculateId(info.name);
    std::string mode = EMISO_IMAGE_ID_MODE;
    info.id   = mode + ":" + id;
}

void Image::remove(std::string name)
{
    std::filesystem::path filePath = EMISO_IMAGE_PATH + name;

    if (std::filesystem::exists(filePath)) {
        try {
            // Remove the file
            std::filesystem::remove(filePath);

            std::cout << "File removed successfully." << std::endl;
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Error removing file: " << e.what() << std::endl;

        }
    } else {
        std::cout << "[WARNING] File '" << filePath << "' does not exists" << std::endl;
    }
}

} // emiso
