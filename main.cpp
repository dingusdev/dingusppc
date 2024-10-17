/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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

#include <core/hostevents.h>
#include <core/timermanager.h>
#include <cpu/ppc/ppcemu.h>
#include <debugger/debugger.h>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <utils/profiler.h>
#include <main.h>

#include <cinttypes>
#include <csignal>
#include <cstring>
#include <iostream>
#include <CLI11.hpp>
#include <loguru.hpp>

using namespace std;

static void sigint_handler(int signum) {
    power_on = false;
    power_off_reason = po_signal_interrupt;
}

static void sigabrt_handler(int signum) {
    LOG_F(INFO, "Shutting down...");

    delete gMachineObj.release();
    cleanup();
}

static string appDescription = string(
    "\nDingusPPC - Alpha 1.01 (10/31/2024)          "
    "\nWritten by divingkatae, maximumspatium,      "
    "\njoevt, mihaip, kkaisershot, et. al.          "
    "\n(c) 2018-2024 The DingusPPC Dev Team.        "
    "\nThis is a build intended for testing.        "
    "\nUse at your own discretion.                  "
    "\n"
);

void run_machine(std::string machine_str, std::string bootrom_path, uint32_t execution_mode, uint32_t profiling_interval_ms);

int main(int argc, char** argv) {

    uint32_t execution_mode = interpreter;

    CLI::App app(appDescription);
    app.allow_windows_style_options(); /* we want Windows-style options */
    app.allow_extras();

    bool   realtime_enabled = false;
    bool   debugger_enabled = false;
    string bootrom_path("bootrom.bin");

    app.add_flag("-r,--realtime", realtime_enabled,
        "Run the emulator in real-time");
    app.add_flag("-d,--debugger", debugger_enabled,
        "Enter the built-in debugger");
    app.add_option("-b,--bootrom", bootrom_path, "Specifies BootROM path")
        ->check(CLI::ExistingFile);

    bool              log_to_stderr = false;
    loguru::Verbosity log_verbosity = loguru::Verbosity_INFO;
    bool              log_no_uptime = false;
    app.add_flag("--log-to-stderr", log_to_stderr,
        "Send internal logging to stderr (instead of dingusppc.log)");
    app.add_flag("--log-verbosity", log_verbosity,
        "Adjust logging verbosity (default is 0 a.k.a. INFO)")
        ->check(CLI::Number);
    app.add_flag("--log-no-uptime", log_no_uptime,
        "Disable the uptime preamble of logged messages");

    uint32_t profiling_interval_ms = 0;
#ifdef CPU_PROFILING
    app.add_option("--profiling-interval-ms", profiling_interval_ms,
        "Specifies periodic interval (in ms) at which to output CPU profiling information");
#endif

    string       machine_str;
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
            MachineFactory::list_machines();
        } else if (sub_arg == "properties") {
            MachineFactory::list_properties();
        } else {
            cout << "Unknown list subcommand " << sub_arg << endl;
        }
        return 0;
    }

    if (debugger_enabled) {
        if (realtime_enabled)
            cout << "Both realtime and debugger enabled! Using debugger" << endl;
        execution_mode = debugger;
    }

    /* initialize logging */
    loguru::g_preamble_date    = false;
    loguru::g_preamble_time    = false;
    loguru::g_preamble_thread  = false;
    loguru::g_preamble_uptime  = !log_no_uptime;

    if (execution_mode == interpreter && !log_to_stderr) {
        loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
        loguru::init(argc, argv);
        loguru::add_file("dingusppc.log", loguru::Append, log_verbosity);
    } else {
        loguru::g_stderr_verbosity = log_verbosity;
        loguru::init(argc, argv);
    }

    if (*machine_opt) {
        LOG_F(INFO, "Machine option was passed in: %s", machine_str.c_str());
    } else {
        machine_str = MachineFactory::machine_name_from_rom(bootrom_path);
        if (machine_str.empty()) {
            LOG_F(ERROR, "Could not autodetect machine");
            return 1;
        }
        else {
            LOG_F(INFO, "Machine was autodetected as: %s", machine_str.c_str());
        }
    }

    /* handle overriding of machine settings from command line */
    map<string, string> settings;
    if (MachineFactory::get_machine_settings(machine_str, settings) < 0) {
        return 1;
    }

    CLI::App sa;
    sa.allow_extras();

    for (auto& s : settings) {
        sa.add_option("--" + s.first, s.second);
    }
    sa.parse(app.remaining_for_passthrough()); /* TODO: handle exceptions! */

    MachineFactory::set_machine_settings(settings);

    cout << "BootROM path: " << bootrom_path << endl;
    cout << "Execution mode: " << execution_mode << endl;

    if (!init()) {
        LOG_F(ERROR, "Cannot initialize");
        return 1;
    }

    // initialize global profiler object
    gProfilerObj.reset(new Profiler());

    // graceful handling of fatal errors
    loguru::set_fatal_handler([](const loguru::Message& message) {
        // Make sure the reason for the failure is visible (it may have been
        // sent to the logfile only).
        cerr << message.preamble << message.indentation << message.prefix << message.message << endl;
        power_off_reason = po_enter_debugger;
        enter_debugger();

        // Ensure that NVRAM and other state is persisted before we terminate.
        delete gMachineObj.release();
    });

    // redirect SIGINT to our own handler
    signal(SIGINT, sigint_handler);

    // redirect SIGABRT to our own handler
    signal(SIGABRT, sigabrt_handler);

    while (true) {
        run_machine(machine_str, bootrom_path, execution_mode, profiling_interval_ms);
        if (power_off_reason == po_restarting) {
            LOG_F(INFO, "Restarting...");
            power_on = true;
            continue;
        }
        if (power_off_reason == po_shutting_down) {
            if (execution_mode != debugger) {
                LOG_F(INFO, "Shutdown.");
                break;
            }
            LOG_F(INFO, "Shutdown...");
            power_on = true;
            continue;
        }
        break;
    }

    cleanup();

    return 0;
}

void run_machine(std::string machine_str, std::string bootrom_path, uint32_t execution_mode, uint32_t profiling_interval_ms) {
    if (MachineFactory::create_machine_for_id(machine_str, bootrom_path) < 0) {
        return;
    }

    // set up system wide event polling using
    // default Macintosh polling rate of 11 ms
    uint32_t event_timer = TimerManager::get_instance()->add_cyclic_timer(MSECS_TO_NSECS(11), [] {
        EventManager::get_instance()->poll_events();
    });

#ifdef CPU_PROFILING
    uint32_t profiling_timer;
    if (profiling_interval_ms > 0) {
        profiling_timer = TimerManager::get_instance()->add_cyclic_timer(MSECS_TO_NSECS(profiling_interval_ms), [] {
            gProfilerObj->print_profile("PPC_CPU");
        });
    }
#endif

    switch (execution_mode) {
    case interpreter:
        power_off_reason = po_starting_up;
        enter_debugger();
        break;
    case threaded_int:
        power_off_reason = po_starting_up;
        enter_debugger();
        break;
    case debugger:
        power_off_reason = po_enter_debugger;
        enter_debugger();
        break;
    default:
        LOG_F(ERROR, "Invalid EXECUTION MODE");
        return;
    }

    LOG_F(INFO, "Cleaning up...");
    TimerManager::get_instance()->cancel_timer(event_timer);
#ifdef CPU_PROFILING
    if (profiling_interval_ms > 0) {
        TimerManager::get_instance()->cancel_timer(profiling_timer);
    }
#endif
    EventManager::get_instance()->disconnect_handlers();
    delete gMachineObj.release();
}
