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

#include <vector>
#include <sstream>

#include "cli.hpp"

#define EMISO_CLI_PROMPT "(emiso)>> "

namespace emiso {

Cli::Cli() {}

Cli::~Cli() {}

void Cli::splitCmd(std::string const &str, const char delim, std::vector<std::string> &out)
{
    // create a stream from the string
    std::stringstream s(str);

    std::string s2;
    while (std::getline (s, s2, delim)) {
        out.push_back(s2); // store the string in s2
    }
}


// Function to handle the "hello" command
void Cli::handleHelloCommand() {
    std::cout << "Hello, World!" << std::endl;
}

// Function to handle the "help" command
void Cli::handleHelpCommand() {
    std::cout << "Available commands:" << std::endl;
    std::cout << "  hello - Print a greeting" << std::endl;
    std::cout << "  help  - Show available commands" << std::endl;
    std::cout << "  exit  - Exit the program" << std::endl;
}


void Cli::handleImagesCommand(std::vector<std::string> &tokens)
{
    std::map<std::string, daemon::ImageInfo> info;

    // TODO - Add checks on 'tokens'
    if (tokens[1] == "info") {
         _image.info(info);

        for (auto it = info.begin(); it != info.end(); ++it) {
            std::string name = it->second.name;
            size_t size = it->second.size;
            std::string id = it->second.id;
            auto date = it->second.date;

            std::cout << name << std::endl;
            std::cout << "    Size: " << size <<  std::endl;
            std::cout << "    ID: " << id << std::endl;
            std::cout << "    date: " << date << std::endl;
        }
    } else if (tokens[1] == "rm") {
        // TODO - Add checks on 'tokens[2]'
         _image.remove(tokens[2]);
    } else {
        std::cout << "[ERROR] image cmd '" << tokens[1] << "' is not supported" << std::endl;
    }
}

void Cli::start()
{
    std::cout << "Starting interactive mode" << std::endl;
    while (true) {
       char* input = readline(EMISO_CLI_PROMPT);
        if (!input) {
            break; // User pressed Ctrl+D (EOF)
        }

        std::string command(input);
        free(input);

        // Add the command to readline's history
        if (!command.empty()) {
            add_history(command.c_str());
        }

        std::vector<std::string> tokens;
        Cli::splitCmd(command, ' ', tokens);


        if (tokens[0] == "hello") {
            Cli::handleHelloCommand();
        } else if (tokens[0] == "help") {
            Cli::handleHelpCommand();
        } else if (tokens[0] == "image") {
            Cli::handleImagesCommand(tokens);
        } else if (tokens[0] == "exit") {
            break; // Exit the program
        } else {
            std::cout << "Unknown command. Type 'help' for a list of available commands." << std::endl;
        }
    }

    std::cout << "Stopping interactive mode" << std::endl;

}

} // emiso