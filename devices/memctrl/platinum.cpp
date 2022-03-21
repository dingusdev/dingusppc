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
    case PlatinumReg::DRAM_REFRESH:
        return this->dram_refresh;
    case PlatinumReg::BANK_0_BASE:
    case PlatinumReg::BANK_1_BASE:
    case PlatinumReg::BANK_2_BASE:
    case PlatinumReg::BANK_3_BASE:
    case PlatinumReg::BANK_4_BASE:
    case PlatinumReg::BANK_5_BASE:
    case PlatinumReg::BANK_6_BASE:
    case PlatinumReg::BANK_7_BASE:
        return this->bank_base[(offset - PlatinumReg::BANK_0_BASE) >> 4];
    case PlatinumReg::CACHE_CONFIG:
        return 0; // report no L2 cache installed
    default:
        LOG_F(WARNING, "Platinum: unknown register read at offset 0x%X", offset);
    }

    return 0;
}

void PlatinumCtrl::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    switch (offset) {
    case PlatinumReg::ROM_TIMING:
        this->rom_timing = value;
        break;
    case PlatinumReg::DRAM_TIMING:
        this->dram_timing = value;
        break;
    case PlatinumReg::DRAM_REFRESH:
        this->dram_refresh = value;
        break;
    case PlatinumReg::FB_CONFIG_2:
        this->fb_config_2 = value;
        break;
    case PlatinumReg::VRAM_REFRESH:
        this->vram_refresh = value;
        break;
    case PlatinumReg::BANK_0_BASE:
    case PlatinumReg::BANK_1_BASE:
    case PlatinumReg::BANK_2_BASE:
    case PlatinumReg::BANK_3_BASE:
    case PlatinumReg::BANK_4_BASE:
    case PlatinumReg::BANK_5_BASE:
    case PlatinumReg::BANK_6_BASE:
    case PlatinumReg::BANK_7_BASE:
        this->bank_base[(offset - PlatinumReg::BANK_0_BASE) >> 4] = value;
        break;
    default:
        LOG_F(WARNING, "Platinum: unknown register write at offset 0x%X", offset);
    }
}

void PlatinumCtrl::insert_ram_dimm(int slot_num, uint32_t capacity)
{
    if (slot_num < 0 || slot_num >= 4) {
        ABORT_F("Platinum: invalid DIMM slot %d", slot_num);
    }

    switch (capacity) {
    case DRAM_CAP_2MB:
    case DRAM_CAP_4MB:
    case DRAM_CAP_8MB:
    case DRAM_CAP_16MB:
    case DRAM_CAP_32MB:
    case DRAM_CAP_64MB:
        this->bank_size[slot_num * 2 + 0] = capacity;
        break;
    case DRAM_CAP_128MB:
        this->bank_size[slot_num * 2 + 0] = DRAM_CAP_64MB;
        this->bank_size[slot_num * 2 + 1] = DRAM_CAP_64MB;
        break;
    default:
        ABORT_F("Platinum: unsupported DRAM capacity %d", capacity);
    }
}

void PlatinumCtrl::map_phys_ram()
{
    uint32_t total_ram = 0;

    for (int i = 0; i < 8; i++) {
        total_ram += this->bank_size[i];
    }

    if (total_ram > DRAM_CAP_64MB) {
        ABORT_F("Platinum: RAM bigger than 64MB not supported yet");
    }

    if (!add_ram_region(0x00000000, total_ram)) {
        ABORT_F("Platinum: could not allocate RAM storage");
    }
}
