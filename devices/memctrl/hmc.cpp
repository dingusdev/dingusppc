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

/** Highspeed Memory Controller emulation.

    Author: Max Poliakovski
*/

#include <devices/deviceregistry.h>
#include <devices/common/hwcomponent.h>
#include <devices/memctrl/hmc.h>

HMC::HMC() : MemCtrlBase()
{
    this->name = "Highspeed Memory Controller";

    supports_types(HWCompType::MEM_CTRL | HWCompType::MMIO_DEV);

    /* add memory mapped I/O region for the HMC control register */
    add_mmio_region(0x50F40000, 0x10000, this);

    this->ctrl_reg = 0ULL;
    this->bit_pos = 0;
}

uint32_t HMC::read(uint32_t reg_start, uint32_t offset, int size)
{
    if (!offset)
        return !!(this->ctrl_reg & (1ULL << this->bit_pos++));
    else
        return 0; /* FIXME: what should be returned for invalid offsets? */
}

void HMC::write(uint32_t reg_start, uint32_t offset, uint32_t value, int size)
{
    uint64_t bit;

    switch(offset) {
        case 0:
            bit = 1ULL << this->bit_pos++;
            this->ctrl_reg = (value & 1) ? this->ctrl_reg | bit :
                               this->ctrl_reg & ~bit;
            break;
        case 8: /* writing to HMCBase + 8 resets internal bit position */
            this->bit_pos = 0;
            break;
    }
}

static const DeviceDescription Hmc_Descriptor = {
    HMC::create, {}, {}
};

REGISTER_DEVICE(HMC, Hmc_Descriptor);
