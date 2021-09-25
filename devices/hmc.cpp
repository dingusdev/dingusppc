/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-20 divingkatae and maximum
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

/** Highspeed Memory Controller emulation.

    Author: Max Poliakovski
*/

#include "hmc.h"

HMC::HMC() : MemCtrlBase()
{
    this->name = "Highspeed Memory Controller";

    /* add memory mapped I/O region for the HMC control register */
    add_mmio_region(0x50F40000, 0xFFFF, this);

    this->config_reg = 0ULL;
    this->bit_pos = 0;
}

bool HMC::supports_type(HWCompType type) {
    if (type == HWCompType::MEM_CTRL || type == HWCompType::MMIO_DEV) {
        return true;
    } else {
        return false;
    }
}

uint32_t HMC::read(uint32_t reg_start, uint32_t offset, int size)
{
    if (!offset)
        return !!(this->config_reg & (1ULL << this->bit_pos++));
    else
        return 0; /* FIXME: what should be returned for invalid offsets? */
}

void HMC::write(uint32_t reg_start, uint32_t offset, uint32_t value, int size)
{
    uint64_t bit;

    switch(offset) {
        case 0:
            bit = 1ULL << this->bit_pos++;
            this->config_reg = (value & 1) ? this->config_reg | bit :
                               this->config_reg & ~bit;
            break;
        case 8: /* writing to HMCBase + 8 resets internal bit position */
            this->bit_pos = 0;
            break;
    }
}
