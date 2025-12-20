/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-25 divingkatae and maximum
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
    this->set_name("Apple Memory-mapped I/O Controller");

    supports_types(HWCompType::MMIO_DEV | HWCompType::INT_CTRL);

    // connect internal SCSI controller
    this->scsi = dynamic_cast<Sc53C94*>(gMachineObj->get_comp_by_name("Sc53C94"));
    this->curio_dma = std::unique_ptr<AmicScsiDma> (new AmicScsiDma());
    this->curio_dma->connect(this->scsi);
    this->scsi->connect(this->curio_dma.get());
    this->scsi->set_drq_callback([this](const uint8_t drq_state) {
        if (drq_state & 1)
            via2_ifr |= VIA2_INT_SCSI_DRQ;
        else
            via2_ifr &= ~VIA2_INT_SCSI_DRQ;
        this->update_via2_irq();
    });

    // connect serial HW
    this->escc = dynamic_cast<EsccController*>(gMachineObj->get_comp_by_name("Escc"));
    this->escc_xmit_b_dma = std::unique_ptr<AmicSerialXmitDma>(new AmicSerialXmitDma("EsccBXmit"));
    this->escc_xmit_a_dma = std::unique_ptr<AmicSerialXmitDma>(new AmicSerialXmitDma("EsccAXmit"));

    // connect Ethernet HW
    this->mace = dynamic_cast<MaceController*>(gMachineObj->get_comp_by_name("Mace"));

    // connect Cuda
    this->viacuda = dynamic_cast<ViaCuda*>(gMachineObj->get_comp_by_name("ViaCuda"));

    // initialize sound HW
    this->snd_out_dma = std::unique_ptr<AmicSndOutDma>(new AmicSndOutDma());
    this->snd_out_dma->init_interrupts(this, DMA1_INT_SOUND << DMA1_INT_SHIFT);
    this->snd_in_dma  = std::unique_ptr<AmicSndInDma>(new AmicSndInDma());
    this->snd_in_dma->init_interrupts(this, DMA0_INT_SOUND << DMA0_INT_SHIFT);
    this->awacs       = std::unique_ptr<AwacDevicePdm> (new AwacDevicePdm());
    this->awacs->set_dma_out(this->snd_out_dma.get());
    this->awacs->set_dma_in(this->snd_in_dma.get());

    // initialize on-board video
    this->disp_id = std::unique_ptr<DisplayID> (new DisplayID());
    this->def_vid = std::unique_ptr<PdmOnboardVideo> (new PdmOnboardVideo());
    this->def_vid->init_interrupts(this, SLOT_INT_VBL << 16);

    // initialize floppy disk HW
    this->swim3 = dynamic_cast<Swim3::Swim3Ctrl*>(gMachineObj->get_comp_by_name("Swim3"));
    this->floppy_dma = std::unique_ptr<AmicFloppyDma> (new AmicFloppyDma());
    this->swim3->set_dma_channel(this->floppy_dma.get());
}

AMIC::~AMIC()
{
    if (this->pseudo_vbl_tid) {
        TimerManager::get_instance()->cancel_timer(this->pseudo_vbl_tid);
        this->pseudo_vbl_tid = 0;
    }
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
    // subdevices registers
    switch(offset >> 12) {
    case 0: // VIA1 registers
    case 1:
        return this->viacuda->read(offset >> 9);
    case 4: // SCC registers
        if ((offset & 0xF) < 0x0C)
            return this->escc->read(compat_to_macrisc[(offset >> 1) & 0xF]);
        else
            LOG_F(ERROR, "AMIC SCC read  @%x.%c", offset, SIZE_ARG(size));
        return 0;
    case 0x8:
    case 0x9:
        LOG_F(WARNING, "AMIC Ethernet ID Rom read  @%x.%c", offset, SIZE_ARG(size));
        return 0;
    case 0xA: // MACE registers
        return this->mace->read((offset >> 4) & 0x1F);
    case 0x10: // SCSI registers
        if (offset & 0x100) {
            return this->scsi->pseudo_dma_read();
        } else {
            return this->scsi->read((offset >> 4) & 0xF);
        }
    case 0x14: // Sound registers
        switch (offset) {
        case AMICReg::Snd_Ctrl_0:
        case AMICReg::Snd_Ctrl_1:
        case AMICReg::Snd_Ctrl_2:
            return this->imm_snd_regs[offset & 3];
        case AMICReg::Snd_Stat_0:
        case AMICReg::Snd_Stat_1:
        case AMICReg::Snd_Stat_2:
            return (this->awacs->read_stat() >> (offset & 3) * 8) & 0xFF;
        case AMICReg::Snd_Phase0:
        case AMICReg::Snd_Phase1:
        case AMICReg::Snd_Phase2: {
            // the sound phase register is organized as follows:
            // 000000oo oooooooo oopppppp where 'o' is the 12-bit offset
            // into the DMA buffer and 'p' is an undocumented prescale value
            // HWInit doesn't care about. Let's hope it will be sufficient
            // to return 0 for prescale.
            uint32_t  phase_val = this->snd_out_dma->get_cur_buf_pos() << 6;
            return (phase_val >> ((2 - (offset & 3)) * 8)) & 0xFF;
        }
        case AMICReg::Snd_Out_Ctrl:
            return this->snd_out_ctrl;
        case AMICReg::Snd_In_Ctrl:
            return this->snd_in_ctrl;
        case AMICReg::Snd_Out_DMA:
            return this->snd_out_dma->read_stat();
        case AMICReg::Snd_In_DMA:
            return this->snd_in_dma->read_stat();
        }
        break;
    case 0x16: // SWIM3 registers
    case 0x17:
        return this->swim3->read((offset >> 9) & 0xF);
    }

    switch(offset) {
    case AMICReg::Ariel_Config:
        return this->def_vid->get_vdac_config();
    case AMICReg::VIA2_Slot_IFR:
        return this->via2_slot_ifr;
    case AMICReg::VIA2_IFR:
    case AMICReg::VIA2_IFR_RBV:
        return this->via2_ifr;
    case AMICReg::VIA2_Slot_IER:
        return this->via2_slot_ier;
    case AMICReg::VIA2_IER:
    case AMICReg::VIA2_IER_RBV:
        return this->via2_ier;
    case AMICReg::Video_Mode:
        return this->def_vid->get_video_mode();
    case AMICReg::Monitor_Id:
        return this->mon_id;
    case AMICReg::Int_Ctrl:
        return (this->int_ctrl & 0xC0) | (this->dev_irq_lines & 0x3F);
    case AMICReg::DMA_IFR_0:
        return this->dma_ifr0;
    case AMICReg::DMA_IFR_1:
        return this->dma_ifr1;
    case AMICReg::Diag_Reg:
        return 0xFE | this->emmo_pin;
    case AMICReg::DMA_Base_Addr_0:
    case AMICReg::DMA_Base_Addr_1:
    case AMICReg::DMA_Base_Addr_2:
    case AMICReg::DMA_Base_Addr_3:
        return (this->dma_base >> (3 - (offset & 3)) * 8) & 0xFF;
    case AMICReg::SCSI_DMA_Ctrl:
        return this->curio_dma->read_stat();
    case AMICReg::Floppy_Addr_Ptr_0:
    case AMICReg::Floppy_Addr_Ptr_1:
    case AMICReg::Floppy_Addr_Ptr_2:
    case AMICReg::Floppy_Addr_Ptr_3:
        return (this->floppy_addr_ptr >> (3 - (offset & 3)) * 8) & 0xFF;
    case AMICReg::Floppy_DMA_Ctrl:
        return this->floppy_dma->read_stat();
    case SCC_DMA_Xmt_A_Ctrl:
        return this->escc_xmit_a_dma->read_stat();
    case SCC_RXA_Byte_Cnt_Hi:
        return 0;
    case SCC_RXA_Byte_Cnt_Lo:
        return 0;
    case SCC_DMA_Rcv_A_Ctrl: {
        uint32_t value = std::rand() & (-1U >> (32 - size * 8));
        LOG_F(WARNING, "AMIC SCC Receive Ch A DMA Ctrl read  @%x.%c = %0*x", offset,
            SIZE_ARG(size), size * 2, value);
        return value;
    }
    case SCC_DMA_Xmt_B_Ctrl:
        return this->escc_xmit_b_dma->read_stat();
    case SCC_RXB_Byte_Cnt_Hi:
        return 0;
    case SCC_RXB_Byte_Cnt_Lo:
        return 0;
    case SCC_DMA_Rcv_B_Ctrl: {
        uint32_t value = std::rand() & (-1U >> (32 - size * 8));
        LOG_F(WARNING, "AMIC SCC Receive Ch B DMA Ctrl read  @%x.%c = %0*x",
            offset, SIZE_ARG(size), size * 2, value);
        return value;
    }
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
    case 4: // SCC registers
        if ((offset & 0xF) < 0x0C)
            this->escc->write(compat_to_macrisc[(offset >> 1) & 0xF], value);
        else
            LOG_F(ERROR, "AMIC SCC write @%x.%c = %0*x",
                offset, SIZE_ARG(size), size * 2, value);
        return;
    case 0xA: // MACE registers
        this->mace->write((offset >> 4) & 0x1F, value);
        return;
    case 0x10:
        if (offset & 0x100)
            this->scsi->pseudo_dma_write(value);
        else
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
            if ((value & 1) != (this->snd_out_ctrl & 1)) {
                if (value & 1) {
                    this->snd_out_dma->init(this->dma_base & ~0x3FFFF,
                                            this->snd_buf_size);
                    this->snd_out_dma->enable();
                    this->awacs->set_sample_rate((this->snd_out_ctrl >> 1) & 3);
                    this->awacs->dma_out_start();
                } else {
                    this->snd_out_dma->disable();
                    this->awacs->dma_out_pause();
                }
            }
            this->snd_out_ctrl = value;
            return;
        case AMICReg::Snd_In_Ctrl:
            if (this->snd_in_ctrl & 0xFC) {
                    this->snd_in_dma->init(this->dma_base & ~0x3FFFF, this->snd_buf_size);
                    this->snd_in_dma->enable();
                    this->awacs->set_sample_rate((this->snd_in_ctrl >> 1) & 3);
                    this->awacs->dma_in_start();
                } 
            else {
                    this->snd_in_dma->disable();
                    this->awacs->dma_in_pause();
            }
            this->snd_in_ctrl = value;
            return;
        case AMICReg::Snd_Out_DMA:
            this->snd_out_dma->write_dma_out_ctrl(value);
            return;
        case AMICReg::Snd_In_DMA:
            this->snd_in_dma->write_dma_in_ctrl(value);
            return;
        }
    case 0x16: // SWIM3 registers
    case 0x17:
        this->swim3->write((offset >> 9) & 0xF, value);
        return;
    }

    switch(offset) {
    case AMICReg::VIA2_Slot_IFR:
        if (value & SLOT_INT_VBL) {
            // clear pending VBL int
            this->ack_slot_int(SLOT_INT_VBL, 0);
        }
        break;
    case AMICReg::VIA2_IFR:
        // if bit 7 is set, clear the corresponding IRQ bit for each "1" in value
        // writing any value to VIA2_IFR with bit 7 cleared has no effect
        if (value & 0x80) {
            this->via2_ifr &= ~(value & 0x7F);
            this->update_via2_irq();
        }
        break;
    case AMICReg::VIA2_Slot_IER:
        if (value & 0x80)
            this->via2_slot_ier |= value & 0x7F;
        else
            this->via2_slot_ier &= ~value;
        break;
    case AMICReg::VIA2_IER:
    case AMICReg::VIA2_IER_RBV:
        if (value & 0x80) {
            this->via2_ier |= value & 0x7F;
        } else {
            this->via2_ier &= ~value;
        }
        this->update_via2_irq();
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
        if (value & CPU_INT_CLEAR) {
            if (this->int_ctrl & CPU_INT_FLAG) {
                this->int_ctrl &= ~CPU_INT_FLAG;
                ppc_release_int();
                LOG_F(5, "AMIC: CPU INT latch cleared");
            }
        }
        // keep interrupt mode bit
        // and discard read-only IQR state bits
        this->int_ctrl |= value & CPU_INT_MODE;
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
    case AMICReg::SCSI_DMA_Base_0:
    case AMICReg::SCSI_DMA_Base_1:
    case AMICReg::SCSI_DMA_Base_2:
    case AMICReg::SCSI_DMA_Base_3:
        SET_ADDR_BYTE(this->scsi_dma_base, offset, value);
        this->scsi_dma_base &= 0xFFFFFFF8UL;
        LOG_F(9, "AMIC: SCSI DMA base address set to 0x%X", this->scsi_dma_base);
        break;
    case AMICReg::SCSI_DMA_Ctrl:
        if (value & 1) { // RST bit set?
            this->scsi_addr_ptr = this->scsi_dma_base;
            this->curio_dma->reset(this->scsi_addr_ptr);
        }
        if (value & 2) { // RUN bit set?
            this->curio_dma->reinit(this->scsi_dma_base);
            if (value & (1 << 6))
                this->curio_dma->xfer_to_device();
            else
                this->curio_dma->xfer_from_device();
        }
        this->curio_dma->write_ctrl(value);
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
        this->escc_xmit_a_dma->write_ctrl(value);
        break;
    case AMICReg::SCC_DMA_Rcv_A_Ctrl:
        LOG_F(INFO, "AMIC SCC Receive Ch A DMA Ctrl updated, val=%x", value);
        break;
    case AMICReg::SCC_DMA_Xmt_B_Ctrl:
        LOG_F(INFO, "AMIC SCC Transmit Ch B DMA Ctrl updated, val=%x", value);
        this->escc_xmit_b_dma->write_ctrl(value);
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
uint64_t AMIC::register_dev_int(IntSrc src_id) {
    switch (src_id) {
    case IntSrc::VIA_CUDA      : return CPU_INT_VIA1      << CPU_INT_SHIFT;
    case IntSrc::VIA2          : return CPU_INT_VIA2      << CPU_INT_SHIFT;
    case IntSrc::ESCC          : return CPU_INT_ESCC      << CPU_INT_SHIFT;
    case IntSrc::ETHERNET      : return CPU_INT_ENET      << CPU_INT_SHIFT;
    case IntSrc::DMA_ALL       : return CPU_INT_ALL_DMA   << CPU_INT_SHIFT;
    case IntSrc::NMI           : return CPU_INT_NMI       << CPU_INT_SHIFT;

    case IntSrc::DMA_SCSI_CURIO: return VIA2_INT_SCSI_DRQ << VIA2_INT_SHIFT;
    case IntSrc::SLOT_ALL      : return VIA2_INT_ALL_SLOT << VIA2_INT_SHIFT;
    case IntSrc::SCSI_CURIO    : return VIA2_INT_SCSI_IRQ << VIA2_INT_SHIFT;
    case IntSrc::DAVBUS        : return VIA2_INT_SOUND    << VIA2_INT_SHIFT;
    case IntSrc::SWIM3         : return VIA2_INT_SWIM3    << VIA2_INT_SHIFT;

    case IntSrc::SLOT_0        : return SLOT_INT_SLOT_0   << SLOT_INT_SHIFT;
    case IntSrc::SLOT_1        : return SLOT_INT_SLOT_1   << SLOT_INT_SHIFT;
    case IntSrc::SLOT_2        : return SLOT_INT_SLOT_2   << SLOT_INT_SHIFT;
    case IntSrc::SLOT_PDS      : return SLOT_INT_SLOT_PDS << SLOT_INT_SHIFT;
    case IntSrc::SLOT_VDS      : return SLOT_INT_SLOT_VDS << SLOT_INT_SHIFT;
    case IntSrc::VBL           : return SLOT_INT_VBL      << SLOT_INT_SHIFT;

    case IntSrc::DMA_DAVBUS_Tx : return DMA1_INT_SOUND    << DMA1_INT_SHIFT;
    default:
        ABORT_F("AMIC: unknown interrupt source %d", src_id);
    }
    return 0;
}

uint64_t AMIC::register_dma_int(IntSrc src_id) {
    ABORT_F("AMIC: register_dma_int() not implemented");
    return 0;
}

void AMIC::ack_int(uint64_t irq_id, uint8_t irq_line_state) {
    // dispatch cascaded AMIC interrupts from various sources
    // irq_id format: 00DDCCBBAA where
    // - AA -> CPU interrupts
    // - BB -> pseudo VIA2 interrupts
    // - CC -> slot interrupts
    if (irq_id >> SLOT_INT_SHIFT) {
        this->ack_slot_int(irq_id >> SLOT_INT_SHIFT, irq_line_state);
    } else if (irq_id >> VIA2_INT_SHIFT) {
        this->ack_via2_int(irq_id >> VIA2_INT_SHIFT, irq_line_state);
    } else if (irq_id >> CPU_INT_SHIFT) {
        this->ack_cpu_int(irq_id >> CPU_INT_SHIFT, irq_line_state);
    } else {
        ABORT_F("AMIC: unknown interrupt source ID 0x%llX", irq_id);
    }
}

void AMIC::ack_slot_int(uint8_t slot_int, uint8_t irq_line_state) {
    // CAUTION: reverse logic (0 - true, 1 - false) in the IFR register!
    if (irq_line_state) {
        this->via2_slot_ifr &= ~slot_int;
    } else {
        this->via2_slot_ifr |= slot_int;
    }
    uint8_t new_irq = !!(~this->via2_slot_ifr & this->via2_slot_ier & 0x7F);
    if (new_irq != this->via2_slot_irq) {
        this->via2_slot_irq = new_irq;
        this->ack_via2_int(VIA2_INT_ALL_SLOT, new_irq);
    }
}

void AMIC::update_via2_irq() {
    uint8_t new_irq = !!(this->via2_ifr & this->via2_ier & 0x7F);
    this->via2_ifr  = (this->via2_ifr & 0x7F) | (new_irq << 7);
    if (new_irq != this->via2_irq) {
        this->via2_irq = new_irq;
        this->ack_cpu_int(CPU_INT_VIA2, new_irq);
    }
}

void AMIC::ack_via2_int(uint8_t via2_int, uint8_t irq_line_state) {
    if (irq_line_state) {
        this->via2_ifr |= via2_int;
    } else {
        this->via2_ifr &= ~via2_int;
    }
    this->update_via2_irq();
}

void AMIC::ack_cpu_int(uint8_t cpu_int, uint8_t irq_line_state) {
    if (this->int_ctrl & CPU_INT_MODE) { // 68k interrupt emulation mode?
        if (irq_line_state) {
            this->dev_irq_lines |= cpu_int;
        } else {
            this->dev_irq_lines &= ~cpu_int;
        }
        if (!(this->int_ctrl & CPU_INT_FLAG)) {
            this->int_ctrl |= CPU_INT_FLAG;
            ppc_assert_int();
            LOG_F(5, "AMIC: CPU INT asserted, source: 0x%02x", cpu_int);
        } else {
            LOG_F(5, "AMIC: CPU INT already latched");
        }

    } else {
        ABORT_F("AMIC: native interrupt mode not implemented");
    }
}

void AMIC::ack_dma_int(uint64_t irq_id, uint8_t irq_line_state) {
    if (irq_id >> DMA1_INT_SHIFT) { // DMA Interrupt Flags 1
        irq_id = (irq_id >> DMA1_INT_SHIFT) & 0xFFU;
        if (irq_line_state)
            this->dma_ifr1 |= irq_id;
        else
            this->dma_ifr1 &= ~irq_id;
    } else if (irq_id >> DMA0_INT_SHIFT) { // DMA Interrupt Flags 0
        irq_id = (irq_id >> DMA0_INT_SHIFT) & 0xFFU;
        if (irq_line_state)
            this->dma_ifr0 |= irq_id;
        else
            this->dma_ifr0 &= ~irq_id;
    }
    uint8_t new_irq = (this->dma_ifr0 || this->dma_ifr1) ? 1 : 0;
    if (new_irq != this->dma_irq) {
        this->dma_irq = new_irq;
        this->ack_cpu_int(CPU_INT_ALL_DMA, new_irq);
    }
}

// ============================ Sound DMA stuff ================================
AmicSndOutDma::AmicSndOutDma() : DmaOutChannel("SndOut")
{
    this->dma_out_ctrl = 0;
    this->enabled = false;
}

void AmicSndOutDma::init(uint32_t buf_base, uint32_t buf_samples) {
    this->out_buf0 = buf_base + AMIC_SND_OUT_BUF0_OFFS;
    this->out_buf1 = buf_base + AMIC_SND_OUT_BUF1_OFFS;

    this->out_buf_len = buf_samples * 2 * 2;

    this->snd_buf_num = 0;
    this->cur_buf_pos = 0;
    this->irq_level = 0;
}

uint8_t AmicSndOutDma::read_stat()
{
    return this->dma_out_ctrl;
}

void AmicSndOutDma::update_irq() {
    uint8_t new_level = !!((this->dma_out_ctrl >> 4) & this->dma_out_ctrl);
    if (new_level != this->irq_level) {
        this->irq_level = new_level;
        TimerManager::get_instance()->add_immediate_timer([this] {
            this->int_ctrl->ack_dma_int(this->snd_dma_irq_id, this->irq_level);
        });
    }
}

void AmicSndOutDma::write_dma_out_ctrl(uint8_t value)
{
    // clear interrupt flags
    if (value & PDM_DMA_INTS_MASK) {
        this->dma_out_ctrl &= ~(value & PDM_DMA_INTS_MASK);
        this->update_irq();
    }
    // update sound output DMA interrupt enable bits
    this->dma_out_ctrl = (this->dma_out_ctrl & 0xF1U) | (value & 0x0EU);
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
        } else {
            // signal buffer 1 drained
            this->dma_out_ctrl |= PDM_DMA_IF1;
        }

        // generate sound out DMA interrupt if (IF0 & IE0)
        // or (IF1 & IE1) or (ERR_IF & ERR_IE)
        this->update_irq();

        // check DMA enable flag after buffer 1 was processed
        // if it's false stop delivering sound data
        // this will effectively stop audio playback
        if (this->snd_buf_num && !this->enabled) {
            return DmaPullResult::NoMorePullData;
        }

        this->cur_buf_pos = 0;  // reset buffer position
        this->snd_buf_num ^= 1; // toggle sound buffers
        rem_len = this->out_buf_len; // buffer size = full buffer
    }

    uint32_t len = std::min((uint32_t)rem_len, req_len);

    MapDmaResult res = mmu_map_dma_mem(
        (this->snd_buf_num ? this->out_buf1 : this->out_buf0) + this->cur_buf_pos,
        len, false);
    *p_data = res.host_va;
    this->cur_buf_pos += len;
    *avail_len = len;
    return DmaPullResult::MorePullData;
}


AmicSndInDma::AmicSndInDma() : DmaInChannel("SndIn") {
    this->dma_in_ctrl = 0;
    this->enabled     = false;
}

void AmicSndInDma::init(uint32_t buf_base, uint32_t buf_samples) {
    this->in_buf0 = buf_base + AMIC_SND_IN_BUF0_OFFS;
    this->in_buf1 = buf_base + AMIC_SND_IN_BUF1_OFFS;

    this->in_buf_len = buf_samples * 2 * 2;

    this->snd_buf_num = 0;
    this->cur_buf_pos = 0;
    this->irq_level   = 0;
}

uint8_t AmicSndInDma::read_stat() {
    return this->dma_in_ctrl;
}

void AmicSndInDma::write_dma_in_ctrl(uint8_t value) {
    if (value & PDM_DMA_INTS_MASK) {
        this->dma_in_ctrl &= ~(value & PDM_DMA_INTS_MASK);
        this->update_irq();
    }
    // update sound input DMA interrupt enable bits
    this->dma_in_ctrl = value;
}

int AmicSndInDma::push_data(const char* src_ptr, int len) {
    len = std::min((int)this->byte_count, len);

    MapDmaResult res = mmu_map_dma_mem(
        (this->snd_buf_num ? this->in_buf1 : this->in_buf0) + this->cur_buf_pos, 
        len, false);
    uint8_t* p_data  = res.host_va;
    if (!res.is_writable) {
        ABORT_F("AMIC: attempting DMA write to read-only memory");
    }
    std::memcpy(p_data, src_ptr, len);

    this->addr_ptr += len;
    this->byte_count -= len;
    if (!this->byte_count) {
        LOG_F(WARNING, "AMIC: DMA interrupts not implemented yet");
    }

    return DmaPushResult::NoMorePushData;
}

void AmicSndInDma::update_irq() {
    uint8_t new_level = !!((this->dma_in_ctrl >> 4) & this->dma_in_ctrl);
    if (new_level != this->irq_level) {
        this->irq_level = new_level;
        TimerManager::get_instance()->add_immediate_timer(
            [this] { this->int_ctrl->ack_dma_int(this->snd_dma_irq_id, this->irq_level); });
    }
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

    MapDmaResult res = mmu_map_dma_mem(this->addr_ptr, len, false);
    uint8_t *p_data = res.host_va;
    if (!res.is_writable) {
        ABORT_F("AMIC: attempting DMA write to read-only memory");
    }
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
    return DmaPullResult::NoMorePullData;
}

// ============================ SCSI DMA stuff ================================
void AmicScsiDma::reset(const uint32_t addr_ptr)
{
    this->stat &= 0x48; // clear interrupt flag, RUN and RST bits
    this->addr_ptr   = addr_ptr;
    this->byte_count = 0;
}

void AmicScsiDma::reinit(const uint32_t addr_ptr)
{
    this->addr_ptr   = addr_ptr;
    this->byte_count = 0;
}

void AmicScsiDma::write_ctrl(uint8_t value)
{
    // copy over DIR, IE and RUN bits
    this->stat = (this->stat & 0x81) | (value & 0x4A);

    // clear interrupt flag if requested
    if (value & 0x80) {
        this->stat &= 0x7F;
    }
}

bool AmicScsiDma::is_ready() {
    return (this->stat & 2) ? true : false;
}

void AmicScsiDma::xfer_from_device() {
    if (this->dev_obj == nullptr)
        return;

    this->xfer_dir = DMA_DIR_FROM_DEV;

    uint32_t len = this->dev_obj->tell_xfer_size();

    MapDmaResult res = mmu_map_dma_mem(this->addr_ptr, len, false);

    int got_bytes = this->dev_obj->xfer_from(res.host_va, len);
    if (got_bytes > 0) {
        this->addr_ptr += got_bytes;
    }
}

void AmicScsiDma::xfer_to_device() {
    if (this->dev_obj == nullptr)
        return;

    this->xfer_dir = DMA_DIR_TO_DEV;

    uint32_t len = this->dev_obj->tell_xfer_size();

    MapDmaResult res = mmu_map_dma_mem(this->addr_ptr, len, false);

    int got_bytes = this->dev_obj->xfer_to(res.host_va, len);
    if (got_bytes > 0) {
        this->addr_ptr += got_bytes;
    }
}

void AmicScsiDma::xfer_retry() {
    if (this->xfer_dir == DMA_DIR_UNDEF)
        return;

    if (this->xfer_dir == DMA_DIR_FROM_DEV)
        this->xfer_from_device();
    else
        this->xfer_to_device();
}

// =========================== Serial DMA stuff ===============================
void AmicSerialXmitDma::write_ctrl(const uint8_t value)
{
    if (value & 1) { // RST bit set?
        this->stat &= 0x7C; // clear IF, RUN and RST bits
    }

    // copy PAUSE to FROZEN
    this->stat = (this->stat & 0xDF) | ((value & 0x10) << 1);

    // copy over RELOAD, PAUSE, IE, CONT and RUN bits
    this->stat = (this->stat & 0xA1) | (value & 0x5E);

    // clear interrupt flag if requested
    if (value & 0x80) {
        this->stat &= 0x7F;
    }
}

DmaPullResult AmicSerialXmitDma::pull_data(uint32_t req_len, uint32_t *avail_len,
                                           uint8_t **p_data)
{
    return DmaPullResult::NoMorePullData;
}

static std::vector<std::string> Amic_Subdevices = {
    "Sc53C94", "Escc", "Mace", "ViaCuda", "Swim3"
};

static const DeviceDescription Amic_Descriptor = {
    AMIC::create, Amic_Subdevices, {}, HWCompType::MMIO_DEV | HWCompType::INT_CTRL
};

REGISTER_DEVICE(Amic, Amic_Descriptor);