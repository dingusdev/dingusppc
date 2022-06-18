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

/** Platinum Memory/Display Controller emulation. */

#include <devices/memctrl/platinum.h>
#include <devices/video/displayid.h>
#include <loguru.hpp>

#include <cinttypes>
#include <memory>

using namespace Platinum;

PlatinumCtrl::PlatinumCtrl() : MemCtrlBase()
{
    this->name = "Platinum Memory Controller";

    // add MMIO region for the configuration and status registers
    add_mmio_region(0xF8000000, 0x500, this);

    // determine actual VRAM size (min. 1MB, max. 4MB)
    this->vram_size = 1 << 20;

    // insert video memory region into the main memory map
    this->add_ram_region(0xF1000000UL, this->vram_size);

    // initialize the CPUID register with the following CPU:
    // PowerPC 601 @ 75 MHz, bus frequency: 37,5 MHz
    this->cpu_id = (0x3001 << 16) | ClkSrc3 | (CpuSpeed3::CPU_75_BUS_38 << 8);

    this->display_id = std::unique_ptr<DisplayID> (new DisplayID());
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
    case PlatinumReg::FB_BASE_ADDR:
        return this->fb_addr;
    case PlatinumReg::MON_ID_SENSE:
        LOG_F(INFO, "Platinum: display sense read");
        return (this->cur_mon_id ^ 7);
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
    case PlatinumReg::FB_BASE_ADDR:
        this->fb_addr = value;
        LOG_F(INFO, "Platinum: Framebuffer address set to 0x%X", value);
        break;
    case PlatinumReg::FB_CONFIG_1:
        this->fb_config_1 = value;
        break;
    case PlatinumReg::FB_CONFIG_2:
        this->fb_config_2 = value;
        break;
    case PlatinumReg::VMEM_PAGE_MODE:
        this->vmem_fp_mode = value;
        break;
    case PlatinumReg::MON_ID_SENSE:
        LOG_F(INFO, "Platinum: display sense written with 0x%X", value);
        this->cur_mon_id = this->display_id->read_monitor_sense(value & 7, value ^ 7);
        break;
    case PlatinumReg::FB_RESET:
        this->fb_reset = value;
        break;
    case PlatinumReg::VRAM_REFRESH:
        this->vram_refresh = value;
        break;
    case PlatinumReg::SWATCH_CONFIG:
        this->swatch_config = value;
        break;
    case PlatinumReg::SWATCH_INT_MASK:
        this->swatch_int_mask = value;
        break;
    case PlatinumReg::SWATCH_HAL:
        LOG_F(INFO, "Swatch HAL set to 0x%X", value);
        break;
    case PlatinumReg::SWATCH_HFP:
        LOG_F(INFO, "Swatch HFP set to 0x%X", value);
        break;
    case PlatinumReg::SWATCH_HPIX:
        LOG_F(INFO, "Swatch HPIX set to 0x%X", value);
        break;
    case PlatinumReg::SWATCH_VAL:
        LOG_F(INFO, "Swatch VAL set to 0x%X", value);
        break;
    case PlatinumReg::SWATCH_VFP:
        LOG_F(INFO, "Swatch VFP set to 0x%X", value);
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
