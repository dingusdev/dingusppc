/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 divingkatae and maximum
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
#include <algorithm>

PsxCtrl::PsxCtrl(int bridge_num, std::string name)
{
    this->name = name;

    supports_types(HWCompType::MEM_CTRL | HWCompType::MMIO_DEV);

    // add MMIO region for the PSX control registers
    this->add_mmio_region(0xF8000000, 0x70, this);

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
    case PsxReg::Page_Mappings_1:
    case PsxReg::Page_Mappings_2:
    case PsxReg::Page_Mappings_3:
    case PsxReg::Page_Mappings_4:
    case PsxReg::Page_Mappings_5:
        LOG_F(INFO, "PSX: write Page Mappings %d @%02x.%c = %0*x",
            (offset >> 3) - PsxReg::Page_Mappings_1 + 1, offset, SIZE_ARG(size), size * 2, value);
        this->pages_cfg[(offset >> 3) - PsxReg::Page_Mappings_1] = value;
        this->map_phys_ram();
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
    case DRAM_CAP_4MB:
    case DRAM_CAP_8MB:
    case DRAM_CAP_16MB:
    case DRAM_CAP_32MB:
        this->bank_size[slot_num] = capacity;
        break;
    case DRAM_CAP_64MB:
        this->bank_size[slot_num + 0] = capacity / 2;
        this->bank_size[slot_num + 1] = capacity / 2;
        break;
    default:
        ABORT_F("PSX: unsupported DRAM capacity %d", capacity);
    }
}

void PsxCtrl::map_phys_ram()
{
    uint32_t total_ram = 0;
    uint32_t bank_size_remaining[5];

    for (int i = 0; i < 5; i++) {
        bank_size_remaining[i] = this->bank_size[i];
        total_ram += this->bank_size[i];
    }

    if (!this->dram_ptr) {
        this->dram_ptr = std::unique_ptr<uint8_t[]> (new uint8_t[total_ram]()); /* () intializer clears the memory to zero */
        if (!this->dram_ptr) {
            ABORT_F("%s: could not allocate RAM storage", this->name.c_str());
        }
    }

    ram_map.erase(std::remove_if(ram_map.begin(), ram_map.end(),
        [this](AddressMapEntry *entry) {
            delete this->remove_region(entry);
            return true;
        }
    ), ram_map.end());

    uint32_t ram_offset = -1;
    uint32_t mem_start = 0;
    for (int page = 0; page < 41; page++) {
        int bank = page >= 40 ? 8 : (this->pages_cfg[page / 8] >> ((page % 8) * 4)) & 15;
        //LOG_F(INFO, "page:%02d bank:%d", page, bank);
        if ((ram_offset == -1) == (bank < 5 && bank_size_remaining[bank] > 0)) {
            if (ram_offset == -1) {
                ram_offset = page * DRAM_CAP_4MB;
                //LOG_F(INFO, "ram_offset:%08x", ram_offset);
            } else {
                uint32_t mem_size = page * DRAM_CAP_4MB - ram_offset;
                AddressMapEntry *entry = this->add_ram_region(ram_offset, mem_size, &dram_ptr[mem_start]);
                mem_start += mem_size;
                ram_offset = -1;
                if (entry) {
                    ram_map.push_back(entry);
                } else {
                    ABORT_F("%s: could not allocate RAM storage", this->name.c_str());
                }
            }
        }
        if (ram_offset != -1) {
            bank_size_remaining[bank] -= DRAM_CAP_4MB;
        }
    }
}

static const DeviceDescription Psx_Descriptor = {
    PsxCtrl::create, {}, {}, HWCompType::MEM_CTRL | HWCompType::MMIO_DEV
};

REGISTER_DEVICE(Psx, Psx_Descriptor);
