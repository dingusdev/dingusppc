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

#include <map>
#include <string>
#include <vector>

using namespace std;

struct DeviceDescription;

struct MachineDescription {
    string                  name;
    string                  description;
    vector<string>          devices;
    PropMap                 settings;
    function<int(string&)>  init_func;
};

class MachineFactory
{
public:
    MachineFactory() = delete;

    static bool add(const string& machine_id, MachineDescription desc);

    static string machine_name_from_rom(string& rom_filepath);

    static int create(string& mach_id);
    static int create_machine_for_id(string& id, string& rom_filepath);

    static void get_device_settings(DeviceDescription& dev, map<string, string> &settings);
    static int get_machine_settings(const string& id, map<string, string> &settings);
    static void set_machine_settings(map<string, string> &settings);

    static void list_machines();
    static void list_properties();

private:
    static void create_device(string& dev_name, DeviceDescription& dev);
    static void print_settings(PropMap& p);
    static void list_device_settings(DeviceDescription& dev);
    static int  load_boot_rom(string& rom_filepath);

    static map<string, MachineDescription> & get_registry() {
        static map<string, MachineDescription> machine_registry;
        return machine_registry;
    }
};

#define REGISTER_MACHINE(mach_name, mach_desc) \
    static bool mach_name ## _registered = MachineFactory::add(#mach_name, (mach_desc))

#endif /* MACHINE_FACTORY_H */
