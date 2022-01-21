/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-21 divingkatae and maximum
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
    this->comp_map.clear();
    this->subdev_map.clear();
    this->aliases.clear();
}

MachineBase::~MachineBase() {
    for (auto it = this->comp_map.begin(); it != this->comp_map.end(); it++) {
        delete it->second;
    }
    this->comp_map.clear();
    this->aliases.clear();
    this->subdev_map.clear();
}

bool MachineBase::add_component(std::string name, HWComponent* dev_obj) {
    if (this->comp_map.count(name)) {
        LOG_F(ERROR, "Component %s already exists!", name.c_str());
        return false;
    }

    this->comp_map[name] = dev_obj;

    return true;
}

bool MachineBase::add_subdevice(std::string name, HWComponent* dev_obj) {
    if (this->subdev_map.count(name)) {
        LOG_F(ERROR, "Subdevice %s already exists!", name.c_str());
        return false;
    }

    this->subdev_map[name] = dev_obj;

    return true;
}

void MachineBase::add_alias(std::string name, std::string alias) {
    this->aliases[alias] = name;
}

HWComponent* MachineBase::get_comp_by_name(std::string name) {
    if (this->aliases.count(name)) {
        name = this->aliases[name];
    }

    if (this->comp_map.count(name)) {
        return this->comp_map[name];
    }

    if (this->subdev_map.count(name)) {
        return this->subdev_map[name];
    } else {
        return NULL;
    }
}

HWComponent* MachineBase::get_comp_by_type(HWCompType type) {
    std::string comp_name;
    bool found = false;

    for (auto it = this->comp_map.begin(); it != this->comp_map.end(); it++) {
        if (it->second->supports_type(type)) {
            comp_name = it->first;
            found     = true;
            break;
        }
    }

    if (found) {
        return this->get_comp_by_name(comp_name);
    }

    for (auto it = this->subdev_map.begin(); it != this->subdev_map.end(); it++) {
        if (it->second->supports_type(type)) {
            comp_name = it->first;
            found     = true;
            break;
        }
    }

    if (found) {
        return this->get_comp_by_name(comp_name);
    } else {
        return NULL;
    }
}

int MachineBase::postinit_devices()
{
    for (auto it = this->comp_map.begin(); it != this->comp_map.end(); it++) {
        if (it->second->device_postinit()) {
            return -1;
        }
    }

    for (auto it = this->subdev_map.begin(); it != this->subdev_map.end(); it++) {
        if (it->second->device_postinit()) {
            return -1;
        }
    }

    return 0;
}
