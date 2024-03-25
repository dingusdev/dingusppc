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

/** Highspeed Memory Controller emulation.

    Author: Max Poliakovski

    Highspeed Memory Controller (HMC) is a custom memory
    and L2 cache controller designed especially for the
    first generation of the Power Macintosh computer.
*/

#ifndef HMC_H
#define HMC_H

#include <devices/common/mmiodevice.h>
#include <devices/memctrl/memctrlbase.h>

#include <cinttypes>
#include <memory>

/* Control register bit definitions. */
enum {
    HMC_CTRL_BITS   = 35, // width of the HMC control register
    HMC_RAM_CFG_POS = 29, // DRAM configuration bits position
    HMC_VBASE_BIT   = 33, // framebuffer base address: 0 - 0x00100000, 1 - 0x0
};

/* Motherboard RAM size constans. */
enum {
    BANK_CFG_128MB  = 0, // 12 x 12 address maxtrix (reset config)
    BANK_CFG_2MB    = 1, //  9 x  9 address maxtrix
    BANK_CFG_8MB    = 2, // 10 x 10 address maxtrix
    BANK_CFG_32MB   = 3, // 11 x 11 address maxtrix
    BANK_SIZE_2MB   = 0x200000,
    BANK_SIZE_4MB   = 0x400000,
    BANK_SIZE_8MB   = 0x800000,
    BANK_SIZE_32MB  = 0x2000000,
    BANK_SIZE_120MB = 0x07800000,
    BANK_MB_START   = 0x00000000, // starting address of the motherboard bank
    BANK_B_START    = 0x08000000,
    BANK_A_ALIAS    = 0x10000000,
};

class HMC : public MemCtrlBase, public MMIODevice {
public:
    HMC();
    ~HMC() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<HMC>(new HMC());
    }

    /* MMIODevice methods */
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

    int install_ram(uint32_t mb_bank_size, uint32_t bank_a_size, uint32_t bank_b_size);

    uint64_t get_control_reg(void) {
        return this->ctrl_reg;
    };

protected:
    void remap_ram_regions();

private:
    int         bit_pos     = 0;
    uint64_t    ctrl_reg    = 0;
    uint8_t     bank_config = BANK_CFG_128MB;

    uint32_t    mb_bank_start   = -1;
    uint32_t    bank_a_start    = -1;
    uint32_t    bank_b_start    = -1;

    uint32_t    mb_bank_size    = 0;
    uint32_t    bank_a_size     = 0;
    uint32_t    bank_b_size     = 0;
};

#endif // HMC_H
