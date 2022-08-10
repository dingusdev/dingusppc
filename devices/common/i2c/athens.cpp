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

/** @file Athens clock generator emulation. */

/**
    Athens (Apple part# 343S1191) is a programmable clock generator ASIC
    used in the PCI Power Macintosh computers.
    It has two outputs: system clock and video aka dot clock. Both can be
    programmed using HW jumpers or I2C interface.
    This high-level emulator focuses on the Athen's I2C interface providing
    methods for querying the resulting virtual frequences.
    This is helpful especially for calculating video parameters.
 */

#include <devices/common/i2c/athens.h>
#include <loguru.hpp>

#include <algorithm>
#include <cinttypes>
#include <vector>

AthensClocks::AthensClocks(uint8_t dev_addr)
{
    supports_types(HWCompType::I2C_DEV);

    this->my_addr = dev_addr;

    // set up power on values
    this->regs[AthensRegs::P2_MUX2] = 0x62;
}

void AthensClocks::start_transaction()
{
    this->pos = 0; // reset read/write position
}

bool AthensClocks::send_subaddress(uint8_t sub_addr)
{
    LOG_F(INFO, "Athens: subaddress set to 0x%X", sub_addr);
    return true;
}

bool AthensClocks::send_byte(uint8_t data)
{
    switch (this->pos) {
    case 0:
        this->reg_num = data;
        this->pos++;
        break;
    case 1:
        if (this->reg_num >= ATHENS_NUM_REGS) {
            LOG_F(WARNING, "Athens: invalid register number %d", this->reg_num);
            return false; // return NACK
        }
        this->regs[this->reg_num] = data;
        if (reg_num == 3) {
            LOG_F(INFO, "Athens: dot clock frequency set to %d Hz", get_dot_freq());
        }
        break;
    default:
        LOG_F(WARNING, "Athens: too much data received!");
        return false; // return NACK
    }
    return true;
}

bool AthensClocks::receive_byte(uint8_t* p_data)
{
    *p_data = 0x41; // return my ID (value has been guessed)
    return true;
}

int AthensClocks::get_sys_freq()
{
    return 0;
}

int AthensClocks::get_dot_freq()
{
    static std::vector<int> D2_commons = {7, 11, 13, 14, 15, 17, 23, 31};
    static std::vector<int> N2_commons = {
        22, 27, 28, 31, 35, 37, 38, 42, 49, 55, 56, 78,  125
    };

    if (this->regs[AthensRegs::P2_MUX2] & 0xC0) {
        LOG_F(INFO, "Athens: dot clock disabled");
        return 0;
    }

    float out_freq = 0.0f;

    int d2 = this->regs[AthensRegs::D2];
    int n2 = this->regs[AthensRegs::N2];

    int post_div = 1 << (3 - (this->regs[AthensRegs::P2_MUX2] & 3));

    if (std::find(D2_commons.begin(), D2_commons.end(), d2) == D2_commons.end()) {
        LOG_F(WARNING, "Athens: untested D2 value %d", d2);
    }

    if (std::find(N2_commons.begin(), N2_commons.end(), n2) == N2_commons.end()) {
        LOG_F(WARNING, "Athens: untested N2 value %d", d2);
    }

    int mux = (this->regs[AthensRegs::P2_MUX2] >> 4) & 3;

    switch (mux) {
    case 0: // clock source -> dot cock VCO
        out_freq = ATHENS_XTAL * ((float)n2 / (float)(d2 * post_div));
        break;
    case 1: // clock source -> system clock VCO
        LOG_F(WARNING, "Athens: system clock VCO not supported yet!");
        break;
    case 2: // clock source -> crystal frequency
        out_freq = ATHENS_XTAL / post_div;
        break;
    case 3:
        LOG_F(WARNING, "Athens: attempt to use reserved Mux value!");
        break;
    }

    return static_cast<int>(out_freq + 0.5f);
}
