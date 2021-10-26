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

/** @file NCR 53C94 SCSI controller emulation. */

#include "ncr53c94.h"
#include <loguru.hpp>

#include <cinttypes>

uint8_t Ncr53C94::read(uint8_t reg_offset)
{
    LOG_F(9, "NCR53C94: reading from register %d", reg_offset);
    return 0;
}

void Ncr53C94::write(uint8_t reg_offset, uint8_t value)
{
    LOG_F(INFO, "NCR53C94: writing 0x%X to %d register", value, reg_offset);
}
