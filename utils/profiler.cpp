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

#include "profiler.h"
#include <iostream>
#include <vector>

/** global profiler object */
std::unique_ptr<Profiler> gProfilerObj = 0;

Profiler::Profiler()
{
    this->profiles_map.clear();
}

bool Profiler::register_profile(std::string name,
                                std::unique_ptr<BaseProfile> profile_obj)
{
    // bail out if a profile corresponding to 'name' already exists
    if (this->profiles_map.find(name) != this->profiles_map.end()) {
        return false;
    }

    this->profiles_map[name] = std::move(profile_obj);
    return true;
}

void Profiler::print_profile(std::string name)
{
    if (this->profiles_map.find(name) == this->profiles_map.end()) {
        std::cout << "Profile " << name << " not found." << std::endl;
        return;
    }

    std::cout << std::endl;
    std::cout << "Summary of the profile '" << name << "':" << std::endl;
    std::cout << "------------------------------------------" << std::endl;

    auto prof_it = this->profiles_map.find(name);

    std::vector<ProfileVar> vars;

    // ask the corresponding profile class to fill its variables for us
    prof_it->second->populate_variables(vars);

    for (auto& var : vars) {
        switch(var.format) {
        case ProfileVarFmt::DEC:
            std::cout << var.name << " : " << var.value << std::endl;
            break;
        case ProfileVarFmt::HEX:
            std::cout << var.name << " : " << std::hex << var.value << std::endl;
            break;
        default:
            std::cout << "Unknown value in variable " << var.name << std::endl;
        }
    }

    std::cout << std::endl;
}

void Profiler::reset_profile(std::string name)
{
    if (this->profiles_map.find(name) == this->profiles_map.end()) {
        std::cout << "Profile " << name << " not found." << std::endl;
        return;
    }

    this->profiles_map.find(name)->second->reset();
}
