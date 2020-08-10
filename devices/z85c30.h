/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-20 divingkatae and maximum
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

/** @file Descriptor-based direct memory access emulation.

    Official documentation can be found in the fifth chapter of the book
    "Macintosh Technology in the Common Hardware Reference Platform"
    by Apple Computer, Inc.
 */

#ifndef Z85C30_H
#define Z85C30_H

#include <cinttypes>

class Z85C30 {
public:
    Z85C30();
    ~Z85C30();

    uint8_t read(uint8_t reg);
    void write(uint8_t reg, uint8_t value);

    private:
    uint8_t read_registers[16]  = {0x00};
    uint8_t write_registers[16] = {0x00};
    uint8_t wr7_alt             = 0x20;
    bool enable_wr7_alt         = false;
};

#endif /* Z85C30_H */