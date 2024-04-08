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

/** Aspem Memory Controller definitions.

    Aspen is a custom memory controller and PCI bridge
    designed especially for Pippin.

    Kudos to Keith Kaisershot @ blitter.net for his precious technical help!
*/

#ifndef ASPEN_MEMCTRL_H
#define ASPEN_MEMCTRL_H

#include <devices/common/mmiodevice.h>
#include <devices/memctrl/memctrlbase.h>

#include <cinttypes>
#include <memory>

/** Aspen register definitions. */
enum {
    SYSTEM_ID   =  4, // 0x10
    CHIP_REV    =  5, // 0x14
    MEM_CONFIG  =  6, // 0x18
    GPIO_IN     = 12, // 0x30
    GPIO_ENABLE = 13, // 0x34
    GPIO_OUT    = 14, // 0x38
};

#define ASPEN_REV_1 0x01000000

/** Aspen RAM bank size bits. */
enum {
    BANK_SIZE_1MB   = 0, // 256Kx16, 9 rows x 9 columns
    BANK_SIZE_4MB   = 1, // 1Mx16, 10 rows x 10 columns
    BANK_SIZE_16MB  = 2, // 4Mx16, 11 rows x 11 columns
    BANK_SIZE_8MB   = 3, // 2Mx16, 11 rows x 10 columns
};

class AspenCtrl : public MemCtrlBase, public MMIODevice {
public:
    AspenCtrl();
    ~AspenCtrl() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<AspenCtrl>(new AspenCtrl());
    }

    int device_postinit() override;

    void insert_ram_dimm(int bank_num, uint32_t capacity);

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;

private:
    int     map_phys_ram();

    uint8_t     gpio_enable = 0;

    uint32_t    bank_sizes[4] = {};
    uint8_t     ram_config = (BANK_SIZE_16MB << 6) | (BANK_SIZE_16MB << 4) |
                             (BANK_SIZE_16MB << 2) | (BANK_SIZE_16MB << 0);
};

#endif // ASPEN_MEMCTRL_H
