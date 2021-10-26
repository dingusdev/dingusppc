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

/** @file NCR 53C94 SCSI controller definitions. */

/* NOTE: Power Macintosh computers don't have a real NCR 53C94 chip.
   The corresponding functionality is provided by the 53C94 compatible
   cell in the custom Curio IC (Am79C950).
*/

#ifndef NCR53C94_H
#define NCR53C94_H

#include <cinttypes>

/** NCR 53C94 read registers */
namespace Read {
    enum Reg53C94 : uint8_t {
        Xfer_Cnt_LSB = 0,
        Xfer_Cnt_MSB = 1,
        FIFO         = 2,
        Command      = 3,
        Status       = 4,
        Interrupt    = 5,
        Seq_Step     = 6,
        FIFO_Flags   = 7,
        Config_1     = 8,
        Config_2     = 0xB,
        Config_3     = 0xC,
        Config_4     = 0xD, // Curio extension?
        Xfer_Cnt_Hi  = 0xE, // Curio extension?
        DMA          = 0xF, // Curio extension?
    };
};

/** NCR 53C94 write registers */
namespace Write {
    enum Reg53C94 : uint8_t {
        Xfer_Cnt_LSB = 0,
        Xfer_Cnt_MSB = 1,
        FIFO         = 2,
        Command      = 3,
        Dest_Bus_ID  = 4,
        Sel_Timeout  = 5,
        Synch_Period = 6,
        Synch_Offset = 7,
        Config_1     = 8,
        Clk_Conv_Fac = 9,
        Test_Mode    = 0xA,
        Config_2     = 0xB,
        Config_3     = 0xC,
        Config_4     = 0xD, // Curio extension?
        Chip_ID      = 0xE, // Curio extension?
        DMA          = 0xF, // Curio extension?
    };
};

class Ncr53C94 {
public:
    Ncr53C94()  = default;
    ~Ncr53C94() = default;

    // NCR 53C94 registers access
    uint8_t read(uint8_t reg_offset);
    void   write(uint8_t reg_offset, uint8_t value);

private:
    uint8_t chip_id;
};

#endif // NCR53C94_H
