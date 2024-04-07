/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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

/** Aspen Memory Controller emulation. */

#include <devices/deviceregistry.h>
#include <devices/memctrl/aspen.h>
#include <loguru.hpp>

AspenCtrl::AspenCtrl() : MemCtrlBase() {
    this->name = "Aspen";

    supports_types(HWCompType::MEM_CTRL | HWCompType::MMIO_DEV);

    // add MMIO region for the configuration and status registers
    add_mmio_region(0xF8000000, 0x800, this);
}

int AspenCtrl::device_postinit() {
    return this->map_phys_ram();
}

void AspenCtrl::insert_ram_dimm(int bank_num, uint32_t capacity) {
    if (bank_num < 0 || bank_num > 3)
        return;

    uint32_t bank_size = capacity << 20; // convert to MB

    switch(bank_size) {
    case DRAM_CAP_1MB:
    case DRAM_CAP_4MB:
    case DRAM_CAP_8MB:
    case DRAM_CAP_16MB:
        bank_sizes[bank_num] = bank_size;
        break;
    default:
        break;
    }
}

uint32_t AspenCtrl::read(uint32_t rgn_start, uint32_t offset, int size) {
    uint8_t     reg_num = (offset >> 2) & 0x1F;
    uint32_t    reg_val;

    switch(reg_num) {
    case SYSTEM_ID:
        reg_val = 0x40010000;
        break;
    case CHIP_REV:
        reg_val = ASPEN_REV_1;
        break;
    case GPIO_IN:
    case GPIO_OUT:
        reg_val = 0;
        break;
    case GPIO_ENABLE:
        reg_val = this->gpio_enable << 24;
        break;
    default:
        LOG_F(WARNING, "%s: reading from register at 0x%X", this->name.c_str(), offset);
        reg_val = 0;
    }

    return reg_val;
}

void AspenCtrl::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) {
    uint8_t     reg_num = (offset >> 2) & 0x1F;

    switch(reg_num) {
    case SYSTEM_ID:
        break; // ignore writes to this read-only register
    case MEM_CONFIG:
        if (this->ram_config != value >> 24) {
            this->ram_config = value >> 24;
            LOG_F(INFO, "%s: RAM config changed to 0x%X", this->name.c_str(),
                  this->ram_config);
        }
        break;
    case GPIO_ENABLE:
        this->gpio_enable = value >> 24;
        break;
    case GPIO_OUT:
        LOG_F(INFO, "%s: output 0x%X to GPIO pins", this->name.c_str(), value >> 24);
        break;
    default:
        LOG_F(WARNING, "%s: unknown register write at offset 0x%X", this->name.c_str(),
              offset);
    }
}

int AspenCtrl::map_phys_ram() {
    uint32_t total_ram = 0, row_mask, col_mask, offset;

    for (int i = 0; i < 4; i++)
        total_ram += this->bank_sizes[i];

    LOG_F(INFO, "%s: total RAM size = %d bytes", this->name.c_str(), total_ram);

    uint32_t    addr = 0;

    for (int i = 0; i < 4; addr += DRAM_CAP_16MB, i++) {
        if (!this->bank_sizes[i])
            continue;
        if (!add_ram_region(addr, this->bank_sizes[i])) {
            LOG_F(ERROR, "%s: could not allocate RAM at 0x%X", this->name.c_str(), addr);
            return -1;
        }

        switch(this->bank_sizes[i]) {
        case DRAM_CAP_1MB:
            row_mask = (1 << 9) - 1;
            col_mask = (1 << 9) - 1;
            break;
        case DRAM_CAP_4MB:
            row_mask = (1 << 10) - 1;
            col_mask = (1 << 10) - 1;
            break;
        case DRAM_CAP_8MB:
            row_mask = (1 << 11) - 1;
            col_mask = (1 << 10) - 1;
            break;
        default: // DRAM_CAP_16MB
            row_mask = (1 << 11) - 1;
            col_mask = (1 << 11) - 1;
        }

        offset = ((0xC01000 >> 13) & row_mask) | ((0xC01000 >> 2) & col_mask);

        if (!this->add_mem_mirror_partial(addr + 0xC01000, addr + offset, offset, 0x1000)) {
            LOG_F(ERROR, "%s: could not create alias for RAM bank %d",
                  this->name.c_str(), i);
            return -1;
        }

        if (this->bank_sizes[i] < DRAM_CAP_16MB) {
            if (!this->add_mem_mirror_partial(addr + 0xC00000, addr + offset, offset, 0x1000)) {
                LOG_F(ERROR, "%s: could not create alias for RAM bank %d",
                      this->name.c_str(), i);
                return -1;
            }
        }

        if (this->bank_sizes[i] < DRAM_CAP_8MB) {
            if (!this->add_mem_mirror_partial(addr + 0x400000, addr + offset, offset, 0x1000)) {
                LOG_F(ERROR, "%s: could not create alias for RAM bank %d",
                      this->name.c_str(), i);
                return -1;
            }
        }
    }

    return 0;
}

static const DeviceDescription Aspen_Descriptor = {
    AspenCtrl::create, {}, {}
};

REGISTER_DEVICE(Aspen, Aspen_Descriptor);
