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

/** @file Null ATA device */

#include <devices/common/ata/ata_null.h>
#include <devices/deviceregistry.h>
#include <cinttypes>
#include <fstream>
#include <loguru.hpp>

AtaNullDevice::AtaNullDevice() {
    supports_types(HWCompType::IDE_DEV);

    regs[IDE_Reg::STATUS]     = IDE_Status::DRDY | IDE_Status::DSC;
    regs[IDE_Reg::ALT_STATUS] = IDE_Status::DRDY | IDE_Status::DSC;
}

uint32_t AtaNullDevice::read(int reg) {
    if (reg == IDE_Reg::ERROR) {
        return IDE_Error::TK0NF;
    }
    else if (reg == IDE_Reg::STATUS) {
        return IDE_Status::ERR;
    }
    LOG_F(WARNING, "Dummy read for IDE register 0x%x", reg);
    return 0x0;
}

void AtaNullDevice::write(int reg, uint32_t value) {
        if (reg == IDE_Reg::COMMAND) {
            process_command(value);
        }
        LOG_F(WARNING, "Dummy write for IDE register 0x%x with value 0x%x", reg, value);
}

int AtaNullDevice::process_command(uint32_t cmd) {
    LOG_F(ERROR, "Attempted to execute command %x", cmd);
    return 0;
}

static const DeviceDescription ATA_Null_Descriptor = {
    AtaNullDevice::create, {}, {}
};

REGISTER_DEVICE(AtaNullDevice, ATA_Null_Descriptor);