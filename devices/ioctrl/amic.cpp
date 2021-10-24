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

#include <cpu/ppc/ppcmmu.h>
#include <devices/common/viacuda.h>
#include <devices/ethernet/mace.h>
#include <devices/ioctrl/amic.h>
#include <machines/machinebase.h>
#include <devices/memctrl/memctrlbase.h>

#include <algorithm>
#include <cinttypes>
#include <loguru.hpp>
#include <memory>

AMIC::AMIC()
{
    this->name = "Apple Memory-mapped I/O Controller";

    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
                           (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));

    /* add memory mapped I/O region for the AMIC control registers */
    if (!mem_ctrl->add_mmio_region(0x50F00000, 0x00040000, this)) {
        LOG_F(ERROR, "Couldn't register AMIC registers!");
    }

    this->mace    = std::unique_ptr<MaceController> (new MaceController(MACE_ID));
    this->viacuda = std::unique_ptr<ViaCuda> (new ViaCuda());

    this->snd_out_dma = std::unique_ptr<AmicSndOutDma> (new AmicSndOutDma());
    this->awacs       = std::unique_ptr<AwacDevicePdm> (new AwacDevicePdm());
    this->awacs->set_dma_out(this->snd_out_dma.get());
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
    uint32_t  phase_val;

    // subdevices registers
    switch(offset >> 12) {
    case 0: // VIA1 registers
    case 1:
        return this->viacuda->read(offset >> 9);
    case 4: // SCC registers
        LOG_F(WARNING, "AMIC: read attempt from unimplemented SCC register");
        return 0;
    case 0xA: // MACE registers
        return this->mace->read((offset >> 4) & 0xFF);
    case 0x10: // SCSI registers
        LOG_F(WARNING, "AMIC: read attempt from unimplemented SCSI register");
        return 0;
    case 0x14: // Sound registers
        switch (offset) {
        case AMICReg::Snd_Stat_0:
        case AMICReg::Snd_Stat_1:
        case AMICReg::Snd_Stat_2:
            return (this->awacs->read_stat() >> (offset &  3 * 8)) & 0xFF;
        case AMICReg::Snd_Phase0:
        case AMICReg::Snd_Phase1:
        case AMICReg::Snd_Phase2:
            // the sound phase register is organized as follows:
            // 000000oo oooooooo oopppppp where 'o' is the 12-bit offset
            // into the DMA buffer and 'p' is an undocumented prescale value
            // HWInit doesn't care about. Let's hope it will be sufficient
            // to return 0 for prescale.
            phase_val = this->snd_out_dma->get_cur_buf_pos() << 6;
            return (phase_val >> ((2 - (offset & 3)) * 8)) & 0xFF;
        case AMICReg::Snd_Out_Ctrl:
            return this->snd_out_ctrl;
        case AMICReg::Snd_Out_DMA:
            return this->snd_out_dma->read_stat();
        }
    }

    switch(offset) {
    case AMICReg::Diag_Reg:
        return 0xFFU; // this value allows the machine to boot normally
    case AMICReg::SCSI_DMA_Ctrl:
        return this->scsi_dma_cs;
    default:
        LOG_F(WARNING, "Unknown AMIC register read, offset=%x", offset);
    }
    return 0;
}

void AMIC::write(uint32_t reg_start, uint32_t offset, uint32_t value, int size)
{
    uint32_t mask;

    // subdevices registers
    switch(offset >> 12) {
    case 0: // VIA1 registers
    case 1:
        this->viacuda->write(offset >> 9, value);
        return;
    case 0xA: // MACE registers
        this->mace->write((offset >> 4) & 0xFF, value);
        return;
    case 0x14: // Sound registers
        switch(offset) {
        case AMICReg::Snd_Ctrl_0:
        case AMICReg::Snd_Ctrl_1:
        case AMICReg::Snd_Ctrl_2:
            // remember values of sound control registers
            this->imm_snd_regs[offset & 3] = value;
            // transfer control information to the sound codec when ready
            if ((this->imm_snd_regs[0] & 0xC0) == PDM_SND_CTRL_VALID) {
                this->awacs->write_ctrl(
                    (this->imm_snd_regs[1] >> 4) | (this->imm_snd_regs[0] & 0x3F),
                    ((this->imm_snd_regs[1] & 0xF) << 8) | this->imm_snd_regs[2]
                );
            }
            return;
        case AMICReg::Snd_Buf_Size_Hi:
        case AMICReg::Snd_Buf_Size_Lo:
            mask = 0xFF00U >> (8 * (offset & 1));
            this->snd_buf_size = (this->snd_buf_size & ~mask) |
                                ((value & 0xFF) << (8 * ((offset & 1) ^1)));
            this->snd_buf_size &= ~3; // sound buffer size is always a multiple of 4
            LOG_F(9, "AMIC: Sound buffer size set to 0x%X", this->snd_buf_size);
            return;
        case AMICReg::Snd_Out_Ctrl:
            LOG_F(9, "AMIC Sound Out Ctrl updated, val=%x", value);
            if ((value & 1) != (this->snd_out_ctrl & 1)) {
                if (value & 1) {
                    LOG_F(9, "AMIC Sound Out DMA enabled!");
                    this->snd_out_dma->init(this->dma_base & ~0x3FFFF,
                                            this->snd_buf_size);
                    this->snd_out_dma->enable();
                    this->awacs->set_sample_rate((this->snd_out_ctrl >> 1) & 3);
                    this->awacs->start_output_dma();
                } else {
                    LOG_F(9, "AMIC Sound Out DMA disabled!");
                    this->snd_out_dma->disable();
                }
            }
            this->snd_out_ctrl = value;
            return;
        case AMICReg::Snd_In_Ctrl:
            LOG_F(INFO, "AMIC Sound In Ctrl updated, val=%x", value);
            return;
        case AMICReg::Snd_Out_DMA:
            this->snd_out_dma->write_dma_out_ctrl(value);
            return;
        }
    }

    switch(offset) {
    case AMICReg::VIA2_Slot_IER:
        LOG_F(INFO, "AMIC VIA2 Slot Interrupt Enable Register updated, val=%x", value);
        break;
    case AMICReg::VIA2_IER:
        LOG_F(INFO, "AMIC VIA2 Interrupt Enable Register updated, val=%x", value);
        break;
    case AMICReg::Video_Mode_Reg:
        LOG_F(INFO, "AMIC Video Mode Register set to %x", value);
        break;
    case AMICReg::Int_Ctrl:
        LOG_F(INFO, "AMIC Interrupt Control Register set to %X", value);
        break;
    case AMICReg::DMA_Base_Addr_0:
    case AMICReg::DMA_Base_Addr_1:
    case AMICReg::DMA_Base_Addr_2:
    case AMICReg::DMA_Base_Addr_3:
        mask = 0xFF000000UL >> (8 * (offset & 3));
        this->dma_base = (this->dma_base & ~mask) |
                        ((value & 0xFF) << (8 * (3 - (offset & 3))));
        LOG_F(9, "AMIC: DMA base address set to 0x%X", this->dma_base);
        break;
    case AMICReg::Enet_DMA_Xmt_Ctrl:
        LOG_F(INFO, "AMIC Ethernet Transmit DMA Ctrl updated, val=%x", value);
        break;
    case AMICReg::SCSI_DMA_Ctrl:
        LOG_F(INFO, "AMIC SCSI DMA Ctrl updated, val=%x", value);
        this->scsi_dma_cs = value;
        break;
    case AMICReg::Enet_DMA_Rcv_Ctrl:
        LOG_F(INFO, "AMIC Ethernet Receive DMA Ctrl updated, val=%x", value);
        break;
    case AMICReg::SWIM3_DMA_Ctrl:
        LOG_F(INFO, "AMIC SWIM3 DMA Ctrl updated, val=%x", value);
        break;
    case AMICReg::SCC_DMA_Xmt_A_Ctrl:
        LOG_F(INFO, "AMIC SCC Transmit Ch A DMA Ctrl updated, val=%x", value);
        break;
    case AMICReg::SCC_DMA_Rcv_A_Ctrl:
        LOG_F(INFO, "AMIC SCC Receive Ch A DMA Ctrl updated, val=%x", value);
        break;
    case AMICReg::SCC_DMA_Xmt_B_Ctrl:
        LOG_F(INFO, "AMIC SCC Transmit Ch B DMA Ctrl updated, val=%x", value);
        break;
    case AMICReg::SCC_DMA_Rcv_B_Ctrl:
        LOG_F(INFO, "AMIC SCC Receive Ch B DMA Ctrl updated, val=%x", value);
        break;
    default:
        LOG_F(WARNING, "Unknown AMIC register write, offset=%x, val=%x",
                offset, value);
    }
}

// =========================== DMA related stuff =============================
AmicSndOutDma::AmicSndOutDma()
{
    this->dma_out_ctrl = 0;
    this->enabled = false;
}

bool AmicSndOutDma::is_active()
{
    return true;
}

void AmicSndOutDma::init(uint32_t buf_base, uint32_t buf_samples)
{
    this->out_buf0 = buf_base + AMIC_SND_BUF0_OFFS;
    this->out_buf1 = buf_base + AMIC_SND_BUF1_OFFS;

    this->out_buf_len = buf_samples * 2 * 2;

    this->snd_buf_num = 0;
    this->cur_buf_pos = 0;
}

uint8_t AmicSndOutDma::read_stat()
{
    return this->dma_out_ctrl;
}

void AmicSndOutDma::write_dma_out_ctrl(uint8_t value)
{
    // clear interrupt flags
    value &= ~PDM_DMA_INTS_MASK;
    this->dma_out_ctrl = value;
    LOG_F(9, "AMIC: Sound out DMA control set to 0x%X", value);
}

DmaPullResult AmicSndOutDma::pull_data(uint32_t req_len, uint32_t *avail_len,
                                       uint8_t **p_data)
{
    *avail_len = 0;

    int rem_len = this->out_buf_len - this->cur_buf_pos;
    if (rem_len <= 0) {
        if (!this->snd_buf_num) {
            // signal buffer 0 drained
            this->dma_out_ctrl |= PDM_DMA_IF0;
            // TODO: generate IE0 interrupt if enabled
        } else {
            // signal buffer 1 drained
            this->dma_out_ctrl |= PDM_DMA_IF1;
            // TODO: generate IE1 interrupt if enabled
        }

        // check DMA enable flag after buffer 1 was processed
        // if it's false stop delivering sound data
        // this will effectively stop audio playback
        if (this->snd_buf_num && !this->enabled) {
            this->cur_buf_pos = 0;
            return DmaPullResult::NoMoreData;
        }

        this->cur_buf_pos = 0;  // reset buffer position
        this->snd_buf_num ^= 1; // toggle sound buffers
        rem_len = this->out_buf_len; // buffer size = full buffer
    }

    uint32_t len = std::min((uint32_t)rem_len, req_len);

    *p_data = mmu_get_dma_mem(
        (this->snd_buf_num ? this->out_buf1 : this->out_buf0) + this->cur_buf_pos,
        len);
    this->cur_buf_pos += len;
    *avail_len = len;
    return DmaPullResult::MoreData;
}
