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
#include "interruptctrl.h"
#include <cinttypes>
#include <string>
#include <loguru.hpp>

using namespace std;

InterruptCtrl::InterruptCtrl() {
}

void InterruptCtrl::update_mask_flags(uint32_t bit_setting) {
    int_mask1 = bit_setting;
}

void InterruptCtrl::update_events_flags(uint32_t bit_setting) {
    int_events1 = bit_setting;
}

void InterruptCtrl::update_levels_flags(uint32_t bit_setting) {
    int_levels1 = bit_setting;
}

void InterruptCtrl::update_clear_flags(uint32_t bit_setting) {
    int_clear1 = bit_setting;
    int_events1 &= ~int_clear1;
}

uint32_t InterruptCtrl::retrieve_mask_flags() {
    return int_mask1;
}

uint32_t InterruptCtrl::retrieve_events_flags() {
    return int_events1;
}

uint32_t InterruptCtrl::retrieve_levels_flags() {
    return int_levels1;
}

uint32_t InterruptCtrl::retrieve_clear_flags() {
    return int_clear1;
}

uint32_t InterruptCtrl::ack_interrupt(uint32_t device_bits){
    bool confirm_interrupt = false;

    int_events1 |= device_bits;
    if (int_events1 & int_mask1)
        confirm_interrupt = true;

    if (confirm_interrupt)
        call_ppc_handler();

    return 0;
}

void InterruptCtrl::call_ppc_handler() {
    ppc_exception_handler(Except_Type::EXC_EXT_INT, 0x0);
}