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

/** Houdini2 PC Compatibility card definitions. */

#ifndef HOUDINI2_H
#define HOUDINI2_H

#include <devices/common/mmiodevice.h>

#include <cinttypes>
#include <memory>

enum PretzelLogicReg {
    MAIN_IER    = 0xE0,
    MAIN_ISR    = 0xE4,
    AIR_RESET   = 0xE8,
    AIR3        = 0x8004,
};

enum Air3Bits {
    SIMM_INSTALLED = 1 << 7, // if set, on-card memory SIMM is installed
};

// Pretzel Logic ASIC
class PretzelLogic : public MMIODevice {
public:
    PretzelLogic();
    ~PretzelLogic() = default;

    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

private:
    uint8_t air_reset;
    uint8_t air_status;
    uint8_t air2;
    uint8_t air3;
    uint8_t port_AB_dir;
    uint8_t port_AB_data;
    uint8_t main_ier;       // main interrupt enable register
};

class HoudiniCard : public HWComponent {
public:
    HoudiniCard();
    ~HoudiniCard() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<HoudiniCard>(new HoudiniCard());
    };

private:
    std::unique_ptr<PretzelLogic>   pl_obj;
};

#endif // HOUDINI2_H
