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

#ifndef HWINTERRUPT_H
#define HWINTERRUPT_H

#include <devices/common/hwcomponent.h>
#include <cinttypes>
#include <memory>

class InterruptCtrl : public HWComponent {
public:
    InterruptCtrl();
    ~InterruptCtrl() = default;

    bool supports_type(HWCompType type) {
        return (type == HWCompType::INT_CTRL);
    };

    uint32_t ack_interrupt(uint32_t device_bits);

    //for all (Old World) PCI-based Macs (is_int is only for Heathrow)
    void update_mask_flags(uint32_t bit_setting);
    void update_events_flags(uint32_t bit_setting);
    void update_levels_flags(uint32_t bit_setting);
    void update_clear_flags(uint32_t bit_setting);
    uint32_t retrieve_mask_flags();
    uint32_t retrieve_events_flags();
    uint32_t retrieve_levels_flags();
    uint32_t retrieve_clear_flags();

    //for Heathrow

    void call_ppc_handler();

private:
    uint32_t int_events1 = 0;
    uint32_t int_mask1   = 0;
    uint32_t int_clear1  = 0;
    uint32_t int_levels1 = 0;
};

#endif /* HWINTERRUPT_H */