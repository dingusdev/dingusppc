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

/** Apple memory-mapped I/O controller emulation.

    Author: Max Poliakovski
*/

#include "amic.h"
#include "machines/machinebase.h"
#include "memctrlbase.h"
#include "viacuda.h"
#include <cinttypes>
#include <loguru.hpp>

AMIC::AMIC()
{
    this->name = "Apple Memory-mapped I/O Controller";

    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
                           (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));

    /* add memory mapped I/O region for the AMIC control registers */
    if (!mem_ctrl->add_mmio_region(0x50F00000, 0x00040000, this)) {
        LOG_F(ERROR, "Couldn't register AMIC registers!");
    }

    this->viacuda = std::unique_ptr<ViaCuda> (new ViaCuda());
}

bool AMIC::supports_type(HWCompType type) {
    if (type == HWCompType::MMIO_DEV) {
        return true;
    } else {
        return false;
    }
}

uint32_t AMIC::read(uint32_t reg_start, uint32_t offset, int size)
{
    if (offset < 0x2000) {
        return this->viacuda->read(offset >> 9);
    }

    LOG_F(INFO, "AMIC read!");
    return 0;
}

void AMIC::write(uint32_t reg_start, uint32_t offset, uint32_t value, int size)
{
    if (offset < 0x2000) {
        this->viacuda->write(offset >> 9, value);
        return;
    }

    switch(offset) {
    case AMICReg::Snd_Out_Cntl:
        LOG_F(INFO, "AMIC Sound Out Ctrl updated, val=%x", value);
        break;
    case AMICReg::Snd_In_Cntl:
        LOG_F(INFO, "AMIC Sound In Ctrl updated, val=%x", value);
        break;
    case AMICReg::VIA2_Slot_IER:
        LOG_F(INFO, "AMIC VIA2 Slot Interrupt Enable Register updated, val=%x", value);
        break;
    case AMICReg::VIA2_IER:
        LOG_F(INFO, "AMIC VIA2 Interrupt Enable Register updated, val=%x", value);
        break;
    case AMICReg::Video_Mode_Reg:
        LOG_F(INFO, "AMIC Video Mode Register set to %x", value);
        break;
    case AMICReg::Int_Cntl:
        LOG_F(INFO, "AMIC Interrupt Control Register set to %X", value);
        break;
    case AMICReg::Enet_DMA_Xmt_Cntl:
        LOG_F(INFO, "AMIC Ethernet Transmit DMA Ctrl updated, val=%x", value);
        break;
    case AMICReg::SCSI_DMA_Cntl:
        LOG_F(INFO, "AMIC SCSI DMA Ctrl updated, val=%x", value);
        break;
    case AMICReg::Enet_DMA_Rcv_Cntl:
        LOG_F(INFO, "AMIC Ethernet Receive DMA Ctrl updated, val=%x", value);
        break;
    case AMICReg::SWIM3_DMA_Cntl:
        LOG_F(INFO, "AMIC SWIM3 DMA Ctrl updated, val=%x", value);
        break;
    case AMICReg::SCC_DMA_Xmt_A_Cntl:
        LOG_F(INFO, "AMIC SCC Transmit Ch A DMA Ctrl updated, val=%x", value);
        break;
    case AMICReg::SCC_DMA_Rcv_A_Cntl:
        LOG_F(INFO, "AMIC SCC Receive Ch A DMA Ctrl updated, val=%x", value);
        break;
    case AMICReg::SCC_DMA_Xmt_B_Cntl:
        LOG_F(INFO, "AMIC SCC Transmit Ch B DMA Ctrl updated, val=%x", value);
        break;
    case AMICReg::SCC_DMA_Rcv_B_Cntl:
        LOG_F(INFO, "AMIC SCC Receive Ch B DMA Ctrl updated, val=%x", value);
        break;
    default:
        LOG_F(WARNING, "Unknown AMIC register write, offset=%x, val=%x",
                offset, value);
    }
}
