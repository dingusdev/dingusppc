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

/** @file Media Access Controller for Ethernet (MACE) emulation. */

#include <devices/deviceregistry.h>
#include <devices/ethernet/mace.h>
#include <loguru.hpp>

#include <cinttypes>

using namespace MaceEnet;

uint8_t MaceController::read(uint8_t reg_offset)
{
    switch(reg_offset) {
    case MaceReg::Interrupt:
        LOG_F(INFO, "MACE: all interrupt flags cleared");
        return 0;
    default:
        LOG_F(INFO, "Reading MACE register %d", reg_offset);
    }

    return 0;
}

void MaceController::write(uint8_t reg_offset, uint8_t value)
{
    switch(reg_offset) {
    case MaceReg::BIU_Config_Ctrl:
        if (value & 1) {
            LOG_F(INFO, "MACE Reset issued");
        } else {
            LOG_F(INFO, "MACE BIU Config set to 0x%X", value);
        }
        break;
    default:
        LOG_F(INFO, "Writing 0x%X to MACE register %d", value, reg_offset);
    }
}

static const DeviceDescription Mace_Descriptor = {
    MaceController::create, {}, {}
};

REGISTER_DEVICE(Mace, Mace_Descriptor);
