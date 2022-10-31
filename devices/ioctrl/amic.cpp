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

/** Apple memory-mapped I/O controller emulation.

    Author: Max Poliakovski
*/

#include <core/timermanager.h>
#include <cpu/ppc/ppcemu.h>
#include <cpu/ppc/ppcmmu.h>
#include <devices/deviceregistry.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/scsi/sc53c94.h>
#include <devices/common/viacuda.h>
#include <devices/ethernet/mace.h>
#include <devices/floppy/swim3.h>
#include <devices/ioctrl/amic.h>
#include <devices/serial/escc.h>
#include <machines/machinebase.h>
#include <devices/memctrl/memctrlbase.h>
#include <devices/video/displayid.h>
#include <devices/video/pdmonboard.h>

#include <algorithm>
#include <cinttypes>
#include <loguru.hpp>
#include <memory>

AMIC::AMIC() : MMIODevice()
{
    this->name = "Apple Memory-mapped I/O Controller";

    supports_types(HWCompType::MMIO_DEV | HWCompType::INT_CTRL);

    // connect internal SCSI controller
    this->scsi = dynamic_cast<Sc53C94*>(gMachineObj->get_comp_by_name("Sc53C94"));

    // connect serial HW
    this->escc = dynamic_cast<EsccController*>(gMachineObj->get_comp_by_name("Escc"));

    // connect Ethernet HW
    this->mace = dynamic_cast<MaceController*>(gMachineObj->get_comp_by_name("Mace"));

    // connect Cuda
    this->viacuda = dynamic_cast<ViaCuda*>(gMachineObj->get_comp_by_name("ViaCuda"));

    // initialize sound HW
    this->snd_out_dma = std::unique_ptr<AmicSndOutDma> (new AmicSndOutDma());
    this->awacs       = std::unique_ptr<AwacDevicePdm> (new AwacDevicePdm());
    this->awacs->set_dma_out(this->snd_out_dma.get());

    // initialize on-board video
    this->disp_id = std::unique_ptr<DisplayID> (new DisplayID());
    this->def_vid = std::unique_ptr<PdmOnboardVideo> (new PdmOnboardVideo());

    // initialize floppy disk HW
    this->swim3 = dynamic_cast<Swim3::Swim3Ctrl*>(gMachineObj->get_comp_by_name("Swim3"));
    this->floppy_dma = std::unique_ptr<AmicFloppyDma> (new AmicFloppyDma());
    this->swim3->set_dma_channel(this->floppy_dma.get());
}

int AMIC::device_postinit()
{
    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
                           (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));

    // add memory mapped I/O region for the AMIC control registers
    if (!mem_ctrl->add_mmio_region(0x50F00000, 0x00040000, this)) {
        LOG_F(ERROR, "Couldn't register AMIC registers!");
    }

    // AMIC drives the VIA CA1 internally to generate 60.15 Hz interrupts
    this->pseudo_vbl_tid = TimerManager::get_instance()->add_cyclic_timer(
        static_cast<uint64_t>((1.0f/60.15) * NS_PER_SEC + 0.5f),
        [this]() {
            this->viacuda->assert_ctrl_line(ViaLine::CA1);
        });

    // set EMMO pin status (active low)
    this->emmo_pin = GET_BIN_PROP("emmo") ^ 1;

    return 0;
}

uint32_t AMIC::read(uint32_t rgn_start, uint32_t offset, int size)
{
    uint32_t  phase_val;

    // subdevices registers
    switch(offset >> 12) {
    case 0: // VIA1 registers
    case 1:
        return this->viacuda->read(offset >> 9);
    case 4: // SCC registers
        return this->escc->read(compat_to_macrisc[(offset >> 1) & 0xF]);
    case 0xA: // MACE registers
        return this->mace->read((offset >> 4) & 0x1F);
    case 0x10: // SCSI registers
        return this->scsi->read((offset >> 4) & 0xF);
    case 0x14: // Sound registers
        switch (offset) {
        case AMICReg::Snd_Stat_0:
        case AMICReg::Snd_Stat_1:
        case AMICReg::Snd_Stat_2:
            return (this->awacs->read_stat() >> (offset & 3) * 8) & 0xFF;
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
        break;
    case 0x16: // SWIM3 registers
    case 0x17:
        return this->swim3->read((offset >> 9) & 0xF);
    }

    switch(offset) {
    case AMICReg::Ariel_Config:
        return this->def_vid->get_vdac_config();
    case AMICReg::VIA2_IFR:
    case AMICReg::VIA2_IFR_RBV:
        return this->via2_ifr;
    case AMICReg::VIA2_IER:
    case AMICReg::VIA2_IER_RBV:
        return this->via2_ier;
    case AMICReg::Video_Mode:
        return this->def_vid->get_video_mode();
    case AMICReg::Monitor_Id:
        return this->mon_id;
    case AMICReg::Int_Ctrl:
        return (this->int_ctrl & 0xC0) | (this->dev_irq_lines & 0x3F);
    case AMICReg::Diag_Reg:
        return 0xFE | this->emmo_pin;
    case AMICReg::DMA_Base_Addr_0:
    case AMICReg::DMA_Base_Addr_1:
    case AMICReg::DMA_Base_Addr_2:
    case AMICReg::DMA_Base_Addr_3:
        return (this->dma_base >> (3 - (offset & 3)) * 8) & 0xFF;
    case AMICReg::SCSI_DMA_Ctrl:
        return this->scsi_dma_cs;
    case AMICReg::Floppy_Addr_Ptr_0:
    case AMICReg::Floppy_Addr_Ptr_1:
    case AMICReg::Floppy_Addr_Ptr_2:
    case AMICReg::Floppy_Addr_Ptr_3:
        return (this->floppy_addr_ptr >> (3 - (offset & 3)) * 8) & 0xFF;
    case AMICReg::Floppy_DMA_Ctrl:
        return this->floppy_dma->read_stat();
    default:
        LOG_F(WARNING, "Unknown AMIC register read, offset=%x", offset);
    }
    return 0;
}

void AMIC::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    uint32_t mask;

    // subdevices registers
    switch(offset >> 12) {
    case 0: // VIA1 registers
    case 1:
        this->viacuda->write(offset >> 9, value);
        return;
    case 4:
        this->escc->write(compat_to_macrisc[(offset >> 1) & 0xF], value);
        return;
    case 0xA: // MACE registers
        this->mace->write((offset >> 4) & 0x1F, value);
        return;
    case 0x10:
        this->scsi->write((offset >> 4) & 0xF, value);
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
            SET_SIZE_BYTE(this->snd_buf_size, offset, value);
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
    case 0x16: // SWIM3 registers
    case 0x17:
        this->swim3->write((offset >> 9) & 0xF, value);
        return;
    }

    switch(offset) {
    case AMICReg::VIA2_IFR:
        // for each "1" in value clear the corresponding IRQ bit
        // TODO: is bit 7 read only?
        // TODO: update interrupts?
        this->via2_ifr &= ~(value & 0x7F);
        break;
    case AMICReg::VIA2_Slot_IER:
        LOG_F(INFO, "AMIC VIA2 Slot Interrupt Enable Register updated, val=%x", value);
        break;
    case AMICReg::VIA2_IER:
        if (value & 0x80) {
            this->via2_ier |= value & 0x7F;
        } else {
            this->via2_ier &= ~value;
        }
        break;
    case AMICReg::Ariel_Clut_Index:
        this->def_vid->set_clut_index(value);
        break;
    case AMICReg::Ariel_Clut_Color:
        this->def_vid->set_clut_color(value);
        break;
    case AMICReg::Ariel_Config:
        this->def_vid->set_vdac_config(value);
        break;
    case AMICReg::Video_Mode:
        this->def_vid->set_video_mode(value);
        break;
    case AMICReg::Pixel_Depth:
        this->def_vid->set_pixel_depth(value);
        break;
    case AMICReg::Monitor_Id: {
            // extract and convert pin directions (0 - input, 1 - output)
            uint8_t dirs = ~value & 7;
            if (!dirs && !(value & 8)) {
                LOG_F(INFO, "AMIC: Monitor sense lines tristated");
            }
            // propagate bit 3 to all pins configured as output
            // set levels of all intput pins to "1"
            uint8_t levels = (7 ^ dirs) | (((value & 8) ? 7 : 0) & dirs);
            // read monitor sense lines and store the result in the bits 4-6
            this->mon_id = (this->mon_id & 0xF) |
                (this->disp_id->read_monitor_sense(levels, dirs) << 4);
        }
        break;
    case AMICReg::Int_Ctrl:
        // reset CPU interrupt bit if requested
        if (value & AMIC_INT_CLR) {
            if (this->int_ctrl & 0x80) {
                this->int_ctrl &= ~AMIC_INT_CLR;
                ppc_release_int();
                LOG_F(5, "AMIC: CPU INT latch cleared");
            }
        }
        // keep interrupt mode bit
        // and discard read-only IQR state bits
        this->int_ctrl |= value & AMIC_INT_MODE;
        break;
    case AMICReg::DMA_Base_Addr_0:
    case AMICReg::DMA_Base_Addr_1:
    case AMICReg::DMA_Base_Addr_2:
    case AMICReg::DMA_Base_Addr_3:
        SET_ADDR_BYTE(this->dma_base, offset, value);
        this->dma_base &= 0xFFFC0000UL;
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
    case AMICReg::Floppy_Addr_Ptr_2:
    case AMICReg::Floppy_Addr_Ptr_3:
        SET_ADDR_BYTE(this->floppy_addr_ptr, offset, value);
        break;
    case AMICReg::Floppy_Byte_Cnt_Hi:
    case AMICReg::Floppy_Byte_Cnt_Lo:
        SET_SIZE_BYTE(this->floppy_byte_cnt, offset, value);
        break;
    case AMICReg::Floppy_DMA_Ctrl:
        if (value & 1) { // RST bit set?
            this->floppy_addr_ptr = this->dma_base + 0x15000;
            this->floppy_dma->reset(this->floppy_addr_ptr);
        }
        if (value & 2) { // RUN bit set?
            this->floppy_dma->reinit(this->floppy_addr_ptr, this->floppy_byte_cnt);
        }
        this->floppy_dma->write_ctrl(value);
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

// ======================== Interrupt related stuff ==========================
uint32_t AMIC::register_dev_int(IntSrc src_id) {
    switch (src_id) {
    case IntSrc::VIA_CUDA:
        return 1;
    case IntSrc::SCSI1:
        return 0x800;
    case IntSrc::SWIM3:
        return 0x2000;
    default:
        ABORT_F("AMIC: unknown interrupt source %d", src_id);
    }
    return 0;
}

uint32_t AMIC::register_dma_int(IntSrc src_id) {
    ABORT_F("AMIC: register_dma_int() not implemented");
    return 0;
}

void AMIC::ack_int(uint32_t irq_id, uint8_t irq_line_state) {
    // dispatch AMIC interrupts from various sources
    if (irq_id < 0x100) {
        this->ack_cpu_int(irq_id, irq_line_state);
    } else if (irq_id < 0x10000) {
        this->ack_via2_int(irq_id >> 8, irq_line_state);
    } else {
        ABORT_F("AMIC: unknown interrupt source ID 0x%X", irq_id);
    }
}

void AMIC::ack_via2_int(uint32_t irq_id, uint8_t irq_line_state) {
    if (irq_line_state) {
        this->via2_ifr |= irq_id;
    } else {
        this->via2_ifr &= ~irq_id;
    }
    uint8_t new_irq = !!(this->via2_ifr & this->via2_ier & 0x7F);
    this->via2_ifr  = (this->via2_ifr & 0x7F) | (new_irq << 7);
    if (new_irq != this->via2_irq) {
        this->via2_irq = new_irq;
        this->ack_cpu_int(2, new_irq);
    }
}

void AMIC::ack_cpu_int(uint32_t irq_id, uint8_t irq_line_state) {
    if (this->int_ctrl & AMIC_INT_MODE) { // 68k interrupt emulation mode?
        if (irq_line_state) {
            this->dev_irq_lines |= irq_id;
        } else {
            this->dev_irq_lines &= ~irq_id;
        }
        if (!(this->int_ctrl & 0x80)) {
            this->int_ctrl |= 0x80; // set CPU interrupt bit
            ppc_assert_int();
            LOG_F(5, "AMIC: CPU INT asserted, source: %d", irq_id);
        } else {
            LOG_F(5, "AMIC: CPU INT already latched");
        }

    } else {
        ABORT_F("AMIC: interrupt mode 0 not implemented");
    }
}

void AMIC::ack_dma_int(uint32_t irq_id, uint8_t irq_line_state) {
    ABORT_F("AMIC: ack_dma_int() not implemented");
}

// ============================ Sound DMA stuff ================================
AmicSndOutDma::AmicSndOutDma()
{
    this->dma_out_ctrl = 0;
    this->enabled = false;
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

// ============================ Floppy DMA stuff ===============================
void AmicFloppyDma::reset(const uint32_t addr_ptr)
{
    this->stat &= 0x48; // clear interrupt flag, RUN and RST bits
    this->addr_ptr   = addr_ptr;
    this->byte_count = 0;
}

void AmicFloppyDma::reinit(const uint32_t addr_ptr, const uint16_t byte_cnt)
{
    this->addr_ptr   = addr_ptr;
    this->byte_count = byte_cnt;
}

void AmicFloppyDma::write_ctrl(uint8_t value)
{
    // copy over DIR, IE and RUN bits
    this->stat = (this->stat & 0x81) | (value & 0x4A);

    // clear interrupt flag if requested
    if (value & 0x80) {
        this->stat &= 0x7F;
    }
}

int AmicFloppyDma::push_data(const char* src_ptr, int len)
{
    len = std::min((int)this->byte_count, len);

    uint8_t *p_data = mmu_get_dma_mem(this->addr_ptr, len);
    std::memcpy(p_data, src_ptr, len);

    this->addr_ptr += len;
    this->byte_count -= len;
    if (!this->byte_count) {
        LOG_F(WARNING, "AMIC: DMA interrupts not implemented yet");
    }

    return 0;
}

DmaPullResult AmicFloppyDma::pull_data(uint32_t req_len, uint32_t *avail_len,
                                       uint8_t **p_data)
{
    return DmaPullResult::NoMoreData;
}

static vector<string> Amic_Subdevices = {
    "Sc53C94", "Escc", "Mace", "ViaCuda", "Swim3"
};

static const DeviceDescription Amic_Descriptor = {
    AMIC::create, Amic_Subdevices, {}
};

REGISTER_DEVICE(Amic, Amic_Descriptor);
