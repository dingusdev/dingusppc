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

/** @file Athens clock generator definitions. */

#ifndef ATHENS_H
#define ATHENS_H

#include <devices/common/hwcomponent.h>
#include <devices/common/i2c/i2c.h>

#include <cinttypes>

#define ATHENS_NUM_REGS 8

constexpr auto ATHENS_XTAL = 31334400.0f; // external crystal oscillator frequency

namespace AthensRegs {

enum AthensRegs: uint8_t {
    ID      = 0,
    D2      = 1,
    N2      = 2,
    P2_MUX2 = 3,
    VN_CTRL = 4, // vendor specific control bits
    BD1     = 5,
    BN1     = 6,
    P1      = 7
};

}; // namespace AthensRegs

class AthensClocks : public I2CDevice, public HWComponent
{
public:
    AthensClocks(uint8_t dev_addr);
    ~AthensClocks() = default;

    // I2CDevice methods
    void start_transaction();
    bool send_subaddress(uint8_t sub_addr);
    bool send_byte(uint8_t data);
    bool receive_byte(uint8_t* p_data);

    // methods for querying virtual frequences
    int get_sys_freq();
    int get_dot_freq();

private:
    uint8_t     my_addr = 0;
    uint8_t     reg_num = 0;
    int         pos = 0;

    uint8_t     regs[ATHENS_NUM_REGS];
};

#endif // ATHENS_H
