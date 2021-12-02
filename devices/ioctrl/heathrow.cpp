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
#include <devices/common/dbdma.h>
#include <devices/common/viacuda.h>
#include <devices/ioctrl/macio.h>
#include <devices/ioctrl/interruptctrl.h>
#include <devices/serial/escc.h>
#include <devices/sound/awacs.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <machines/machinebase.h>

#include <cinttypes>
#include <functional>
#include <iostream>
#include <memory>

/** Heathrow Mac I/O device emulation.

    Author: Max Poliakovski
*/

using namespace std;

HeathrowIC::HeathrowIC() : PCIDevice("mac-io/heathrow") {
    this->nvram = std::unique_ptr<NVram> (new NVram());

    this->viacuda = std::unique_ptr<ViaCuda> (new ViaCuda());
    gMachineObj->add_subdevice("ViaCuda", this->viacuda.get());

    // initialize sound chip and its DMA output channel, then wire them together
    this->screamer    = std::unique_ptr<AwacsScreamer> (new AwacsScreamer());
    this->snd_out_dma = std::unique_ptr<DMAChannel> (new DMAChannel());
    this->screamer->set_dma_out(this->snd_out_dma.get());
    this->snd_out_dma->set_callbacks(
        std::bind(&AwacsScreamer::dma_start, this->screamer.get()),
        std::bind(&AwacsScreamer::dma_end, this->screamer.get())
    );

    this->mesh        = std::unique_ptr<MESHController> (new MESHController(HeathrowMESHID));
    this->escc        = std::unique_ptr<EsccController>(new EsccController());
}

uint32_t HeathrowIC::register_device(DEV_ID dev_id){
    return (1 << dev_id);
};

void HeathrowIC::process_interrupt(uint32_t cookie) {
    bool confirm_interrupt = false;
    uint32_t bit_field = 0;

    switch (cookie) { 
        case DEV_ID::DAVBUS_DMA_OUT:
            bit_field = (1 << DEV_ID::DAVBUS_DMA_OUT);
            break;
        case DEV_ID::SCSI0:
            bit_field = (1 << DEV_ID::SCSI0);
            break;
        case DEV_ID::VIA_CUDA:
            bit_field = (1 << DEV_ID::VIA_CUDA);
            break;
        case DEV_ID::SWIM3:
            bit_field = (1 << DEV_ID::SWIM3);
            break;
        case DEV_ID::DAVBUS:
            bit_field = (1 << DEV_ID::DAVBUS);
            break;
    }

    if (confirm_interrupt)
        ack_interrupt(bit_field);
};

uint32_t HeathrowIC::pci_cfg_read(uint32_t reg_offs, uint32_t size) {
    return this->pci_cfg_hdr[reg_offs & 0xFF];
}

void HeathrowIC::pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size) {
    switch (reg_offs) {
    case CFG_REG_BAR0:    // base address register
        value = LE2BE(value);
        if (value == 0xFFFFFFFF) {
            LOG_F(
                ERROR,
                "%s err: BAR0 block size determination not \
                implemented yet \n",
                this->name.c_str());
        } else if (value & 1) {
            LOG_F(ERROR, "%s err: BAR0 I/O space not supported! \n", this->name.c_str());
        } else if (value & 0x06) {
            LOG_F(ERROR, "%s err: BAR0 64-bit I/O space not supported! \n", this->name.c_str());
        } else {
            this->base_addr = value & 0xFFF80000;
            this->host_instance->pci_register_mmio_region(this->base_addr, 0x80000, this);
            LOG_F(INFO, "%s base address set to %x \n", this->name.c_str(), this->base_addr);
        }
        break;
    }
}

uint32_t HeathrowIC::dma_read(uint32_t offset, int size) {
    uint32_t res = 0;

    switch (offset >> 8) {
    case 8:
        res = this->snd_out_dma->reg_read(offset & 0xFF, size);
        this->ack_interrupt(DEV_ID::DAVBUS_DMA_OUT);
        break;
    default:
        LOG_F(WARNING, "Unsupported DMA channel read, offset=0x%X", offset);
    }

    return res;
}

void HeathrowIC::dma_write(uint32_t offset, uint32_t value, int size) {
    switch (offset >> 8) {
    case 8:
        this->snd_out_dma->reg_write(offset & 0xFF, value, size);
        break;
    default:
        LOG_F(WARNING, "Unsupported DMA channel write, offset=0x%X, val=0x%X", offset, value);
    }
}


uint32_t HeathrowIC::read(uint32_t reg_start, uint32_t offset, int size) {
    uint32_t res = 0;

    LOG_F(9, "%s: reading from offset %x \n", this->name.c_str(), offset);

    unsigned sub_addr = (offset >> 12) & 0x7F;

    switch (sub_addr) {
    case 0:
        res = mio_ctrl_read(offset, size);
        break;
    case 8:
        res = dma_read(offset - 0x8000, size);
        break;
    case 0x10:
        res = this->mesh->read((offset >> 4) & 0xF);
        break;
    case 0x11: // BMAC
        LOG_F(WARNING, "Attempted to read from BMAC: %x \n", (offset - 0x11000));    
        break;
    case 0x12: // ESCC compatible
        return this->escc->read_compat((offset >> 4) & 0xF);
    case 0x13: // ESCC MacRISC
        return this->escc->read((offset >> 4) & 0xF);
    case 0x14:
        res = this->screamer->snd_ctrl_read(offset - 0x14000, size);
        break;
    case 0x15:    // BMAC
        LOG_F(WARNING, "Attempted to read from SWIM3: %x \n", (offset - 0x15000));
        break;
    case 0x16:
    case 0x17:
        res = this->viacuda->read((offset - 0x16000) >> 9);
        break;
    case 0x20:    // IDE
    case 0x21:
        LOG_F(WARNING, "Attempted to read from IDE: %x \n", (offset - 0x20000));
        break;
    default:
        if (sub_addr >= 0x60) {
            res = this->nvram->read_byte((offset - 0x60000) >> 4);
        } else {
            LOG_F(WARNING, "Attempting to read unmapped I/O space: %x \n", offset);
        }
    }

    return res;
}

void HeathrowIC::write(uint32_t reg_start, uint32_t offset, uint32_t value, int size) {
    LOG_F(9, "%s: writing to offset %x \n", this->name.c_str(), offset);

    unsigned sub_addr = (offset >> 12) & 0x7F;

    switch (sub_addr) {
    case 0:
        mio_ctrl_write(offset, value, size);
        break;
    case 8:
        dma_write(offset - 0x8000, value, size);
        break;
    case 0x10:
        this->mesh->write((offset >> 4) & 0xF, value);
        this->process_interrupt(DEV_ID::SCSI0);
        break;
    case 0x11: // BMAC
        LOG_F(WARNING, "Attempted to write to BMAC: %x \n", (offset - 0x11000));    
        break;
    case 0x12: // ESCC compatible
        this->escc->write_compat((offset >> 4) & 0xF, value);
        break;
    case 0x13: // ESCC MacRISC
        this->escc->write((offset >> 4) & 0xF, value);
        break;
    case 0x14:
        this->screamer->snd_ctrl_write(offset - 0x14000, value, size);
        this->process_interrupt(DEV_ID::DAVBUS);
        break;
    case 0x15: // SWIM 3
        LOG_F(WARNING, "Attempted to write to SWIM 3: %x \n", (offset - 0x15000));
        break;
    case 0x16:
    case 0x17:
        this->viacuda->write((offset - 0x16000) >> 9, value);
        this->process_interrupt(DEV_ID::VIA_CUDA);
        break;
    case 0x20: // IDE
    case 0x21:
        LOG_F(WARNING, "Attempted to write to IDE: %x \n", (offset - 0x20000));    
        break;
    default:
        if (sub_addr >= 0x60) {
            this->nvram->write_byte((offset - 0x60000) >> 4, value);
        } else {
            LOG_F(WARNING, "Attempting to write to unmapped I/O space: %x \n", offset);
        }
    }
}

uint32_t HeathrowIC::mio_ctrl_read(uint32_t offset, int size) {
    uint32_t res = 0;

    switch (offset & 0xFC) {
    case 0x10:
        LOG_F(0, "read from MIO:Int_Events2 register \n");
        res = this->retrieve_reg(REG_ID::events2);
        break;
    case 0x14:
        LOG_F(0, "read from MIO:Int_Mask2 register \n");
        res = this->retrieve_reg(REG_ID::mask2);
        break;
    case 0x18:
        LOG_F(0, "read from MIO:Int_Clear2 register \n");
        res = this->retrieve_reg(REG_ID::clear2);
        break;
    case 0x1C:
        LOG_F(0, "read from MIO:Int_Levels2 register \n");
        res = this->retrieve_reg(REG_ID::levels2);
        break;
    case 0x20:
        LOG_F(0, "read from MIO:Int_Events1 register \n");
        res = this->retrieve_reg(REG_ID::events1);
        break;
    case 0x24:
        LOG_F(0, "read from MIO:Int_Mask1 register \n");
        res = this->retrieve_reg(REG_ID::mask1);
        break;
    case 0x28:
        LOG_F(0, "read from MIO:Int_Clear1 register \n");
        res = this->retrieve_reg(REG_ID::clear1);
        break;
    case 0x2C:
        LOG_F(0, "read from MIO:Int_Levels1 register \n");
        res = this->retrieve_reg(REG_ID::levels1);
        break;
    case 0x34: /* heathrowIDs / HEATHROW_MBCR (Linux): media bay config reg? */
        LOG_F(9, "read from MIO:ID register at Address %x \n", ppc_state.pc);
        res = this->macio_id;
        break;
    case 0x38:
        LOG_F(9, "read from MIO:Feat_Ctrl register \n");
        res = BYTESWAP_32(this->feat_ctrl);
        break;
    default:
        LOG_F(WARNING, "read from unknown MIO register at %x \n", offset);
        break;
    }

    return res;
}

void HeathrowIC::mio_ctrl_write(uint32_t offset, uint32_t value, int size) {
    switch (offset & 0xFC) {
    case 0x10:
        LOG_F(0, "write %x to MIO:Int_Events2 register \n", value);
        this->update_reg(REG_ID::events2, value);
        break;
    case 0x14:
        LOG_F(0, "write %x to MIO:Int_Mask2 register \n", value);
        this->update_reg(REG_ID::mask2, value);
        break;
    case 0x18:
        LOG_F(0, "write %x to MIO:Int_Clear2 register \n", value);
        this->update_reg(REG_ID::clear2, value);
        break;
    case 0x1C:
        LOG_F(0, "write %x to MIO:Int_Levels2 register \n", value);
        this->update_reg(REG_ID::levels2, value);
        break;
    case 0x20:
        LOG_F(0, "write %x to MIO:Int_Events1 register \n", value);
        this->update_reg(REG_ID::events1, value);
        break;
    case 0x24:
        LOG_F(0, "write %x to MIO:Int_Mask1 register \n", value);
        this->update_reg(REG_ID::mask1, value);
        break;
    case 0x28:
        LOG_F(0, "write %x to MIO:Int_Clear1 register \n", value);
        this->update_reg(REG_ID::clear1, value);
        break;
    case 0x2C:
        LOG_F(0, "write %x to MIO:Int_Levels1 register \n", value);
        this->update_reg(REG_ID::levels1, value);
        break;
    case 0x34:
        LOG_F(WARNING, "Attempted to write %x to MIO:ID at %x; Address : %x \n", value, offset, ppc_state.pc);
        break;
    case 0x38:
        this->feature_control(BYTESWAP_32(value));
        break;
    case 0x3C:
        LOG_F(9, "write %x to MIO:Aux_Ctrl register \n", value);
        this->aux_ctrl = value;
        break;
    default:
        LOG_F(WARNING, "write %x to unknown MIO register at %x \n", value, offset);
        break;
    }
}

void HeathrowIC::feature_control(const uint32_t value)
{
    LOG_F(0, "write %x to MIO:Feat_Ctrl register \n", value);

    this->feat_ctrl = value;

    if (!(this->feat_ctrl & 1)) {
        LOG_F(9, "Heathrow: Monitor sense enabled");
    } else {
        LOG_F(9, "Heathrow: Monitor sense disabled");
    }
}
