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

// The main runfile - main.cpp
// This is where the magic begins

#include "debugger/debugger.h"
#include "machines/machinefactory.h"
#include "machines/machineproperties.h"
#include "ppcemu.h"
#include <cinttypes>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <thirdparty/SDL2/include/SDL.h>
#include <thirdparty/loguru/loguru.hpp>

using namespace std;

void display_help() {
    std::cout << "                                                   " << endl;
    std::cout << "To interact with DingusPPC, please refer to the    " << endl;
    std::cout << "following command line reference guide:            " << endl;
    std::cout << "___________________________________________________" << endl;
    std::cout << "| COMMAND  | FUNCTION                             |" << endl;
    std::cout << "___________________________________________________" << endl;
    std::cout << " realtime  | Run the emulator in real-time         " << endl;
    std::cout << " debugger  | Enter the interactive debugger        " << endl;
    std::cout << " ram       | Specify the number of RAM banks,      " << endl;
    std::cout << "           | followed by how much each bank holds  " << endl;
    std::cout << " videocard | Specify the video card to emulate     " << endl;
    std::cout << " gfxmem    | Specify the how much memory           " << endl;
    std::cout << "           | to allocate to the emulated video card" << endl;
    std::cout << " romfile   | Insert the ROM file to use            " << endl;
    std::cout << "                                                   " << endl;
}

void display_recognized_machines() {
    std::cout << "                                                   " << endl;
    std::cout << "The following machines are supported by DingusPPC: " << endl;
    std::cout << "___________________________________________________" << endl;
    std::cout << "| COMMAND  | MACHINE RECOGNIZED                   |" << endl;
    std::cout << "___________________________________________________" << endl;
    std::cout << " pmg3      | Power Mac G3 Beige                    " << endl;
    std::cout << " pm6100    | Power Mac 6100                        " << endl;
    std::cout << "                                                   " << endl;
}

int main(int argc, char** argv) {
    /*
    Execution Type:
    0 = Realtime (Interpreter)
    1 = Realtime (Debugger)
    2 = Recompiler (to-do)

    The rest will be decided later
    */

    uint32_t execution_mode    = 0;
    uint32_t sys_ram_size[12]  = {64, 0, 0, 0};
    uint32_t gfx_mem           = 2;
    bool machine_specified     = false;
    string machine_name        = "";

    std::cout << "DingusPPC - Prototype 5bf5 (8/23/2020)       " << endl;
    std::cout << "Written by divingkatae and maximumspatium    " << endl;
    std::cout << "(c) 2018-2020 The DingusPPC Dev Team.        " << endl;
    std::cout << "This is not intended for general use.        " << endl;
    std::cout << "Use at your own discretion.                  " << endl;

    if (argc < 1) {
        display_help();

        return 0;
    }
    else {
        
        std::string rom_file = "rom.bin", disk_file = "disk.img";
        int video_card_vendor = 0x1002, video_card_id = 0x4750;

        for (int arg_loop = 1; arg_loop < argc; arg_loop++) {
            string checker = argv[arg_loop];
            cout << checker << endl;
            
            if ((checker == "realtime") || (checker == "-realtime") || (checker == "/realtime")) {
                loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
                loguru::g_preamble_date    = false;
                loguru::g_preamble_time    = false;
                loguru::g_preamble_thread  = false;
                loguru::init(argc, argv);
                loguru::add_file("dingusppc.log", loguru::Append, 0);
                // Replace the above line with this for maximum debugging detail:
                // loguru::add_file("dingusppc.log", loguru::Append, loguru::Verbosity_MAX);
                execution_mode = 0;
            } else if ((checker == "debugger") || (checker == "/debugger") || (checker == "-debugger")) {
                loguru::g_stderr_verbosity = 0;
                loguru::g_preamble_date    = false;
                loguru::g_preamble_time    = false;
                loguru::g_preamble_thread  = false;
                loguru::init(argc, argv);
                execution_mode = 1;
            } 
            else if ((checker == "help") || (checker == "/help") || (checker == "-help")) {
                display_help();
                return 0;
            }  
            else if ((checker == "pmg3") || (checker == "/pmg3") || (checker == "-pmg3")) {
                machine_name = "PowerMacG3";
                machine_specified = true;
            } 
            else if ((checker == "pm6100") || (checker == "/pm6100") || (checker == "-pm6100")) {
                machine_name      = "PowerMac6100";
                machine_specified = true;
            } 
            else if ((checker == "machinehelp") || (checker == "/machinehelp") || (checker == "-machinehelp")) {
                machine_name      = "MachineHelp";
                machine_specified = true;
            }  
            else if ((checker == "ram") || (checker == "/ram") || (checker == "-ram")) {
                arg_loop++;
                string ram_banks   = argv[arg_loop];
                uint32_t ram_loop  = stoi(ram_banks);
                for (int check_loop = 0; check_loop < ram_loop; check_loop++) {
                    sys_ram_size[check_loop] = stoi(argv[arg_loop]);
                }
            }  
            else if ((checker == "gfxmem") || (checker == "/gfxmem") || (checker == "-gfxmem")) {
                arg_loop++;
                string vram_amount = argv[arg_loop];
                gfx_mem            = stoi(vram_amount);
                LOG_F(INFO, "GFX MEMORY set to: %d", gfx_mem);
            }
            else if ((checker == "romfile") || (checker == "/romfile") || (checker == "-romfile")) {
                arg_loop++;
                rom_file = argv[arg_loop];
                LOG_F(INFO, "ROM FILE will now be: %s", rom_file.c_str());
            } 
            else if ((checker == "diskimg") || (checker == "/diskimg") || (checker == "-diskimg")) {
                arg_loop++;
                rom_file = argv[arg_loop];
                LOG_F(INFO, "Load the DISK IMAGE from: %s", rom_file.c_str());
            } 
            else if ((checker == "videocard") || (checker == "/videocard") || (checker == "-videocard")) {
                arg_loop++;
                string check_card = argv[arg_loop];
                if (checker.compare("RagePro") == 0) {
                    video_card_vendor = 0x1002;
                    video_card_id     = 0x4750;
                } 
                else if (checker.compare("Rage128") == 0) {
                    video_card_vendor = 0x1002;
                    video_card_id     = 0x5046;
                } 
                else if (checker.compare("Radeon7000") == 0) {
                    video_card_vendor = 0x1002;
                    video_card_id     = 0x5159;
                }
            }

        }

        if (machine_specified) {
            if (machine_name.compare("PowerMac6100") == 0) {
                if (establish_machine_settings(machine_name, sys_ram_size)) {
                    if (create_gossamer(sys_ram_size, gfx_mem)) {
                        goto bail;
                    }
                } else {
                    LOG_F(ERROR, "Invalid Settings Specified");
                    return -1;
                }
            } 
            else if (machine_name.compare("PowerMac6100") == 0) {
                LOG_F(ERROR, "Board not yet ready for: %s", machine_name.c_str());
                return -1;
            } 
            else {
                display_recognized_machines();
                return -1;
            }
        } 
        else{
            if (create_machine_for_rom(rom_file.c_str(), sys_ram_size, gfx_mem)) {
                goto bail;
            } else {
                LOG_F(
                    WARNING,
                    "Could not create ROM, because the file %s was not found!", rom_file.c_str());
                display_help();
                return -1;
            }
        }

#ifdef SDL
        if (SDL_Init(SDL_INIT_AUDIO)){
            LOG_F(ERROR, "SDL_Init error: %s", SDL_GetError());
            goto bail;
        }
#endif

        switch (execution_mode) {
        case 0:
            ppc_exec();
            break;
        case 1:
            enter_debugger();
            break;
        default:
            LOG_F(ERROR, "Invalid EXECUTION MODE");
            return 1;
        }
    } 

bail:
    LOG_F(INFO, "Cleaning up...");

    delete gMachineObj.release();

    return 0;
}
