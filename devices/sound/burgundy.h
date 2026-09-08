/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

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

/** Burgundy sound codec definitions. */

#ifndef BURGUNDY_H
#define BURGUNDY_H

#include <devices/common/hwcomponent.h>
#include <devices/sound/awacs.h>

#include <cinttypes>
#include <string>

/** Number of internal registers implemented in Burgundy. */
constexpr auto BURGUNDY_NUM_REGS = 123;

namespace SOUND_CONTROL {    // 0x00
enum {
    INSUBFRAME_MASK     = 0x0000000F,
    INSUBFRAME_POS      = 0,
    INSUBFRAME0         = 0x1,
    INSUBFRAME1         = 0x2,
    INSUBFRAME2         = 0x4,
    INSUBFRAME3         = 0x8,
    OUTSUBFRAME_MASK    = 0x000000F0,
    OUTSUBFRAME_POS     = 4,
    OUTSUBFRAME0        = 0x1,
    OUTSUBFRAME1        = 0x2,
    OUTSUBFRAME2        = 0x4,
    OUTSUBFRAME3        = 0x8,
    RATE_MASK           = 0x00000700,
    RATE_POS            = 8,
    RATE_44100          = 0x0,
    ERROR               = 0x00000800,
    PORTCHANGE          = 0x00001000,
    ERRORINT            = 0x00002000,
    STATUSSUBFRAME_MASK = 0x00018000,
    STATUSSUBFRAME_POS  = 15,
    STATUSSUBFRAME0     = 0x0,
    STATUSSUBFRAME1     = 0x1,
    STATUSSUBFRAME2     = 0x2,
    STATUSSUBFRAME3     = 0x3,
};
}

namespace CODEC_CONTROL {    // 0x10
enum {
    DATA_MASK        = 0x000000FF,
    DATA_POS         = 0,
    CURRENTBYTE_MASK = 0x00000300,
    CURRENTBYTE_POS  = 8,
    LASTBYTE_MASK    = 0x00000C00,
    LASTBYTE_POS     = 10,
    ADDR_MASK        = 0x000FF000,
    ADDR_POS         = 12,
    RESET            = 0x00100000,    // should be set when current byte is 0
    WRITE            = 0x00200000,    // READ = 0
    BUSY             = 0x01000000,    // set when writing to CODEC_CONTROL, clear when done
};
}

namespace CODEC_STATUS {    // 0x20
enum {
    SENSE_MASK             = 0x0000000F,
    SENSE_POS              = 0,
    SENSE_MIC              = 0x2,
    SENSE_HEADPHONES       = 0x4,
    SENSE_HEADPHONES2      = 0x8,    // 1 = line level mic (default) 0 = powered mic
    DATA_MASK              = 0x00000FF0,
    DATA_POS               = 4,
    CURRENTBYTE_MASK       = 0x00003000,
    CURRENTBYTE_POS        = 12,
    BYTECOUNTER_MASK       = 0x0000C000,
    BYTECOUNTER_POS        = 14,
    INDICATOR_MASK         = 0x000F0000,
    INDICATOR_POS          = 16,
    INDICATOR_TONECONTROL  = 0x1,
    INDICATOR_OVERFLOW0    = 0x2,
    INDICATOR_OVERFLOW1    = 0x3,
    INDICATOR_OVERFLOW2    = 0x4,
    INDICATOR_INPUTLINECHG = 0x6,
    INDICATOR_THRESHOLD0   = 0xB,
    INDICATOR_THRESHOLD1   = 0xC,
    INDICATOR_THRESHOLD2   = 0xD,
    INDICATOR_THRESHOLD3   = 0xE,
    INDICATOR_TWILIGHTCMP  = 0xF,
    READY                  = 0x00400000,
    FIRST_VALID_BYTE       = 0x00800000,    // wait for set, then wait for clear before reading data
};
}

class BurgundyCodec : public MacioSndCodec {
public:
    BurgundyCodec(std::string name);
    ~BurgundyCodec() = default;

    uint32_t    snd_ctrl_read(uint32_t offset, int size);
    void        snd_ctrl_write(uint32_t offset, uint32_t value, int size);

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<BurgundyCodec>(new BurgundyCodec("Burgundy"));
    }

private:
    uint32_t    snd_ctrl_reg    = 0x1011;
    uint32_t    last_ctrl_data  = 0;
    uint8_t     byte_counter    = 0;
    uint8_t     reg_addr        = 0;
    bool        first_valid     = false;
    uint8_t     read_pos        = 0;
    uint8_t     data_byte       = 0;
    uint32_t    frame_count     = 0;
    uint64_t    frame_count_start_time = 0;

    uint32_t    reg_array[BURGUNDY_NUM_REGS] = {};
};

#endif // BURGUNDY_H
