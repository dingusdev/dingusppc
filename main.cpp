/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-25 divingkatae and maximum
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
#include <cpu/ppc/ppcdisasm.h>
#include <cpu/ppc/ppcemu.h>
#include <cpu/ppc/ppcmmu.h>
#include <debugger/debugger.h>
#include <devices/common/ofnvram.h>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <utils/profiler.h>
#include <main.h>

#include <cinttypes>
#include <csignal>
#include <cstring>
#include <iostream>
#include <optional>
#include <CLI11.hpp>
#include <loguru.hpp>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
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
    "\nDingusPPC - Alpha 1.03 (5/5/2025)            "
    "\nWritten by divingkatae, maximumspatium,      "
    "\njoevt, mihaip, kkaisershot, et. al.          "
    "\n(c) 2018-2025 The DingusPPC Dev Team.        "
    "\nThis is a build intended for testing.        "
    "\nUse at your own discretion.                  "
    "\n"
);

static uint32_t keyboard_id = 0;

/// Check for an existing directory (returns error message if check fails)
class WorkingDirectoryValidator : public CLI::detail::ExistingDirectoryValidator {
public:
    WorkingDirectoryValidator() {
        func_ = [](std::string& filename) {
            std::string result = CLI::ExistingDirectory.operator()(filename);
            if (result.empty()) {
                #ifdef _WIN32
                _chdir(filename.c_str());
                #else
                chdir(filename.c_str());
                #endif
            }
            return result;
        };
    }
};

const WorkingDirectoryValidator WorkingDirectory;

void run_machine(
    std::string machine_str, char *rom_data, size_t rom_size, uint32_t execution_mode
    ,const std::vector<std::string> &env_vars
    ,uint32_t profiling_interval_ms
);

int main(int argc, char** argv) {

    uint32_t execution_mode = interpreter;

    CLI::App app(appDescription);
    app.allow_windows_style_options(); /* we want Windows-style options */
    app.allow_extras();

    bool realtime_enabled = false;
    bool debugger_enabled = false;
    string keyboard_string = "Eng_USA";

    const std::map<std::string, int> kbd_map{
        {"Eng_USA", 0}, {"Eng_GBR", 1}, {"Fra_FRA", 10}, {"Deu_DEU", 20},
        {"Ita_ITA", 30},{"Spa_ESP", 40}, {"Jpn_JPN", 80},
    };

    string bootrom_path("bootrom.bin");
    string working_directory_path(".");

    auto execution_mode_group = app.add_option_group("execution mode")
        ->require_option(-1);
    execution_mode_group->add_flag("-r,--realtime", realtime_enabled,
        "Run the emulator in real-time");
    execution_mode_group->add_flag("-d,--debugger", debugger_enabled,
        "Enter the built-in debugger");
    app.add_option("-k,--keyboard", keyboard_string, "Specify keyboard ID");
    app.add_option("-w,--workingdir", working_directory_path, "Specifies working directory")
        ->check(WorkingDirectory);
    app.add_option("-b,--bootrom", bootrom_path, "Specifies BootROM path")
        ->check(CLI::ExistingFile);
    app.add_flag("--deterministic", is_deterministic,
        "Make execution deterministic");

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

    std::vector<std::string> env_vars;
    app.add_option("--setenv", env_vars, "Set Open Firmware variables at startup")
        ->take_all();

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

    auto rom_data = std::unique_ptr<char[]>(new char[4 * 1024 * 1024]);
    memset(&rom_data[0], 0, static_cast<size_t>(4 * 1024 * 1024));
    size_t rom_size = MachineFactory::read_boot_rom(bootrom_path, &rom_data[0]);
    if (!rom_size) {
        return 1;
    }

    string machine_str_from_rom = MachineFactory::machine_name_from_rom(&rom_data[0], rom_size);
    if (machine_str_from_rom.empty()) {
        LOG_F(ERROR, "Could not autodetect machine from ROM.");
    } else {
        LOG_F(INFO, "Machine detected from ROM as: %s", machine_str_from_rom.c_str());
    }
    if (*machine_opt) {
        LOG_F(INFO, "Machine option was passed in: %s", machine_str.c_str());
    } else {
        machine_str = machine_str_from_rom;
    }
        if (machine_str.empty()) {
        LOG_F(ERROR, "Must specificy a machine or provide a supported ROM.");
            return 1;
    }

    // Hook to allow properties to be read from the command-line, regardless
    // of when they are registered.
    MachineFactory::get_setting_value = [&](const std::string& name) -> std::optional<std::string> {
        CLI::App sa;
        sa.allow_extras();

        std::string value;
        sa.add_option("--" + name, value)->expected(0,1);
        try {
            sa.parse(app.remaining_for_passthrough());
        } catch (const CLI::Error& e) {
            ABORT_F("Cannot parse CLI: %s", e.get_name().c_str());
        }

        if (sa.count("--" + name) > 0) {
            return value;
        } else {
            return std::nullopt;
        }
    };

    if (MachineFactory::register_machine_settings(machine_str) < 0) {
        return 1;
    }

    cout << "BootROM path: " << bootrom_path << endl;
    cout << "Execution mode: " << execution_mode << endl;
    if (is_deterministic) {
        cout << "Using deterministic execution mode, input will be ignored." << endl;
    }

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
        DppcDebugger::get_instance()->enter_debugger();

        // Ensure that NVRAM and other state is persisted before we terminate.
        delete gMachineObj.release();
    });

    // redirect SIGINT to our own handler
    signal(SIGINT, sigint_handler);

    // redirect SIGABRT to our own handler
    signal(SIGABRT, sigabrt_handler);

    keyboard_id = kbd_map.at(keyboard_string);

    while (true) {
        run_machine(
            machine_str,
            &rom_data[0],
            rom_size,
            execution_mode,
            env_vars,
            profiling_interval_ms);
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

    // if we didn't delete this then delete it now
    delete gMachineObj.release();

    cleanup();

    return 0;
}

void run_machine(std::string machine_str, char* rom_data,
    size_t rom_size,
    uint32_t execution_mode,
    const std::vector<std::string> &env_vars,
    uint32_t
#ifdef CPU_PROFILING
     profiling_interval_ms
#endif
) {
    if (MachineFactory::create_machine_for_id(machine_str, rom_data, rom_size) < 0) {
        return;
    }

    if (!env_vars.empty()) {
        OfConfigUtils ofnvram;
        if (!ofnvram.init()) {
            for (const auto &env_var : env_vars) {
                auto pos = env_var.find('=');
                if (pos != std::string::npos) {
                    std::string name = env_var.substr(0, pos);
                    std::string value = env_var.substr(pos + 1);
                    if (ofnvram.setenv(name, value)) {
                        LOG_F(INFO, "Set Open Firmware variable %s to %s", name.c_str(), value.c_str());
                    } else {
                        LOG_F(WARNING, "Cannot set Open Firmware variable %s to %s", name.c_str(), value.c_str());
                    }
                } else {
                    LOG_F(WARNING, "Invalid format for --setenv: %s", env_var.c_str());
                }
            }
        } else {
            LOG_F(WARNING, "Cannot initialize NVRAM wrapper, will not set Open Firmware variables");
        }
    }

    uint32_t deterministic_timer;
    if (is_deterministic) {
        EventManager::get_instance()->disable_input_handlers();
        // Log the PC and instruction every second to make it easier to validate
        // that execution is the same every time.
        deterministic_timer = TimerManager::get_instance()->add_cyclic_timer(MSECS_TO_NSECS(1000), [] {
            PPCDisasmContext ctx;
            ctx.instr_code = ppc_read_instruction(mmu_translate_imem(ppc_state.pc));
            ctx.instr_addr = ppc_state.pc;
            ctx.simplified = false;
            auto op_name = disassemble_single(&ctx);
            LOG_F(INFO, "TS=%016llu PC=0x%08x executing %s", get_virt_time_ns(), ppc_state.pc, op_name.c_str());
        });
    }

    EventManager::get_instance()->set_keyboard_locale(keyboard_id);

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
        DppcDebugger::get_instance()->enter_debugger();
        break;
    case threaded_int:
        power_off_reason = po_starting_up;
        DppcDebugger::get_instance()->enter_debugger();
        break;
    case debugger:
        power_off_reason = po_enter_debugger;
        DppcDebugger::get_instance()->enter_debugger();
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
    if (is_deterministic) {
        TimerManager::get_instance()->cancel_timer(deterministic_timer);
    }
    EventManager::get_instance()->disconnect_handlers();
    delete gMachineObj.release();
}
