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

/** @file Heathrow hard drive controller */

#include <devices/common/ata/atabasedevice.h>
#include <devices/common/ata/atadefs.h>
#include <devices/deviceregistry.h>
#include <loguru.hpp>

#include <cinttypes>

using namespace ata_interface;

AtaBaseDevice::AtaBaseDevice(const std::string name, uint8_t type) {
    this->set_name(name);
    supports_types(HWCompType::IDE_DEV);

    this->device_type = type;

    device_reset(false);
    device_set_signature();
}

void AtaBaseDevice::device_reset(bool is_soft_reset) {
    LOG_F(INFO, "%s: %s-reset triggered", this->name.c_str(),
          is_soft_reset ? "soft" : "hard");

    // Diagnostic code
    this->r_error = 1; // device 0 passed, device 1 passed or not present
}

void AtaBaseDevice::device_set_signature() {
    this->r_sect_count = 1;
    this->r_sect_num   = 1;
    this->r_dev_head   = 0;

    // set protocol signature
    if (this->device_type == DEVICE_TYPE_ATAPI) {
        this->r_cylinder_lo = 0x14;
        this->r_cylinder_hi = 0xEB;
        this->r_status_save = this->r_status; // for restoring on the first command
        this->r_status      = 0;
    } else { // assume ATA by default
        this->r_cylinder_lo = 0;
        this->r_cylinder_hi = 0;
        this->r_status = DRDY | DSC; // DSC=1 is required for ATA devices
    }
}

uint16_t AtaBaseDevice::read(const uint8_t reg_addr) {
    switch (reg_addr) {
    case ATA_Reg::DATA:
        LOG_F(WARNING, "Retrieving data from %s", this->name.c_str());
        return 0xFFFFU;
    case ATA_Reg::ERROR:
        return this->r_error;
    case ATA_Reg::SEC_COUNT:
        return this->r_sect_count;
    case ATA_Reg::SEC_NUM:
        return this->r_sect_num;
    case ATA_Reg::CYL_LOW:
        return this->r_cylinder_lo;
    case ATA_Reg::CYL_HIGH:
        return this->r_cylinder_hi;
    case ATA_Reg::DEVICE_HEAD:
        return this->r_dev_head;
    case ATA_Reg::STATUS:
        // TODO: clear pending interrupt
        return this->r_status;
    case ATA_Reg::ALT_STATUS:
        return this->r_status;
    default:
        LOG_F(WARNING, "Attempted to read unknown IDE register: %x", reg_addr);
        return 0;
    }
}

void AtaBaseDevice::write(const uint8_t reg_addr, const uint16_t value) {
    switch (reg_addr) {
    case ATA_Reg::DATA:
        LOG_F(WARNING, "Pushing data to %s", this->name.c_str());
        break;
    case ATA_Reg::FEATURES:
        this->r_features = value;
        break;
    case ATA_Reg::SEC_COUNT:
        this->r_sect_count = value;
        break;
    case ATA_Reg::SEC_NUM:
        this->r_sect_num = value;
        break;
    case ATA_Reg::CYL_LOW:
        this->r_cylinder_lo = value;
        break;
    case ATA_Reg::CYL_HIGH:
        this->r_cylinder_hi = value;
        break;
    case ATA_Reg::DEVICE_HEAD:
        this->r_dev_head = value;
        break;
    case ATA_Reg::COMMAND:
        this->r_command = value;
        if (is_selected() || this->r_command == DIAGNOSTICS) {
            perform_command();
        }
        break;
    case ATA_Reg::DEV_CTRL:
        this->device_control(value);
        break;
    default:
        LOG_F(WARNING, "Attempted to write unknown IDE register: %x", reg_addr);
    }
}

void AtaBaseDevice::device_control(const uint8_t new_ctrl) {
    // perform ATA Soft Reset if requested
    if ((this->r_dev_ctrl ^ new_ctrl) & SRST) {
        if (new_ctrl & SRST) { // SRST set -> phase 0 aka self-test
            this->r_status |= BSY;
            this->device_reset(true);
        } else { // SRST cleared -> phase 1 aka signature and error report
            if (!this->my_dev_id && this->host_obj->is_device1_present())
                this->r_error |= 0x80;
            this->device_set_signature();
            this->r_status &= ~BSY;

            if (this->my_dev_id && this->r_error == 1) {
                this->host_obj->assert_pdiag();
            }
        }
    }
    this->r_dev_ctrl = new_ctrl;
}
