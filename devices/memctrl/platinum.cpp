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

/** Platinum Memory Controller emulation. */

#include "platinum.h"
#include <loguru.hpp>

#include <cinttypes>

using namespace Platinum;

PlatinumCtrl::PlatinumCtrl() : MemCtrlBase()
{
    this->name = "Platinum Memory Controller";

    // add MMIO region for the configuration and status registers
    add_mmio_region(0xF8000000, 0x500, this);

    // initialize the CPUID register with the following CPU:
    // PowerPC 601 @ 75 MHz, bus frequency: 37,5 MHz
    this->cpu_id = (0x3001 << 16) | ClkSrc3 | (CpuSpeed3::CPU_75_BUS_38 << 8);
}

uint32_t PlatinumCtrl::read(uint32_t reg_start, uint32_t offset, int size)
{
    if (size != 4) {
        LOG_F(WARNING, "Platinum: unsupported register access size %d!", size);
        return 0;
    }

    switch (offset) {
    case PlatinumReg::CPU_ID:
        return this->cpu_id;
    default:
        LOG_F(WARNING, "Platinum: unknown register read at offset 0x%X", offset);
    }

    return 0;
}

void PlatinumCtrl::write(uint32_t reg_start, uint32_t offset, uint32_t value, int size)
{
    LOG_F(WARNING, "Platinum: unknown register write at offset 0x%X", offset);
}
