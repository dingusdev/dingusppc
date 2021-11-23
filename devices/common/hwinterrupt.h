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

#include <cinttypes>
#include <memory>
#include <string>

class HWInterrupt {
public:
    HWInterrupt(std::string sys_type = "None");
    ~HWInterrupt() = default;

    uint32_t ack_interrupts();

    //for all (Old World) PCI-based Macs
    uint32_t update_int1_flags(uint32_t bit_setting);
    uint32_t clear_int1_flags(uint32_t bit_setting);

    //for Heathrow
    uint32_t update_int2_flags(uint32_t bit_setting);
    uint32_t clear_int2_flags(uint32_t bit_setting);

    uint32_t pending_interrupts(uint32_t device_bits); 
    void call_ppc_handler(uint32_t device_bits);

private:
    uint32_t pending_int1_flags;
    uint32_t pending_int2_flags;
    uint32_t scsi0_dma_mask_bit    = 0;
    uint32_t scca_dma_out_mask_bit = 0;
    uint32_t scca_dma_in_mask_bit  = 0;
    uint32_t sccb_dma_out_mask_bit = 0;
    uint32_t sccb_dma_in_mask_bit  = 0;
    uint32_t dav_dma_out_mask_bit  = 0;
    uint32_t dav_dma_in_mask_bit   = 0;
    uint32_t scsi0_mask_bit        = 0;
    uint32_t scca_mask_bit         = 0;
    uint32_t sccb_mask_bit         = 0;
    uint32_t davbus_mask_bit       = 0;
    uint32_t cuda_mask_bit         = 0;
    uint32_t nmi_mask_bit          = 0;
};

#endif /* HWINTERRUPT_H */