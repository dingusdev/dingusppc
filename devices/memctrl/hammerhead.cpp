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

/** Hammerhead Memory Controller emulation. */

#include <devices/deviceregistry.h>
#include <devices/memctrl/hammerhead.h>
#include <loguru.hpp>

#include <cinttypes>
#include <memory>

using namespace Hammerhead;

HammerheadCtrl::HammerheadCtrl() : MemCtrlBase()
{
    this->name = "Hammerhead Memory Controller";

    supports_types(HWCompType::MEM_CTRL | HWCompType::MMIO_DEV);

    // add MMIO region for the configuration and status registers
    add_mmio_region(0xF8000000, 0x500, this);
}

uint32_t HammerheadCtrl::read(uint32_t rgn_start, uint32_t offset, int size)
{
    uint32_t result;

    if (offset >= HammerheadReg::BANK_0_BASE_MSB &&
        offset <= HammerheadReg::BANK_25_BASE_LSB) {
        offset = (offset - HammerheadReg::BANK_0_BASE_MSB) >> 4;
        if (offset & 1) { // return the LSB part
            result = bank_base[offset >> 1] & 0xFFU;
        } else { // return the MSB part
            result = bank_base[offset >> 1] >> 8;
        }
        goto finish;
    }

    switch (offset) {
    case HammerheadReg::CPU_ID:
        result = HH_CPU_ID_TNT;
        break;
    case HammerheadReg::MOTHERBOARD_ID:
        result = MBID_VCI0_PRESENT << 5;
        break;
    case HammerheadReg::CPU_SPEED:
        result = BUS_SPEED_33_MHZ << 5;
        break;
    case HammerheadReg::ARBITER_CONFIG:
        result = this->arb_config;
        break;
    case HammerheadReg::WHO_AM_I:
        result = BM_PRIMARY_CPU << 3;
        break;
    case HammerheadReg::L2_CACHE_CONFIG:
        result = 0; // say there is no L2 cache
        break;
    default:
        LOG_F(WARNING, "Hammerhead: unknown register read at offset 0x%X", offset);
        return 0;
    }

    // Hammerhead registers are one byte wide so always place
    // the result in the MSB of a multibyte read
finish:
    return result << ((size - 1) << 3);
}

void HammerheadCtrl::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    // extract byte value from the MSB of a multibyte value
    value = value >> ((size - 1) << 3);

    if (offset >= HammerheadReg::BANK_0_BASE_MSB &&
        offset <= HammerheadReg::BANK_25_BASE_LSB) {
        offset = (offset - HammerheadReg::BANK_0_BASE_MSB) >> 4;
        if (offset & 1) { // update the LSB part
            bank_base[offset >> 1] = (bank_base[offset >> 1] & 0xFF00U) | value;
        } else { // update the MSB part
            bank_base[offset >> 1] = (bank_base[offset >> 1] & 0x00FFU) | (value << 8);
        }
        LOG_F(INFO, "Hammerhead: bank base #%d set to 0x%X", offset >> 1, bank_base[offset >> 1]);
        return;
    }

    switch (offset) {
    case HammerheadReg::MEM_TIMING_0:
        LOG_F(9, "Hammerhead: MEM_TIMING_0 set to 0x%X", value);
        break;
    case HammerheadReg::MEM_TIMING_1:
        LOG_F(9, "Hammerhead: MEM_TIMING_1 set to 0x%X", value);
        break;
    case HammerheadReg::REFRESH_TIMING:
        LOG_F(9, "Hammerhead: REFRESH_TIMING set to 0x%X", value);
        break;
    case HammerheadReg::ROM_TIMING:
        LOG_F(9, "Hammerhead: ROM_TIMING set to 0x%X", value);
        break;
    case HammerheadReg::ARBITER_CONFIG:
        this->arb_config = value;
        break;
    default:
        LOG_F(WARNING, "Hammerhead: unknown register write at offset 0x%X", offset);
    }
}

void HammerheadCtrl::insert_ram_dimm(int slot_num, uint32_t capacity)
{
    if (slot_num < 0 || slot_num >= 26) {
        ABORT_F("Hammerhead: invalid DIMM slot number %d", slot_num);
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
        ABORT_F("Hammerhead: unsupported DRAM capacity %d", capacity);
    }
}

void HammerheadCtrl::map_phys_ram()
{
    uint32_t total_ram = 0;

    for (int i = 0; i < 26; i++) {
        total_ram += this->bank_size[i];
    }

    LOG_F(INFO, "Hammerhead: total RAM size = %d bytes", total_ram);

    if (!add_ram_region(0x00000000, total_ram)) {
        ABORT_F("Hammerhead: could not allocate physical RAM storage");
    }
}

static const DeviceDescription Hammerhead_Descriptor = {
    HammerheadCtrl::create, {}, {}
};

REGISTER_DEVICE(Hammerhead, Hammerhead_Descriptor);
