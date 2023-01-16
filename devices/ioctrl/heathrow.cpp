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

#include <cpu/ppc/ppcemu.h>
#include <devices/deviceregistry.h>
#include <devices/common/dbdma.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/viacuda.h>
#include <devices/floppy/swim3.h>
#include <devices/ioctrl/macio.h>
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

HeathrowIC::HeathrowIC() : PCIDevice("mac-io/heathrow"), InterruptCtrl()
{
    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::INT_CTRL);

    // populate my PCI config header
    this->vendor_id   = PCI_VENDOR_APPLE;
    this->device_id   = 0x0010;
    this->class_rev   = 0xFF000001;
    this->cache_ln_sz = 8;
    this->lat_timer   = 0x40;
    this->bars_cfg[0] = 0xFFF80000UL; // declare 512Kb of memory-mapped I/O space
    this->finish_config_bars();

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };

    // NVRAM connection
    this->nvram = dynamic_cast<NVram*>(gMachineObj->get_comp_by_name("NVRAM"));

    // connect Cuda
    this->viacuda = dynamic_cast<ViaCuda*>(gMachineObj->get_comp_by_name("ViaCuda"));

    // initialize sound chip and its DMA output channel, then wire them together
    this->screamer    = std::unique_ptr<AwacsScreamer> (new AwacsScreamer());
    this->snd_out_dma = std::unique_ptr<DMAChannel> (new DMAChannel());
    this->screamer->set_dma_out(this->snd_out_dma.get());
    this->snd_out_dma->set_callbacks(
        std::bind(&AwacsScreamer::dma_start, this->screamer.get()),
        std::bind(&AwacsScreamer::dma_end, this->screamer.get())
    );

    // connect SCSI HW
    this->mesh = dynamic_cast<MESHController*>(gMachineObj->get_comp_by_name("Mesh"));

    // connect serial HW
    this->escc = dynamic_cast<EsccController*>(gMachineObj->get_comp_by_name("Escc"));

    // connect floppy disk HW and initialize its DMA channel
    this->swim3 = dynamic_cast<Swim3::Swim3Ctrl*>(gMachineObj->get_comp_by_name("Swim3"));
    this->floppy_dma = std::unique_ptr<DMAChannel> (new DMAChannel());
    this->swim3->set_dma_channel(this->floppy_dma.get());

    // set EMMO pin status (active low)
    this->emmo_pin = GET_BIN_PROP("emmo") ^ 1;
}

void HeathrowIC::notify_bar_change(int bar_num)
{
    if (bar_num) // only BAR0 is supported
        return;

    if (this->base_addr != (this->bars[bar_num] & 0xFFFFFFF0UL)) {
        if (this->base_addr) {
            LOG_F(WARNING, "Heathrow: deallocating I/O memory not implemented");
        }
        this->base_addr = this->bars[0] & 0xFFFFFFF0UL;
        this->host_instance->pci_register_mmio_region(this->base_addr, 0x80000, this);
        LOG_F(INFO, "%s: base address set to 0x%X", this->pci_name.c_str(), this->base_addr);
    }
}

uint32_t HeathrowIC::dma_read(uint32_t offset, int size) {
    switch (offset >> 8) {
    case 1:
        return this->floppy_dma->reg_read(offset & 0xFF, size);
    case 8:
        return this->snd_out_dma->reg_read(offset & 0xFF, size);
    default:
        LOG_F(WARNING, "Unsupported DMA channel read, offset=0x%X", offset);
    }

    return 0;
}

void HeathrowIC::dma_write(uint32_t offset, uint32_t value, int size) {
    switch (offset >> 8) {
    case 1:
        this->floppy_dma->reg_write(offset & 0xFF, value, size);
        break;
    case 8:
        this->snd_out_dma->reg_write(offset & 0xFF, value, size);
        break;
    default:
        LOG_F(WARNING, "Unsupported DMA channel write, offset=0x%X, val=0x%X", offset, value);
    }
}


uint32_t HeathrowIC::read(uint32_t rgn_start, uint32_t offset, int size) {
    uint32_t res = 0;

    LOG_F(9, "%s: reading from offset %x", this->name.c_str(), offset);

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
    case 0x12: // ESCC compatible addressing
        if ((offset & 0xFF) < 16) {
            return this->escc->read(compat_to_macrisc[(offset >> 1) & 0xF]);
        }
        // fallthrough
    case 0x13: // ESCC MacRISC addressing
        return this->escc->read((offset >> 4) & 0xF);
    case 0x14:
        res = this->screamer->snd_ctrl_read(offset - 0x14000, size);
        break;
    case 0x15: // SWIM3
        return this->swim3->read((offset >> 4 )& 0xF);
    case 0x16:
    case 0x17:
        res = this->viacuda->read((offset - 0x16000) >> 9);
        break;
    default:
        if (sub_addr >= 0x60) {
            res = this->nvram->read_byte((offset - 0x60000) >> 4);
        } else {
            LOG_F(WARNING, "Attempting to read unmapped I/O space: %x", offset);
        }
    }

    return res;
}

void HeathrowIC::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) {
    LOG_F(9, "%s: writing to offset %x", this->name.c_str(), offset);

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
        break;
    case 0x12: // ESCC compatible addressing
        if ((offset & 0xFF) < 16) {
            this->escc->write(compat_to_macrisc[(offset >> 1) & 0xF], value);
            break;
        }
        // fallthrough
    case 0x13: // ESCC MacRISC addressing
        this->escc->write((offset >> 4) & 0xF, value);
        break;
    case 0x14:
        this->screamer->snd_ctrl_write(offset - 0x14000, value, size);
        break;
    case 0x15: // SWIM3
        this->swim3->write((offset >> 4) & 0xF, value);
        break;
    case 0x16:
    case 0x17:
        this->viacuda->write((offset - 0x16000) >> 9, value);
        break;
    default:
        if (sub_addr >= 0x60) {
            this->nvram->write_byte((offset - 0x60000) >> 4, value);
        } else {
            LOG_F(WARNING, "Attempting to write to unmapped I/O space: %x", offset);
        }
    }
}

uint32_t HeathrowIC::mio_ctrl_read(uint32_t offset, int size) {
    uint32_t res = 0;

    switch (offset & 0xFC) {
    case MIO_INT_EVENTS2:
        res = this->int_events2;
        break;
    case MIO_INT_MASK2:
        res = this->int_mask2;
        break;
    case MIO_INT_LEVELS2:
        res = this->int_levels2;
        break;
    case MIO_INT_EVENTS1:
        res = this->int_events1;
        break;
    case MIO_INT_MASK1:
        res = this->int_mask1;
        break;
    case MIO_INT_LEVELS1:
        res = this->int_levels1;
        break;
    case MIO_OHARE_ID:
        LOG_F(9, "read from MIO:ID register at Address %x", ppc_state.pc);
        res = (this->fp_id << 24) | (this->mon_id << 16) | (this->mb_id << 8) |
              (this->cpu_id | (this->emmo_pin << 4));
        break;
    case MIO_OHARE_FEAT_CTRL:
        LOG_F(9, "read from MIO:Feat_Ctrl register");
        res = this->feat_ctrl;
        break;
    default:
        LOG_F(WARNING, "read from unknown MIO register at %x", offset);
        break;
    }

    return BYTESWAP_32(res);
}

void HeathrowIC::mio_ctrl_write(uint32_t offset, uint32_t value, int size) {
    switch (offset & 0xFC) {
    case MIO_INT_MASK2:
        this->int_mask2 |= BYTESWAP_32(value) & ~MACIO_INT_MODE;
        break;
    case MIO_INT_CLEAR2:
        if (value & MACIO_INT_CLR) {
            this->int_events2 = 0;
            clear_cpu_int();
        } else {
            this->int_events2 &= BYTESWAP_32(value);
        }
        break;
    case MIO_INT_MASK1:
        this->int_mask1 = BYTESWAP_32(value);
        // copy IntMode bit to InterruptMask2 register
        this->int_mask2 = (this->int_mask2 & ~MACIO_INT_MODE) | (this->int_mask1 & MACIO_INT_MODE);
        break;
    case MIO_INT_CLEAR1:
        if (value & MACIO_INT_CLR) {
            this->int_events1 = 0;
            clear_cpu_int();
        } else {
            this->int_events1 &= BYTESWAP_32(value);
        }
        break;
    case MIO_OHARE_ID:
        LOG_F(WARNING, "Attempted to write %x to MIO:ID at %x; Address : %x", value, offset, ppc_state.pc);
        break;
    case MIO_OHARE_FEAT_CTRL:
        this->feature_control(BYTESWAP_32(value));
        break;
    case 0x3C:
        LOG_F(9, "write %x to MIO:Aux_Ctrl register", value);
        this->aux_ctrl = value;
        break;
    default:
        LOG_F(WARNING, "write %x to unknown MIO register at %x", value, offset);
        break;
    }
}

void HeathrowIC::feature_control(const uint32_t value)
{
    LOG_F(9, "write %x to MIO:Feat_Ctrl register", value);

    this->feat_ctrl = value;

    if (!(this->feat_ctrl & 1)) {
        LOG_F(9, "Heathrow: Monitor sense enabled");
    } else {
        LOG_F(9, "Heathrow: Monitor sense disabled");
    }
}

uint32_t HeathrowIC::register_dev_int(IntSrc src_id)
{
    switch (src_id) {
    case IntSrc::VIA_CUDA:
        return 1 << 7;
    case IntSrc::SCSI1:
        return 1 << 1;
    case IntSrc::SWIM3:
        return 1 << 8;
    default:
        ABORT_F("Heathrow: unknown interrupt source %d", src_id);
    }
    return 0;
}

uint32_t HeathrowIC::register_dma_int(IntSrc src_id)
{
    return 0;
}

void HeathrowIC::ack_int(uint32_t irq_id, uint8_t irq_line_state)
{
    if (this->int_mask1 & MACIO_INT_MODE) { // 68k interrupt emulation mode?
        if (irq_id > 0x200000) {
            irq_id >>= 21;
            this->int_events2 |= irq_id; // signal IRQ line change
            this->int_events2 &= this->int_mask2;
            // update IRQ line state
            if (irq_line_state) {
                this->int_levels2 |= irq_id;
            } else {
                this->int_levels2 &= ~irq_id;
            }
        } else {
            irq_id <<= 11;
            this->int_events1 |= irq_id; // signal IRQ line change
            this->int_events1 &= this->int_mask1;
            // update IRQ line state
            if (irq_line_state) {
                this->int_levels1 |= irq_id;
            } else {
                this->int_levels1 &= ~irq_id;
            }
        }
        // signal CPU interrupt
        if (this->int_events1 || this->int_events2) {
            if (!this->cpu_int_latch) {
                this->cpu_int_latch = true;
                ppc_assert_int();
                LOG_F(5, "Heathrow: CPU INT asserted, source: %d", irq_id);
            } else {
                LOG_F(5, "Heathrow: CPU INT already latched");
            }
        }
    } else {
        ABORT_F("Heathrow: native interrupt mode not implemented");
    }
}

void HeathrowIC::ack_dma_int(uint32_t irq_id, uint8_t irq_line_state)
{
}

void HeathrowIC::clear_cpu_int()
{
    if (!this->int_events1 && !this->int_events2) {
        this->cpu_int_latch = false;
        ppc_release_int();
        LOG_F(5, "Heathrow: CPU INT latch cleared");
    }
}

static const vector<string> Heathrow_Subdevices = {
    "NVRAM", "ViaCuda", "Mesh", "Escc", "Swim3"
};

static const DeviceDescription Heathrow_Descriptor = {
    HeathrowIC::create, Heathrow_Subdevices, {}
};

REGISTER_DEVICE(Heathrow, Heathrow_Descriptor);
