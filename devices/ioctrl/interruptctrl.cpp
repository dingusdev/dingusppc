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

#include <cpu/ppc/ppcemu.h>
#include <devices/ioctrl/interruptctrl.h>
#include <cinttypes>
#include <string>
#include <loguru.hpp>

using namespace std;

InterruptCtrl::InterruptCtrl() {
}

void InterruptCtrl::update_reg(REG_ID retrieve_reg, uint32_t bit_setting) {
    switch (retrieve_reg) { 
        case mask1:
            int_mask1 = bit_setting;
            break;
        case clear1:
            int_clear1 = bit_setting;
            int_events1 &= ~int_clear1;
            break;
        case events1:
            int_events1 = bit_setting;
            break;
        case levels1:
            int_levels1 = bit_setting;
            break;
        case mask2:
            int_mask2 = bit_setting;
            break;
        case clear2:
            int_clear2 = bit_setting;
            int_events2 &= ~int_clear2;
            break;
        case events2:
            int_events2 = bit_setting;
            break;
        case levels2:
            int_levels2 = bit_setting;
            break;
    }
}

uint32_t InterruptCtrl::retrieve_reg(REG_ID retrieve_reg) {
    switch (retrieve_reg) {
    case mask1:
        return int_mask1;
        break;
    case clear1:
        return int_clear1;
        break;
    case events1:
        return int_events1;
        break;
    case levels1:
        return int_levels1;
        break;
    case mask2:
        return int_mask2;
        break;
    case clear2:
        return int_clear2;
        break;
    case events2:
        return int_events2;
        break;
    case levels2:
        return int_levels2;
        break;
    }
}

uint32_t InterruptCtrl::ack_interrupt(uint32_t device_bits) {
    bool confirm_interrupt = false;

    int_events1 |= device_bits;
    if (int_events1 & int_mask1)
        confirm_interrupt = true;

    if (confirm_interrupt)
        ack_cpu_interrupt();

    return 0;
}

void InterruptCtrl::ack_cpu_interrupt() {
    ppc_exception_handler(Except_Type::EXC_EXT_INT, 0x0);
}