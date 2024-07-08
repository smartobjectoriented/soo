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

#include <iomanip>
#include <chrono>
#include <sstream>
#include <fstream>
#include <sys/utsname.h>

#include <emiso/utils.hpp>

#define EMISO_OS_RELEASE_PATH "/etc/os-release"
#define EMISO_AGENCY_UID_PATH "/sys/soo/agencyUID"

namespace emiso {

Utils::Utils ()
{
    // Retrieve platform info
    struct utsname unameData;
    if (uname(&unameData) != -1) {
        _info.kernel_version = unameData.release;
        _info.os_type = unameData.sysname;
        _info.arch = unameData.machine;
    }

    // Retrieve OS information
    std::ifstream osReleaseFile(EMISO_OS_RELEASE_PATH);
    if (osReleaseFile.is_open()) {
        std::string line;
        while (std::getline(osReleaseFile, line)) {
            if (line.find("PRETTY_NAME=") != std::string::npos) {
                _info.os = line.substr(13);
            }
            if (line.find("VERSION=") != std::string::npos) {
                _info.os_version = line.substr(8);
            }
        }
        osReleaseFile.close();
    } else {
        std::cerr << "Failed to open /etc/os-release" << std::endl;
    }
}

Utils::~Utils() {}

SystemInfo Utils::getInfo()
{
    return _info;
}


std::string Utils::getSystemTime()
{
    // Get the current time with nanoseconds
    auto currentTime = std::chrono::system_clock::now();

    // Convert to RFC 3339 format with nanoseconds
    auto timeT = std::chrono::system_clock::to_time_t(currentTime);
    auto fractionalSeconds = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime.time_since_epoch()).count() % 1000000000;
    struct tm timeinfo;
    gmtime_r(&timeT, &timeinfo);

    // Format the date and time
    char buffer[64]; // Sufficient size for RFC 3339 format
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &timeinfo);

    // Create an output string stream to capture the formatted time
    std::ostringstream formattedTime;
    formattedTime << buffer << "." << std::setw(9) << std::setfill('0') << fractionalSeconds << "Z";

    // Get the formatted time as a string
    return formattedTime.str();
}


std::string Utils::getAgencyUID()
{
    std::ifstream sysfsFile(EMISO_AGENCY_UID_PATH);
    if (!sysfsFile.is_open()) {
        std::cerr << "Failed to open sysfs file: " << EMISO_AGENCY_UID_PATH << std::endl;
        // TODO - handle error
    }

    std::string fileContent;
    std::getline(sysfsFile, fileContent); // Read the content of the file

    sysfsFile.close(); // Close the file

    if (sysfsFile.fail()) {
        std::cerr << "Error occurred while reading the sysfs file." << std::endl;
        // TODO - handle error
    }

    return fileContent;
}


} // emiso
