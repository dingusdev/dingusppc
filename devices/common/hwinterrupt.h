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

#ifndef HW_INTERRUPT_H
#define HW_INTERRUPT_H

#include <cinttypes>

/** Enumerator for various interrupt sources. */
enum IntSrc : int {
    VIA_CUDA = 1,
    SCSI1    = 2,
    SWIM3    = 3,
    SCC      = 4,
    ETHERNET = 5,
    NMI      = 6,
};

/** Base class for interrupt controllers. */
class InterruptCtrl {
public:
    InterruptCtrl() = default;
    virtual ~InterruptCtrl() = default;

    // register interrupt sources for a device
    virtual uint32_t register_dev_int(IntSrc src_id) = 0;
    virtual uint32_t register_dma_int(IntSrc src_id) = 0;

    // acknowledge HW interrupt
    virtual void ack_int(uint32_t irq_id, uint8_t irq_line_state)     = 0;
    virtual void ack_dma_int(uint32_t irq_id, uint8_t irq_line_state) = 0;
};

#endif // HW_INTERRUPT_H
