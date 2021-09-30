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

/** @file MESH (Macintosh Enhanced SCSI Hardware) controller emulation. */

#include "mesh.h"
#include <cinttypes>
#include <loguru.hpp>

uint8_t MESHController::read(uint8_t reg_offset)
{
    switch(reg_offset) {
    case MeshReg::BusStatus0:
        LOG_F(9, "MESH: read from BusStatus0 register");
        return 0;
    case MeshReg::MeshID:
        LOG_F(INFO, "MESH: read from MeshID register");
        return this->chip_id; // tell them who we are
    default:
        LOG_F(WARNING, "MESH: read from unimplemented register at offset %d", reg_offset);
    }

    return 0;
}

void MESHController::write(uint8_t reg_offset, uint8_t value)
{
    switch(reg_offset) {
    case MeshReg::Sequence:
        LOG_F(INFO, "MESH: write 0x%02X to Sequence register", value);
        break;
    case MeshReg::BusStatus1:
        LOG_F(INFO, "MESH: write 0x%02X to BusStatus1 register", value);
        break;
    case MeshReg::IntMask:
        LOG_F(INFO, "MESH: write 0x%02X to IntMask register", value);
        break;
    case MeshReg::Interrupt:
        LOG_F(INFO, "MESH: write 0x%02X to Interrupt register", value);
        break;
    case MeshReg::SourceID:
        LOG_F(INFO, "MESH: write 0x%02X to SourceID register", value);
        break;
    case MeshReg::SyncParms:
        LOG_F(INFO, "MESH: write 0x%02X to SyncParms register", value);
        break;
    default:
        LOG_F(WARNING, "MESH: write to unimplemented register at offset %d", reg_offset);
    }
}
