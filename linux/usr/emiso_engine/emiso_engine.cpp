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

// EMISO Engine - Start the engine on port '2375'. It can be started in secure
// mode (HTTPS/TLS) with '-s' option
// The interactive mode ('-i' option) starts a cli interface instead of the web
// server

#include <iostream>
#include <httpserver.hpp>

#include "webserver/webserver.hpp"
#include "cli/cli.hpp"

#define SERVER_PORT    2375

int main(int argc, char **argv)
{
    int c;
    bool secure = false;
    bool interactive = false;

    // TODO - Use C++ parameter handler lib !
    while ((c = getopt(argc, argv, "is")) != EOF) {
        switch (c) {
            case 'i':
                interactive = true;
                break;
            case 's':
                secure = true;
                break;
            default:
                // usage();
                exit(1);
                break;
        }
    }

    if (interactive) {
        emiso::Cli cli;
        cli.start();

    } else {
        // TODO - in interactive mode, also start the webserver !
        emiso::WebServer server(SERVER_PORT, secure);
        server.start(true);
    }

    return 0;
}
