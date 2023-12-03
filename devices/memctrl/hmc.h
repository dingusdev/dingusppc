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

/** Bit in the HMC control register telling us where the frame bufffer
   for the on-board video is located. */
#define HMC_VBASE_BIT   33

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

    uint64_t get_control_reg(void) {
        return this->ctrl_reg;
    };

private:
    int      bit_pos;
    uint64_t ctrl_reg;
};

#endif // HMC_H
