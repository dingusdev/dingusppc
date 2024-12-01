/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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

#include "machineproperties.h"
#include <loguru.hpp>

#include <algorithm>
#include <cinttypes>
#include <string>
#include <sstream>

void StrProperty::set_string(std::string str)
{
    if (!this->check_val(str)) {
        LOG_F(ERROR, "Invalid property value '%s'!", str.c_str());
        LOG_F(ERROR, "Valid values: %s!",
            this->get_valid_values_as_str().c_str());
    } else {
        this->val = str;
    }
}

std::string StrProperty::get_valid_values_as_str()
{
    std::stringstream ss;

    switch (this->check_type) {
    case CHECK_TYPE_LIST: {
        bool first = true;
        for (auto it = begin(this->vec); it != end(this->vec); ++it) {
            if (!first)
                ss << ", ";
            ss << "'" << *it << "'";
            first = false;
        }
    }
    return ss.str();
    default:
        return std::string("Any");
    }
}

bool StrProperty::check_val(std::string str)
{
    switch (this->check_type) {
    case CHECK_TYPE_LIST:
        if (find(this->vec.begin(), this->vec.end(), str) != this->vec.end())
            return true;
        else
            return false;
    default:
        return true;
    }
}

uint32_t IntProperty::get_int()
{
    try {
        uint32_t result = (uint32_t)strtoul(this->get_string().c_str(), 0, 0);

        /* perform value check */
        if (!this->check_val(result)) {
            LOG_F(ERROR, "Invalid property value %d!", result);
            LOG_F(ERROR, "Valid values: %s!",
                this->get_valid_values_as_str().c_str());
            this->set_string(std::to_string(this->int_val));
        } else {
            this->int_val = result;
        }
    } catch (std::string bad_string) {
        LOG_F(ERROR, "Could not convert string %s to an integer!",
            bad_string.c_str());
    }
    return this->int_val;
}

std::string IntProperty::get_valid_values_as_str()
{
    std::stringstream ss;

    switch (this->check_type) {
    case CHECK_TYPE_RANGE:
        ss << "[" << this->min << "..." << this->max << "]";
        return ss.str();
    case CHECK_TYPE_LIST: {
        bool first = true;
        for (auto it = begin(this->vec); it != end(this->vec); ++it) {
            if (!first)
                ss << ", ";
            ss << *it;
            first = false;
        }
        return ss.str();
    }
    default:
        return std::string("Any");
    }
}

bool IntProperty::check_val(uint32_t val)
{
    switch (this->check_type) {
    case CHECK_TYPE_RANGE:
        if (val < this->min || val > this->max)
            return false;
        else
            return true;
    case CHECK_TYPE_LIST:
        if (find(this->vec.begin(), this->vec.end(), val) != this->vec.end())
            return true;
        else
            return false;
    default:
        return true;
    }
}

void BinProperty::set_string(std::string val)
{
    if ((val.compare("ON") == 0) || (val.compare("on") == 0) || (val.compare("1") == 0) || (val.compare("yes") == 0)) {
        this->bin_val = 1;
        this->val = val;
    } else if ((val.compare("OFF") == 0) || (val.compare("off") == 0) || (val.compare("0") == 0) || (val.compare("no") == 0)) {
        this->bin_val = 0;
        this->val = val;
    } else {
        LOG_F(ERROR, "Invalid BinProperty value %s!", val.c_str());
    }
}

void parse_device_path(std::string dev_path, std::string& bus_id, uint32_t& dev_num) {
    bus_id  = "";
    dev_num = -1;

    size_t delimiter_pos = dev_path.find(":");
    if (delimiter_pos == std::string::npos)
        ABORT_F("Invalid device path %s", dev_path.c_str());

    bus_id = dev_path.substr(0, delimiter_pos);

    try {
        dev_num = (uint32_t)strtoul(dev_path.substr(delimiter_pos+1).c_str(), 0, 0);
    } catch (std::string bad_string) {
        ABORT_F("Invalid device number %s in device path %s", bad_string.c_str(),
            dev_path.c_str());
    }
}
