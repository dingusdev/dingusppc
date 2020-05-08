/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-20 divingkatae and maximum
                      (theweirdo)     spatium

(Contact divingkatae#1017 or powermax#2286 on Discord for more info)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

//The main runfile - main.cpp
//This is where the magic begins

//#define SDL

#include <thirdparty/loguru/loguru.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cinttypes>
#include <stdio.h>
#include "ppcemu.h"
#include "debugger/debugger.h"
#include "machines/machinefactory.h"
#ifdef SDL
#include <thirdparty/SDL2/include/SDL.h>
#endif

using namespace std;

int main(int argc, char **argv)
{

    std::cout << "DingusPPC - Prototype 5bf4 (7/14/2019)       " << endl;
    std::cout << "Written by divingkatae, (c) 2019.            " << endl;
    std::cout << "This is not intended for general use.        " << endl;
    std::cout << "Use at your own discretion.                  " << endl;

    if (argc > 1) {
        string checker = argv[1];
        cout << checker << endl;

        if ((checker == "1") || (checker == "realtime") || \
            (checker == "-realtime") || (checker == "/realtime")) {
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            loguru::g_preamble_date    = false;
            loguru::g_preamble_time    = false;
            loguru::g_preamble_thread  = false;
            loguru::init(argc, argv);
            loguru::add_file("dingusppc.log", loguru::Append, 0);
            //Replace the above line with this for maximum debugging detail:
            //loguru::add_file("dingusppc.log", loguru::Append, loguru::Verbosity_MAX);
        }
        else if ((checker == "debugger") || (checker == "/debugger") ||
            (checker == "-debugger")) {
            loguru::g_stderr_verbosity = 0;
            loguru::g_preamble_date    = false;
            loguru::g_preamble_time    = false;
            loguru::g_preamble_thread  = false;
            loguru::init(argc, argv);
        }

        std::string rom_file = "rom.bin", disk_file = "disk.img";
        int ram_size = 64, machine_gestalt = 510, video_card_vendor = 0x1002, video_card_id = 0x4750;

        std::ifstream config_input("config.txt");
        std::string line;
        std::istringstream sin;

        if (!config_input) {
            LOG_F(ERROR, "Could not open config.txt. Creating a dummy config.txt now.");
            std::ofstream config_output("config.txt");

            config_output.write("rompath=rom.bin\n", 17);
            config_output.write("diskpath=disk.img\n", 19);
            config_output.write("ramsize=64\n", 12);
            config_output.write("machine_gestalt=510\n", 21);
            config_output.write("video_vendor=0x1002\n", 21);
            config_output.write("video_card=0x4750\n", 19);

            config_output.close();
        }
        else {
            while (std::getline(config_input, line)) {
                sin.str(line.substr(line.find("=") + 1));
                if (line.find("rompath") != std::string::npos) {
                    sin >> rom_file;
                }
                else if (line.find("diskpath") != std::string::npos) {
                    sin >> disk_file;
                }
                else if (line.find("ramsize") != std::string::npos) {
                    sin >> ram_size;
                }
                else if (line.find("machine_gestalt") != std::string::npos) {
                    sin >> machine_gestalt;
                }
                else if (line.find("video_vendor") != std::string::npos) {
                    sin >> video_card_vendor;
                }
                else if (line.find("video_card") != std::string::npos) {
                    sin >> video_card_id;
                }
                sin.clear();
            }
        }

        if (create_machine_for_rom(rom_file.c_str())) {
            goto bail;
        }

#ifdef SDL
        if (SDL_Init(SDL_INIT_AUDIO)){
            LOG_F(ERROR, "SDL_Init error: %s", SDL_GetError());
            goto bail;
        }
#endif

        if ((checker == "1") || (checker == "realtime") || \
            (checker == "-realtime") || (checker == "/realtime")) {
            ppc_exec();
        } else if ((checker == "debugger") || (checker == "/debugger") ||
            (checker == "-debugger")) {
            enter_debugger();
        }
    }
    else {
        std::cout << "                                                " << endl;
        std::cout << "Please enter one of the following commands when " << endl;
        std::cout << "booting up DingusPPC...                         " << endl;
        std::cout << "                                                " << endl;
        std::cout << "                                                " << endl;
        std::cout << "realtime - Run the emulator in real-time.       " << endl;
        std::cout << "debugger - Enter the interactive debugger.      " << endl;
    }

bail:
    LOG_F(INFO, "Cleaning up...");

    delete gMachineObj.release();

    return 0;
}
