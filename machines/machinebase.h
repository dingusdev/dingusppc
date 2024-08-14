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

/** @file Base class for managing different HW components of a machine.

    Author: Max Poliakovski
 */

#ifndef MACHINE_BASE_H
#define MACHINE_BASE_H

#include <map>
#include <memory>
#include <string>

class HWComponent;
enum HWCompType : uint64_t;

class MachineBase {
public:
    MachineBase(std::string name);
    ~MachineBase();

    void add_device(std::string name, std::unique_ptr<HWComponent> dev_obj);
    void remove_device(HWComponent* dev_obj);
    HWComponent* get_comp_by_name(std::string name);
    HWComponent* get_comp_by_name_optional(std::string name);
    HWComponent* get_comp_by_type(HWCompType type);
    int postinit_devices();

private:
    std::string name;
    std::map<std::string, std::unique_ptr<HWComponent>> device_map;
};

extern std::unique_ptr<MachineBase> gMachineObj;

#endif /* MACHINE_BASE_H */
