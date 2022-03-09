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

/** @file Media Access Controller for Ethernet (MACE) definitions. */

#ifndef MACE_H
#define MACE_H

#include <cinttypes>

// MACE Chip ID from AMD datasheet
// TODO: compare with real HW
#define MACE_ID 0x3940

// MACE registers offsets
// Refer to the Am79C940 datasheet for details
namespace MaceEnet {

enum MaceReg : uint8_t {
    Rcv_FIFO        = 0,
    Xmit_FIFO       = 1,
    Xmit_Frame_Ctrl = 2,
    Xmit_Frame_Stat = 3,
    Xmit_Retry_Cnt  = 4,
    Rcv_Frame_Ctrl  = 5,
    Rcv_Frame_Stat  = 6,
    FIFO_Frame_Cnt  = 7,
    Interrupt       = 8,
    Interrupt_Mask  = 9,
    Poll            = 0x0A,
    BIU_Config_Ctrl = 0x0B,
    FIFO_Config     = 0x0C,
    MAC_Config_Ctrl = 0x0D,
    PLS_Config_Ctrl = 0x0E,
    PHY_Config_Ctrl = 0x0F,
    Chip_ID_Lo      = 0x10,
    Chip_ID_Hi      = 0x11,
    Addr_Config     = 0x12,
    Log_Addr_Flt    = 0x14,
    Phys_Addr       = 0x15,
    Missed_Pkt_Cnt  = 0x18,
    Runt_Pkt_Cnt    = 0x1A, // not used in Macintosh?
    Rcv_Collis_Cnt  = 0x1B, // not used in Macintosh?
    User_Test       = 0x1D,
    Rsrvd_Test_1    = 0x1E, // not used in Macintosh?
    Rsrvd_Test_2    = 0x1F, // not used in Macintosh?
};

}; // namespace MaceEnet

class MaceController {
public:
    MaceController(uint16_t id) { this->chip_id = id; };
    ~MaceController() = default;

    // MACE registers access
    uint8_t read(uint8_t reg_offset);
    void    write(uint8_t reg_offset, uint8_t value);

private:
    uint16_t    chip_id; // per-instance MACE Chip ID
};

#endif // MACE_H
