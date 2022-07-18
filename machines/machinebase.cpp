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

#include <devices/common/hwcomponent.h>
#include <loguru.hpp>
#include <machines/machinebase.h>

#include <map>
#include <memory>
#include <string>

std::unique_ptr<MachineBase> gMachineObj = 0;

MachineBase::MachineBase(std::string name) {
    this->name = name;

    /* initialize internal maps */
    this->device_map.clear();
}

MachineBase::~MachineBase() {
    this->device_map.clear();
}

void MachineBase::add_device(std::string name, std::unique_ptr<HWComponent> dev_obj) {
    if (this->device_map.count(name)) {
        LOG_F(ERROR, "Device %s already exists!", name.c_str());
        return;
    }

    this->device_map[name] = std::move(dev_obj);
}

HWComponent* MachineBase::get_comp_by_name(std::string name) {
    if (this->device_map.count(name)) {
        return this->device_map[name].get();
    } else {
        LOG_F(WARNING, "Component name %s not found!", name.c_str());
        return nullptr;
    }
}

HWComponent* MachineBase::get_comp_by_type(HWCompType type) {
    std::string comp_name;
    bool found = false;

    for (auto it = this->device_map.begin(); it != this->device_map.end(); it++) {
        if (it->second->supports_type(type)) {
            comp_name = it->first;
            found     = true;
            break;
        }
    }

    if (found) {
        return this->get_comp_by_name(comp_name);
    } else {
        LOG_F(WARNING, "No component of type %lu was found!", type);
        return nullptr;
    }
}

int MachineBase::postinit_devices()
{
    for (auto it = this->device_map.begin(); it != this->device_map.end(); it++) {
        if (it->second->device_postinit()) {
            return -1;
        }
    }

    return 0;
}
