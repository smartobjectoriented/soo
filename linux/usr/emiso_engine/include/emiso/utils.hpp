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

/* Provide basic functions for the EMISO engine */

#ifndef EMISO_UTILS_H
#define EMISO_UTILS_H

#include <iostream>

namespace emiso {

    struct SystemInfo {
        std::string kernel_version;
        std::string os;
        std::string os_version;
        std::string os_type;
        std::string arch;
    };

    class Utils {

    public:
        static Utils& getInstance() {
            // Create the instance if it doesn't exist
            static Utils instance;
            return instance;
        }

        /* Return info in the platform */
        SystemInfo getInfo();

        // Return the current system time, in RFC 3339 format with nano-seconds
        std::string getSystemTime();

        // Return the Agency UUID of the Smart Object
        std::string getAgencyUID();

    private:
        SystemInfo _info;

        Utils();
        ~Utils();

        // Make copy constructor and assignment operator private to prevent copying
        Utils(const Utils&) = delete;
        Utils& operator=(const Utils&) = delete;
    };

} // emiso

#endif // EMISO_UTILS_H




