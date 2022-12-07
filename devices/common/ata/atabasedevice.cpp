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

/** @file Heathrow hard drive controller */

#include <devices/common/ata/atabasedevice.h>
#include <devices/common/ata/atadefs.h>
#include <devices/deviceregistry.h>
#include <loguru.hpp>

#include <cinttypes>

#define sector_size 512

using namespace std;

AtaBaseDevice::AtaBaseDevice(const std::string name)
{
    this->set_name(name);
    supports_types(HWCompType::IDE_DEV);

    device_reset();
}

void AtaBaseDevice::device_reset()
{
    this->r_error = 1; // Device 0 passed, Device 1 passed or not present

    this->r_sect_count = 1;
    this->r_sect_num   = 1;

    // set ATA protocol signature
    this->r_cylinder_lo = 0;
    this->r_cylinder_hi = 0;
    this->r_status = IDE_Status::DRDY | IDE_Status::DSC;
}

uint16_t AtaBaseDevice::read(const uint8_t reg_addr) {
    switch (reg_addr) {
    case IDE_Reg::IDE_DATA:
        LOG_F(WARNING, "Retrieving data from %s", this->name.c_str());
        return 0xFFFFU;
    case IDE_Reg::ERROR:
        return this->r_error;
    case IDE_Reg::SEC_COUNT:
        return this->r_sect_count;
    case IDE_Reg::SEC_NUM:
        return this->r_sect_num;
    case IDE_Reg::CYL_LOW:
        return this->r_cylinder_lo;
    case IDE_Reg::CYL_HIGH:
        return this->r_cylinder_hi;
    case IDE_Reg::DEVICE_HEAD:
        return this->r_dev_head;
    case IDE_Reg::STATUS:
        // TODO: clear pending interrupt
        return this->r_status;
    case IDE_Reg::ALT_STATUS:
        return this->r_status;
    default:
        LOG_F(WARNING, "Attempted to read unknown IDE register: %x", reg_addr);
        return 0;
    }
}

void AtaBaseDevice::write(const uint8_t reg_addr, const uint16_t value) {
    switch (reg_addr) {
    case IDE_Reg::IDE_DATA:
        LOG_F(WARNING, "Pushing data to %s", this->name.c_str());
        break;
    case IDE_Reg::FEATURES:
        this->r_features = value;
        break;
    case IDE_Reg::SEC_COUNT:
        this->r_sect_count = value;
        break;
    case IDE_Reg::SEC_NUM:
        this->r_sect_num = value;
        break;
    case IDE_Reg::CYL_LOW:
        this->r_cylinder_lo = value;
        break;
    case IDE_Reg::CYL_HIGH:
        this->r_cylinder_hi = value;
        break;
    case IDE_Reg::DEVICE_HEAD:
        this->r_dev_head = value;
        break;
    case IDE_Reg::COMMAND:
        this->r_command = value;
        if (is_selected()) {
            perform_command();
        }
        break;
    case IDE_Reg::DEV_CTRL:
        this->r_dev_ctrl = value;
        break;
    default:
        LOG_F(WARNING, "Attempted to write unknown IDE register: %x", reg_addr);
    }
}
