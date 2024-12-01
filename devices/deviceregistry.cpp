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

/** @file Device registry implementation. */

#include <devices/deviceregistry.h>
#include <devices/common/hwcomponent.h>

#include <string>

bool DeviceRegistry::add(const std::string name, DeviceDescription desc)
{
    if (device_registered(name)) {
        return false;
    }

    get_registry()[name] = desc;
    return true;
}

bool DeviceRegistry::device_registered(const std::string dev_name)
{
    if (get_registry().find(dev_name) != get_registry().end()) {
        return true;
    }

    return false;
}

std::unique_ptr<HWComponent> DeviceRegistry::create(const std::string& name)
{
    auto it = get_registry().find(name);
    if (it != get_registry().end()) {
        return it->second.m_create_func();
    }

    return nullptr;
}

DeviceDescription& DeviceRegistry::get_descriptor(const std::string& name)
{
    auto it = get_registry().find(name);
    if (it != get_registry().end()) {
        return it->second;
    }

    static DeviceDescription empty_descriptor = {};

    return empty_descriptor;
}
