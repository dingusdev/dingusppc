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

/** PSX Memory Controller emulation. */

#include <devices/deviceregistry.h>
#include <devices/memctrl/psx.h>
#include <loguru.hpp>

#include <cinttypes>
#include <string>

PsxCtrl::PsxCtrl(int bridge_num, std::string name)
{
    this->name = name;

    supports_types(HWCompType::MEM_CTRL | HWCompType::MMIO_DEV);

    // add MMIO region for the PSX control registers
    add_mmio_region(0xF8000000, 0x70, this);

    this->sys_id     = 0x10000000;
    this->chip_rev   = 0; // old PSX, what's about PSX+?
    this->sys_config = PSX_BUS_SPEED_50;
}

uint32_t PsxCtrl::read(uint32_t rgn_start, uint32_t offset, int size)
{
    switch (offset >> 3) {
    case PsxReg::Sys_Id:
        return this->sys_id;
    case PsxReg::Revision:
        return this->chip_rev;
    case PsxReg::Sys_Config:
        return this->sys_config;
    default:
        LOG_F(WARNING, "PSX: read from unsupported control register at 0x%X", offset);
        return 0;
    }
}

void PsxCtrl::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    switch (offset >> 3) {
    case PsxReg::Sys_Config:
        this->sys_config = value;
        break;
    case PsxReg::ROM_Config:
        this->rom_cfg = value;
        break;
    case PsxReg::DRAM_Config:
        this->dram_cfg = value;
        break;
    case PsxReg::DRAM_Refresh:
        this->dram_refresh = value;
        break;
    case PsxReg::Flash_Config:
        this->flash_cfg = value;
        break;
    case PsxReg::Page1_Mapping:
    case PsxReg::Page2_Mapping:
    case PsxReg::Page3_Mapping:
    case PsxReg::Page4_Mapping:
    case PsxReg::Page5_Mapping:
        this->pages_cfg[(offset >> 3) - PsxReg::Page1_Mapping] = value;
        break;
    default:
        LOG_F(WARNING, "PSX: write to unsupported/read-only control register at 0x%X", offset);
    };
}

void PsxCtrl::insert_ram_dimm(int slot_num, uint32_t capacity)
{
    if (slot_num < 0 || slot_num >= 5) {
        ABORT_F("PSX: invalid DIMM slot %d", slot_num);
    }

    switch (capacity) {
    case 0:
        break;
    case DRAM_CAP_2MB:
    case DRAM_CAP_4MB:
    case DRAM_CAP_8MB:
    case DRAM_CAP_16MB:
    case DRAM_CAP_32MB:
        this->bank_sizes[slot_num] = capacity;
        break;
    default:
        ABORT_F("PSX: unsupported DRAM capacity %d", capacity);
    }
}

void PsxCtrl::map_phys_ram()
{
    uint32_t total_ram = 0;

    for (int i = 0; i < 5; i++) {
        total_ram += this->bank_sizes[i];
    }

    if (total_ram > DRAM_CAP_32MB) {
        ABORT_F("PSX: RAM bigger than 32MB not supported yet");
    }

    if (!add_ram_region(0x00000000, total_ram)) {
        ABORT_F("PSX: could not allocate RAM storage");
    }
}

static const DeviceDescription Psx_Descriptor = {
    PsxCtrl::create, {}, {}
};

REGISTER_DEVICE(Psx, Psx_Descriptor);
