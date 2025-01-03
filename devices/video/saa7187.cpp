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

/** @file SAA7187 Digital video encoder emulation. */

#include <devices/video/saa7187.h>
#include <loguru.hpp>

Saa7187VideoEncoder::Saa7187VideoEncoder(uint8_t dev_addr)
{
    supports_types(HWCompType::I2C_DEV);

    this->my_addr = dev_addr;
}

void Saa7187VideoEncoder::start_transaction()
{
    LOG_F(INFO, "Saa7187: start_transaction");
    this->pos = 0; // reset read/write position
}

bool Saa7187VideoEncoder::send_subaddress(uint8_t sub_addr)
{
    LOG_F(ERROR, "Saa7187: send_subaddress 0x%02x", sub_addr);
    //this->pos = sub_addr;
    return true;
}

bool Saa7187VideoEncoder::send_byte(uint8_t data)
{
    if (this->pos == 0) {
        this->reg_num = data;
        LOG_F(INFO, "Saa7187: write 0x%02x", data);
    }
    else {
        if (this->reg_num < Saa7187Regs::last_reg) {
            this->regs[this->reg_num] = data;
            LOG_F(INFO, "Saa7187: write 0x%02x = 0x%02x", this->reg_num, data);
        }
        else {
            LOG_F(ERROR, "Saa7187: write 0x%02x = 0x%02x", this->reg_num, data);
        }
        this->reg_num++;
    }
    this->pos++;
    return true;
}

bool Saa7187VideoEncoder::receive_byte(uint8_t* p_data)
{
    LOG_F(ERROR, "Saa7187: receive_byte");
    *p_data = 0x00;
    return true;
}
