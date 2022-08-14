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

#ifndef I2C_PROM_H
#define I2C_PROM_H

#include <devices/common/hwcomponent.h>
#include <devices/common/i2c/i2c.h>

#include <cinttypes>
#include <memory>

class I2CProm : public I2CDevice, public HWComponent
{
public:
    I2CProm(uint8_t dev_addr, int size);
    ~I2CProm() = default;

    // I2CDevice methods
    void start_transaction();
    bool send_subaddress(uint8_t sub_addr);
    bool send_byte(uint8_t data);
    bool receive_byte(uint8_t* p_data);

    // data management methods
    void fill_memory(int start, int size, uint8_t c);
    void set_memory(int start, const uint8_t* in_data, int size);

private:
    std::unique_ptr<uint8_t[]>  data;

    int     rom_size    = 0;
    int     pos         = 0;
    uint8_t my_addr     = 0xA0;
};

#endif // I2C_PROM_H
