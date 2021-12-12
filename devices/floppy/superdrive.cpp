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

/** @file Macintosh Superdrive emulation. */

#include "superdrive.h"
#include <loguru.hpp>

#include <cinttypes>

using namespace MacSuperdrive;

MacSuperDrive::MacSuperDrive()
{
    this->media_kind = MediaKind::high_density;
}

void MacSuperDrive::command(uint8_t addr, uint8_t value)
{
    LOG_F(9, "Superdrive: command addr=0x%X, value=%d", addr, value);

    switch(addr) {
    case CommandAddr::Motor_On_Off:
        if (value) {
            LOG_F(INFO, "Superdrive: turn spindle motor off");
        } else {
            LOG_F(INFO, "Superdrive: turn spindle motor on");
        }
        break;
    default:
        LOG_F(WARNING, "Superdrive: unimplemented command, addr=0x%X", addr);
    }
}

uint8_t MacSuperDrive::status(uint8_t addr)
{
    LOG_F(9, "Superdrive: status request, addr = 0x%X", addr);

    switch(addr) {
    case StatusAddr::MFM_Support:
        return 1; // Superdrive does support MFM encoding scheme
    case StatusAddr::Double_Sided:
        return 1; // yes, Superdrive is double sided
    case StatusAddr::Drive_Exists:
        return 0; // tell the world I'm here
    case StatusAddr::Media_Kind:
        return this->media_kind;
    default:
        LOG_F(WARNING, "Superdrive: unimplemented status request, addr=0x%X", addr);
        return 0;
    }
}
