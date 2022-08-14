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

/** @file Generic PROM device programmable over I2C. */

#include <devices/common/i2c/i2cprom.h>
#include <loguru.hpp>

#include <cinttypes>
#include <cstring>
#include <memory>

I2CProm::I2CProm(uint8_t dev_addr, int size)
{
    supports_types(HWCompType::I2C_DEV);

    this->my_addr  = dev_addr;
    this->rom_size = size;

    // allocate storage for ROM data
    this->data = std::unique_ptr<uint8_t[]> (new uint8_t[this->rom_size]);
}

void I2CProm::fill_memory(int start, int size, uint8_t c)
{
    if ((start + size) <= this->rom_size) {
        std::memset(&this->data[start], c, size);
    }
}

void I2CProm::set_memory(int start, const uint8_t* in_data, int size)
{
    if ((start + size) <= this->rom_size) {
        std::memcpy(&this->data[start], in_data, size);
    }
}

void I2CProm::start_transaction() {
    this->pos = 0;
};

bool I2CProm::send_subaddress(uint8_t sub_addr) {
    this->pos = sub_addr;
    return true;
};

bool I2CProm::send_byte(uint8_t data) {
    LOG_F(9, "I2CRom: 0x%X received", data);
    return true;
};

bool I2CProm::receive_byte(uint8_t* p_data) {
    if (this->pos >= this->rom_size) {
        this->pos = 0; // attempt to read past last byte should wrap around
    }
    *p_data = this->data[this->pos++];
    return true;
};
