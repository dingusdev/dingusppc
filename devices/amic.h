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

/** @file Apple memory-mapped I/O controller emulation. */

#ifndef AMIC_H
#define AMIC_H

#include "mmiodevice.h"
#include "viacuda.h"
#include <cinttypes>
#include <memory>

/* AMIC DMA registers offsets from AMIC base (0x50F00000). */
enum AMICReg : uint32_t {
    // Sound control registers
    Snd_Out_Cntl        = 0x14010,
    Snd_In_Cntl         = 0x14011,

    // VIA2 registers
    VIA2_Slot_IER       = 0x26012,
    VIA2_IER            = 0x26013,

    // Video control registers
    Video_Mode_Reg      = 0x28000,

    Int_Cntl            = 0x2A000,

    // DMA control registers
    Enet_DMA_Xmt_Cntl   = 0x31C20,
    SCSI_DMA_Cntl       = 0x32008,
    Enet_DMA_Rcv_Cntl   = 0x32028,
    SWIM3_DMA_Cntl      = 0x32068,
    SCC_DMA_Xmt_A_Cntl  = 0x32088,
    SCC_DMA_Rcv_A_Cntl  = 0x32098,
    SCC_DMA_Xmt_B_Cntl  = 0x320A8,
    SCC_DMA_Rcv_B_Cntl  = 0x320B8,
};

class AMIC : public MMIODevice {
public:
    AMIC();
    ~AMIC() = default;

    bool supports_type(HWCompType type);

    /* MMIODevice methods */
    uint32_t read(uint32_t reg_start, uint32_t offset, int size);
    void write(uint32_t reg_start, uint32_t offset, uint32_t value, int size);

protected:
    void dma_reg_write(uint32_t offset, uint32_t value, int size);

private:
    std::unique_ptr<ViaCuda> viacuda;
};

#endif // AMIC_H
