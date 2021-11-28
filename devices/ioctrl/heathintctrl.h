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

#ifndef HEATHINTCTRL_H
#define HEATHINTCTRL_H

#include <devices/ioctrl/interruptctrl.h>
#include <cinttypes>
#include <memory>

enum Heathrow_Int1 {
    SCSI_DMA       = (1 << 0),
    Swim3_DMA      = (1 << 1),
    IDE0_DMA       = (1 << 2),
    IDE1_DMA       = (1 << 3),
    SCC_A_DMA_OUT  = (1 << 4),
    SCC_A_DMA_IN   = (1 << 5),
    SCC_B_DMA_OUT  = (1 << 6),
    SCC_B_DMA_IN   = (1 << 7),
    DAVBUS_DMA_OUT = (1 << 8),
    DAVBUS_DMA_IN  = (1 << 9),
    SCSI0          = (1 << 12),
    IDE0           = (1 << 13),
    IDE1           = (1 << 14),
    SCC_A          = (1 << 15),
    SCC_B          = (1 << 16),
    DAVBUS         = (1 << 17),
    VIA_CUDA       = (1 << 18),
    SWIM3          = (1 << 19),
    NMI            = (1 << 20),
    PERCH_DMA      = (1 << 21),
    PERCH          = (1 << 26),
};

enum Heathrow_Int2 {
    BMAC_DMA_OUT = (1 << 32),
    BMAC_DMA_IN  = (1 << 33),
    BMAC         = (1 << 42),
};

class HeathIntCtrl : public InterruptCtrl {
    public:
    HeathIntCtrl(){};
    ~HeathIntCtrl() = default;

    void update_mask2_flags(uint32_t bit_setting) {
        int_mask2 = bit_setting;
    }

    void update_events2_flags(uint32_t bit_setting) {
        int_events2 = bit_setting;
    }

    void update_levels2_flags(uint32_t bit_setting) {
        int_levels2 = bit_setting;
    }

    void update_clear2_flags(uint32_t bit_setting) {
        int_clear2 = bit_setting;
        int_events2 &= ~int_clear2;
    }

    uint32_t retrieve_mask2_flags() {
        return int_mask2;
    }

    uint32_t retrieve_events2_flags() {
        return int_events2;
    }

    uint32_t retrieve_levels2_flags() {
        return int_levels2;
    }

    uint32_t retrieve_clear2_flags() {
        return int_clear2;
    }

    uint32_t ack_interrupt(uint32_t device_bits, bool is_int2) {
        bool confirm_interrupt = false;

        if (is_int2) {
            int_events2 |= device_bits;
            uint32_t mask2_check = retrieve_mask2_flags();
            if (mask2_check && int_events2)
                confirm_interrupt = true;
        } else {
            //slightly nasty workaround
            uint32_t mask1_check = retrieve_mask_flags() + device_bits;
            update_mask_flags(mask1_check + device_bits);
            uint32_t events1_check = retrieve_events_flags();
            if (mask1_check && events1_check)
                confirm_interrupt = true;
        }

        if (confirm_interrupt)
            call_ppc_handler();

        return 0;
    }

    private:
    uint32_t int_events2 = 0;
    uint32_t int_mask2   = 0;
    uint32_t int_clear2  = 0;
    uint32_t int_levels2 = 0;
};

#endif /* HEATHINTCTRL_H */