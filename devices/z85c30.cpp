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

#include "devices/z85c30.h"
#include <cinttypes>
#include <thirdparty/loguru/loguru.hpp>


Z85C30::Z85C30() {
    write_registers[0x04] = 0x04;
    write_registers[0x09] = 0xC0;
    write_registers[0x0B] = 0x08;
    write_registers[0x0E] = 0x30;
    write_registers[0x0F] = 0xF8;

    read_registers[0x00]  = 0x44;
    read_registers[0x01]  = 0x06;
}

Z85C30::~Z85C30() {

}

uint8_t Z85C30::read(uint8_t reg) {
    LOG_F(INFO, "Reading from register %d: 0x%x \n", reg, this->write_registers[reg]);
    return this->write_registers[reg];
}

void Z85C30::write(uint8_t reg, uint8_t value) {
    LOG_F(INFO, "Writing value 0x%x to register %d \n", value, reg);

    if (reg == 0x07) {
        if (this->enable_wr7_alt) {
            this->wr7_alt = value;
        }
        else {
            this->write_registers[0x07] = value;
        }
    } 
    else {
        this->write_registers[reg] = value;
    }

    if (reg == 0x0F) {
        if (this->write_registers[0x0F] & 0x1) {
            this->enable_wr7_alt = true;
        }
        else{
            this->enable_wr7_alt = false;
        }
    }
}