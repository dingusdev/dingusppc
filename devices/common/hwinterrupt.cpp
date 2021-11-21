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
#include "hwinterrupt.h"
#include <cinttypes>
#include <string>
#include <loguru.hpp>

using namespace std;

InterruptManager::InterruptManager(std::string sys_type) {
    if (sys_type.compare("Heathrow") == 0) {
        scsi0_dma_mask_bit    = (1 << 0);
        scca_dma_out_mask_bit = (1 << 4);
        scca_dma_in_mask_bit  = (1 << 5);
        sccb_dma_out_mask_bit = (1 << 6);
        sccb_dma_in_mask_bit  = (1 << 7);
        dav_dma_out_mask_bit  = (1 << 8);
        dav_dma_in_mask_bit   = (1 << 9);
        scsi0_mask_bit        = (1 << 12);
        scca_mask_bit         = (1 << 15);
        sccb_mask_bit         = (1 << 16);
        davbus_mask_bit       = (1 << 17);
        cuda_mask_bit         = (1 << 18);
        nmi_mask_bit          = (1 << 20);
    } 
    else if (sys_type.compare("AMIC") == 0) {
        LOG_F(WARNING, "Missing interrupt handling for %s!", sys_type.c_str());
    } 
    else {
        LOG_F(ERROR, "Cannot determine interrupt layout for %s!", sys_type.c_str());
    }
}


uint32_t InterruptManager::ack_interrupts(){

}

uint32_t InterruptManager::clear_interrupts(){

}

void InterruptManager::call_ppc_handler(uint32_t device_bits) {
    ppc_exception_handler(Except_Type::EXC_EXT_INT, device_bits);
}