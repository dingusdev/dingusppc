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

/** Burgundy sound codec definitions. */

#ifndef BURGUNDY_H
#define BURGUNDY_H

#include <devices/common/hwcomponent.h>
#include <devices/sound/awacs.h>

#include <cinttypes>
#include <string>

enum {
    BURGUNDY_REG_WR = 1 << 21,
    BURGUNDY_READY  = 1 << 22,
};

/** Number of internal registers implemented in Burgundy. */
#define BURGUNDY_NUM_REGS   123

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
    uint32_t    last_ctrl_data  = 0;
    uint8_t     byte_counter    = 0;
    uint8_t     reg_addr        = 0;
    uint8_t     first_valid     = 0;
    uint8_t     read_pos        = 0;
    uint8_t     data_byte       = 0;

    uint32_t    reg_array[BURGUNDY_NUM_REGS] = {};
};

#endif // BURGUNDY_H
