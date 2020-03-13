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

#include <thirdparty/loguru.hpp>
#include <iostream>
#include <cstring>
#include <cinttypes>
#include <stdio.h>
#include "ppcemu.h"
#include "debugger/debugger.h"
#include "machines/machinefactory.h"

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

        uint32_t rom_filesize;

        if (create_machine_for_rom("rom.bin")) {
            goto bail;
        }

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
