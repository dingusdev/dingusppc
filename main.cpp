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
#include "execution/interpreter_loop.h"
#include "machines/machinefactory.h"
#include "machines/machineproperties.h"
#include "ppcemu.h"
#include <cinttypes>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <thirdparty/CLI11/CLI11.hpp>
#include <thirdparty/SDL2/include/SDL.h>
#include <thirdparty/loguru/loguru.hpp>

using namespace std;

static string appDescription = string(
    "\nDingusPPC - Prototype 5bf5 (8/23/2020)       "
    "\nWritten by divingkatae and maximumspatium    "
    "\n(c) 2018-2020 The DingusPPC Dev Team.        "
    "\nThis is not intended for general use.        "
    "\nUse at your own discretion.                  "
    "\n"
);

int main(int argc, char** argv) {
    /*
    Execution Type:
    0 = Realtime (Interpreter)
    1 = Realtime (Debugger)
    2 = Recompiler (to-do)

    The rest will be decided later
    */

    uint32_t execution_mode = 0;

    CLI::App app(appDescription);
    app.allow_windows_style_options(); /* we want Windows-style options */
    app.allow_extras();

    bool   realtime_enabled, debugger_enabled = false;
    string machine_str;
    string bootrom_path("bootrom.bin");

    app.add_flag("-r,--realtime", realtime_enabled,
        "Run the emulator in real-time");

    app.add_flag("-d,--debugger", debugger_enabled,
        "Enter the built-in debugger");

    app.add_option("-b,--bootrom", bootrom_path, "Specifies BootROM path")
        ->check(CLI::ExistingFile);

    CLI::Option* machine_opt = app.add_option("-m,--machine",
        machine_str, "Specify machine ID");

    auto list_cmd = app.add_subcommand("list",
        "Display available machine configurations and exit");

    string sub_arg;

    list_cmd->add_option("machines", sub_arg, "List supported machines");
    list_cmd->add_option("properties", sub_arg, "List available properties");

    CLI11_PARSE(app, argc, argv);

    if (*list_cmd) {
        if (sub_arg == "machines") {
            list_machines();
        } else if (sub_arg == "properties") {
            list_properties();
        } else {
            cout << "Unknown list subcommand " << sub_arg << endl;
        }
        return 0;
    }

    if (debugger_enabled) {
        if (realtime_enabled)
            cout << "Both realtime and debugger enabled! Using debugger" << endl;
        execution_mode = 1;
    }

    /* initialize logging */
    loguru::g_preamble_date    = false;
    loguru::g_preamble_time    = false;
    loguru::g_preamble_thread  = false;

    if (!execution_mode) {
        loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
        loguru::init(argc, argv);
        loguru::add_file("dingusppc.log", loguru::Append, 0);
    } else {
        loguru::g_stderr_verbosity = 0;
        loguru::init(argc, argv);
    }

    if (*machine_opt) {
        LOG_F(INFO, "Machine option was passed in: %s", machine_str.c_str());
    } else {
        machine_str = machine_name_from_rom(bootrom_path);
        if (machine_str.empty()) {
            LOG_F(ERROR, "Could not autodetect machine");
            return 0;
        }
        else {
            LOG_F(INFO, "Machine was autodetected as: %s", machine_str.c_str());
        }
    }

    /* handle overriding of machine settings from command line */
    map<string, string> settings;
    if (get_machine_settings(machine_str, settings) < 0) {
        return 0;
    }

    CLI::App sa;
    sa.allow_extras();

    for (auto& s : settings) {
        sa.add_option("--" + s.first, s.second);
    }
    sa.parse(app.remaining_for_passthrough()); /* TODO: handle exceptions! */

    set_machine_settings(settings);

    cout << "BootROM path: " << bootrom_path << endl;
    cout << "Execution mode: " << execution_mode << endl;

    if (create_machine_for_id(machine_str, bootrom_path) < 0) {
        goto bail;
    }

#ifdef SDL
        if (SDL_Init(SDL_INIT_AUDIO)){
            LOG_F(ERROR, "SDL_Init error: %s", SDL_GetError());
            return 0;
        }
#endif

    switch (execution_mode) {
        case 0:
            interpreter_main_loop();
            break;
        case 1:
            enter_debugger();
            break;
        default:
            LOG_F(ERROR, "Invalid EXECUTION MODE");
            return 0;
    }

bail:
    LOG_F(INFO, "Cleaning up...");

    delete gMachineObj.release();

    return 0;
}
