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

#include <devices/common/ata/ata_full.h>
#include <devices/deviceregistry.h>
#include <fstream>
#include <limits>
#include <loguru.hpp>
#include <stdio.h>

#define sector_size 512

using namespace std;

AtaFullDevice::AtaFullDevice() {
    supports_types(HWCompType::IDE_DEV);

    regs[IDE_Reg::ERROR]      = IDE_Error::ANMF;
    regs[IDE_Reg::SEC_COUNT]  = 0x1;
    regs[IDE_Reg::SEC_NUM]    = 0x1;
    regs[IDE_Reg::STATUS]     = IDE_Status::DRDY | IDE_Status::DSC;
    regs[IDE_Reg::ALT_STATUS] = IDE_Status::DRDY | IDE_Status::DSC;
}

void AtaFullDevice::insert_image(std::string filename) {
    this->ide_img.open(filename, ios::out | ios::in | ios::binary);

    // Taken from:
    // https://stackoverflow.com/questions/22984956/tellg-function-give-wrong-size-of-file/22986486
    ide_img.ignore(std::numeric_limits<std::streamsize>::max());
    img_size = this->ide_img.gcount();
    ide_img.clear();    //  Since ignore will have set eof.
    ide_img.seekg(0, std::ios_base::beg);
}

uint32_t AtaFullDevice::read(int reg) {
    switch (reg) {
    case IDE_Reg::IDE_DATA:
        LOG_F(0, "Retrieving DATA from IDE: %x", regs[IDE_Reg::IDE_DATA]);
        return regs[IDE_Reg::IDE_DATA];
    case IDE_Reg::ERROR:
        return regs[IDE_Reg::ERROR];
    case IDE_Reg::SEC_COUNT:
        return regs[IDE_Reg::SEC_COUNT];
    case IDE_Reg::SEC_NUM:
        return regs[IDE_Reg::SEC_NUM];
    case IDE_Reg::CYL_LOW:
        return regs[IDE_Reg::CYL_LOW];
    case IDE_Reg::CYL_HIGH:
        return regs[IDE_Reg::CYL_HIGH];
    case IDE_Reg::DRIVE_HEAD:
        return regs[IDE_Reg::DRIVE_HEAD];
    case IDE_Reg::STATUS:
        return regs[IDE_Reg::STATUS];
    case IDE_Reg::ALT_STATUS:
        return regs[IDE_Reg::ALT_STATUS];
    case IDE_Reg::TIME_CONFIG:
        return regs[IDE_Reg::TIME_CONFIG];
    default:
        LOG_F(WARNING, "Attempted to read unknown IDE register: %x", reg);
        return 0x0;
    }
}

void AtaFullDevice::write(int reg, uint32_t value) {
    switch (reg) {
    case IDE_Reg::IDE_DATA:
        regs[IDE_Reg::IDE_DATA] = value;
        break;
    case IDE_Reg::FEATURES:
        regs[IDE_Reg::FEATURES] = value;
        break;
    case IDE_Reg::SEC_COUNT:
        regs[IDE_Reg::SEC_COUNT] = value;
        break;
    case IDE_Reg::SEC_NUM:
        regs[IDE_Reg::SEC_NUM] = value;
        break;
    case IDE_Reg::CYL_LOW:
        regs[IDE_Reg::CYL_LOW] = value;
        break;
    case IDE_Reg::CYL_HIGH:
        regs[IDE_Reg::CYL_HIGH] = value;
        break;
    case IDE_Reg::DRIVE_HEAD:
        regs[IDE_Reg::DRIVE_HEAD] = value;
        break;
    case IDE_Reg::COMMAND:
        regs[IDE_Reg::COMMAND] = value;
        LOG_F(0, "Executing COMMAND for IDE: %x", value);
        perform_command(value);
        break;
    case IDE_Reg::DEV_CTRL:
        regs[IDE_Reg::DEV_CTRL] = value;
        break;
    case IDE_Reg::TIME_CONFIG:
        regs[IDE_Reg::TIME_CONFIG] = value;
        break;
    default:
        LOG_F(WARNING, "Attempted to write unknown IDE register: %x", reg);
    }
}

int AtaFullDevice::perform_command(uint32_t command) {
    switch (command) {
    case IDE_Cmd::READ_SECTOR:
        LOG_F(WARNING, "Trying to read sector with: %x", command);
        break;
    case IDE_Cmd::WRITE_SECTOR:
        LOG_F(WARNING, "Trying to write sector with: %x", command);
        break;
    default:
        LOG_F(WARNING, "Attempted to execute IDE command: %x", command);
    }

    return 0;
}


static const DeviceDescription ATA_Full_Descriptor = {
    AtaFullDevice::create, {}, {}
};

REGISTER_DEVICE(AtaFullDevice, ATA_Full_Descriptor);