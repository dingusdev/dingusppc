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

/** @file Base class for managing different HW components of a machine.

    Author: Max Poliakovski
 */

#ifndef MACHINE_BASE_H
#define MACHINE_BASE_H

#include <memory>
#include <string>
#include <map>
#include "devices/hwcomponent.h"

class MachineBase
{
public:
    MachineBase(std::string name);
    ~MachineBase();

    bool add_component(std::string name, HWComponent *dev_obj);
    bool add_subdevice(std::string name, HWComponent *dev_obj);
    void add_alias(std::string name, std::string alias);
    HWComponent *get_comp_by_name(std::string name);
    HWComponent *get_comp_by_type(HWCompType type);

private:
    std::string name;
    std::map<std::string, HWComponent *>comp_map;
    std::map<std::string, HWComponent *>subdev_map;
    std::map<std::string, std::string> aliases;
};

extern std::unique_ptr<MachineBase> gMachineObj;

#endif /* MACHINE_BASE_H */
