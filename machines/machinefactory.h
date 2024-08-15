/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-22 divingkatae and maximum
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

/** @file Factory for creating different machines.

    Author: Max Poliakovski
 */

#ifndef MACHINE_FACTORY_H
#define MACHINE_FACTORY_H

#include <machines/machineproperties.h>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct DeviceDescription;

struct MachineDescription {
    std::string                       name;
    std::string                       description;
    std::vector<std::string>          devices;
    PropMap                           settings;
    std::function<int(std::string&)>  init_func;
};

typedef std::function<std::optional<std::string>(const std::string&)> GetSettingValueFunc;

class MachineFactory
{
public:
    MachineFactory() = delete;

    static bool add(const std::string& machine_id, MachineDescription desc);

    static size_t read_boot_rom(std::string& rom_filepath, char *rom_data);
    static std::string machine_name_from_rom(char *rom_data, size_t rom_size);

    static int create(std::string& mach_id);
    static int create_machine_for_id(std::string& id, char *rom_data, size_t rom_size);

    static void register_device_settings(const std::string &name);
    static int  register_machine_settings(const std::string& id);

    static void list_machines();
    static void list_properties();

    static GetSettingValueFunc get_setting_value;

private:
    static void create_device(std::string& dev_name, DeviceDescription& dev);
    static void print_settings(const PropMap& p);
    static void list_device_settings(DeviceDescription& dev);
    static int  load_boot_rom(char *rom_data, size_t rom_size);
    static void register_settings(const PropMap& p);

    static std::map<std::string, MachineDescription> & get_registry() {
        static std::map<std::string, MachineDescription> machine_registry;
        return machine_registry;
    }
};

#define REGISTER_MACHINE(mach_name, mach_desc) \
    static bool mach_name ## _registered = MachineFactory::add(#mach_name, (mach_desc))

#endif /* MACHINE_FACTORY_H */
