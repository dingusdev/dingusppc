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

/** @file Device registry definitions. */

#ifndef DEVICE_REGISTRY_H
#define DEVICE_REGISTRY_H

#include <machines/machineproperties.h>

#include <functional>
#include <memory>
#include <string>
#include <map>
#include <vector>

class HWComponent;

typedef std::function<std::unique_ptr<HWComponent> ()> CreateFunc;

struct DeviceDescription {
    CreateFunc                m_create_func;
    std::vector<std::string>  subdev_list;
    PropMap                   properties;
};

class DeviceRegistry
{
public:
    DeviceRegistry() = delete;

    static bool add(const std::string name, DeviceDescription desc);

    static bool device_registered(const std::string dev_name);

    static std::unique_ptr<HWComponent> create(const std::string& name);

    static DeviceDescription& get_descriptor(const std::string& name);

private:
    static std::map<std::string, DeviceDescription> & get_registry() {
        static std::map<std::string, DeviceDescription> device_registry;
        return device_registry;
    }
};

#define REGISTER_DEVICE(dev_name, dev_desc) \
    static bool dev_name ## _registered = DeviceRegistry::add(#dev_name, (dev_desc))

#endif // DEVICE_REGISTRY_H
