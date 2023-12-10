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

/** Hammerhead Memory Controller definitions.

    Author: Max Poliakovski

    Hammerhead is a custom memory controller and ARBus arbiter
    designed especially for the TNT family of Power Macintosh computers.
*/

#ifndef HAMMERHEAD_MEMCTRL_H
#define HAMMERHEAD_MEMCTRL_H

#include <devices/common/hwcomponent.h>
#include <devices/common/mmiodevice.h>
#include <devices/memctrl/memctrlbase.h>

#include <cinttypes>
#include <memory>

namespace Hammerhead {
    #define RISC_MACHINE  0x3
    #define MACH_TYPE_TNT 0x1
    #define EXTENDED_ID   0x1 // examine some other registers to get full ID
    #define HH_CPU_ID_TNT ((RISC_MACHINE << 4) | (EXTENDED_ID << 3) | MACH_TYPE_TNT)

    // constants for the MBID field of the MOTHERBOARD_ID register
    enum {
        MBID_VCI0_PRESENT = 4,
        MBID_PCI2_PRESENT = 2,
    };

    // constants for the BURST_ROM field of the MOTHERBOARD_ID register
    enum {
        MBID_BURST_ROM = 1,
    };

    // ARBus speed constants for the CPU_SPEED register
    enum {
        BUS_SPEED_40_MHZ = 0,
        BUS_SPEED_33_MHZ = 1,
        BUS_SPEED_44_MHZ = 2,
        BUS_SPEED_50_MHZ = 3
    };

    // bus master IDs for the WHO_AM_I register
    enum {
        BM_VIDEO_BRIDGE  = 1 << 4,
        BM_PCI_BRIDGE_1  = 1 << 3,
        BM_PCI_BRIDGE_2  = 1 << 2,
        BM_PRIMARY_CPU   = 1 << 1,
        BM_SECONDARY_CPU = 1 << 0
    };

    // Configuration and status registers.
    enum HammerheadReg : uint16_t {
        CPU_ID           =  0x00,
        ASIC_REVISION    =  0x10,
        MOTHERBOARD_ID   =  0x20,
        CPU_SPEED        =  0x30,
        MB_DRAM_CONFIG   =  0x40,
        MEM_TIMING_0     =  0x50,
        MEM_TIMING_1     =  0x60,
        REFRESH_TIMING   =  0x70,
        ROM_TIMING       =  0x80,
        ARBITER_CONFIG   =  0x90,
        ARBUS_TIMEOUT    =  0xA0,
        WHO_AM_I         =  0xB0,
        L2_CACHE_CONFIG  =  0xE0,
        BANK_0_BASE_MSB  = 0x1C0,
        BANK_25_BASE_LSB = 0x4F0,
    };

}; // namespace Hammerhead

class HammerheadCtrl : public MemCtrlBase, public MMIODevice {
public:
    HammerheadCtrl();
    ~HammerheadCtrl() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<HammerheadCtrl>(new HammerheadCtrl());
    }

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

    void set_motherboard_id(const uint8_t val) {
        this->mb_id = val & 7;
    };

    void set_rom_type(const uint8_t val) {
        this->rom_type = val & 1;
    };

    void set_bus_speed(const uint8_t val) {
        this->bus_speed = val & 7;
    };

    void insert_ram_dimm(int slot_num, uint32_t capacity);
    void map_phys_ram();

private:
    uint8_t     mb_id      = 0;
    uint8_t     rom_type   = Hammerhead::MBID_BURST_ROM;
    uint8_t     bus_speed  = Hammerhead::BUS_SPEED_40_MHZ;
    uint8_t     arb_config = 0;

    uint16_t    bank_base[26] = {};
    uint32_t    bank_size[26] = {};
};

#endif // HAMMERHEAD_MEMCTRL_H
