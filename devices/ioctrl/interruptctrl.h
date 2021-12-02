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

enum DEV_ID {
    SCSI_DMA       = 0,
    Swim3_DMA      = 1,
    IDE0_DMA       = 2,
    IDE1_DMA       = 3,
    SCC_A_DMA_OUT  = 4,
    SCC_A_DMA_IN   = 5,
    SCC_B_DMA_OUT  = 6,
    SCC_B_DMA_IN   = 7,
    DAVBUS_DMA_OUT = 8,
    DAVBUS_DMA_IN  = 9,
    VIDEO_OUT      = 10,
    VIDEO_IN       = 11,
    SCSI0          = 12,
    IDE0           = 13,
    IDE1           = 14,
    SCC_A          = 15,
    SCC_B          = 16,
    DAVBUS         = 17,
    VIA_CUDA       = 18,
    SWIM3          = 19,
    NMI            = 20,
    PERCH_DMA      = 21,
    PERCH          = 22,
    BMAC_DMA_OUT   = 0,
    BMAC_DMA_IN    = 1,
    BMAC           = 10,
};

enum REG_ID { 
    mask1 = 0,
    clear1 = 1,
    events1 = 2,
    levels1 = 3,
    mask2   = 4,
    clear2  = 5,
    events2 = 6,
    levels2 = 7,
};

class InterruptCtrl {
public:
    InterruptCtrl();
    ~InterruptCtrl() = default;

    uint32_t ack_interrupt(uint32_t device_bits);

    virtual uint32_t register_device(DEV_ID dev_id){
        return 0;
    };

    void update_reg(REG_ID retrieve_reg, uint32_t bit_setting);
    uint32_t retrieve_reg(REG_ID retrieve_reg);

    void ack_cpu_interrupt();

protected:
    uint32_t int_events1 = 0;
    uint32_t int_mask1   = 0;
    uint32_t int_clear1  = 0;
    uint32_t int_levels1 = 0;
    uint32_t int_events2 = 0;
    uint32_t int_mask2   = 0;
    uint32_t int_clear2  = 0;
    uint32_t int_levels2 = 0;
};

#endif /* HWINTERRUPT_H */