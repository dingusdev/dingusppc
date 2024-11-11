/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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
#include <devices/common/ata/idechannel.h>
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
#include <memory>

/** Heathrow Mac I/O device emulation.

    Author: Max Poliakovski
*/

using namespace std;

HeathrowIC::HeathrowIC() : PCIDevice("mac-io_heathrow"), InterruptCtrl()
{
    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::INT_CTRL);

    // populate my PCI config header
    this->vendor_id   = PCI_VENDOR_APPLE;
    this->device_id   = 0x0010;
    this->class_rev   = 0xFF000001;
    this->cache_ln_sz = 8;

    this->setup_bars({{0, 0xFFF80000UL}}); // declare 512Kb of memory-mapped I/O space

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };

    // NVRAM connection
    this->nvram = dynamic_cast<NVram*>(gMachineObj->get_comp_by_name("NVRAM"));

    // connect Cuda
    this->viacuda = dynamic_cast<ViaCuda*>(gMachineObj->get_comp_by_name("ViaCuda"));

    // find appropriate sound chip, create a DMA output channel for sound,
    // then wire everything together
    this->snd_codec   = dynamic_cast<MacioSndCodec*>(gMachineObj->get_comp_by_type(HWCompType::SND_CODEC));
    this->snd_out_dma = std::unique_ptr<DMAChannel> (new DMAChannel("snd_out"));
    this->snd_out_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_DAVBUS_Tx));
    this->snd_codec->set_dma_out(this->snd_out_dma.get());
    this->snd_out_dma->set_callbacks(
        std::bind(&AwacsScreamer::dma_out_start, this->snd_codec),
        std::bind(&AwacsScreamer::dma_out_stop, this->snd_codec)
    );

    // connect SCSI HW and the corresponding DMA channel
    this->mesh = dynamic_cast<MeshController*>(gMachineObj->get_comp_by_name("MeshHeathrow"));
    this->mesh_dma = std::unique_ptr<DMAChannel> (new DMAChannel("mesh"));

    // connect IDE HW
    this->ide_0 = dynamic_cast<IdeChannel*>(gMachineObj->get_comp_by_name("Ide0"));
    this->ide_1 = dynamic_cast<IdeChannel*>(gMachineObj->get_comp_by_name("Ide1"));

    // connect serial HW
    this->escc = dynamic_cast<EsccController*>(gMachineObj->get_comp_by_name("Escc"));

    // connect DBDMA
    this->escc_b_rcv_dma = std::unique_ptr<DMAChannel>(new DMAChannel("DBDMABRx"));

    // connect floppy disk HW and initialize its DMA channel
    this->swim3 = dynamic_cast<Swim3::Swim3Ctrl*>(gMachineObj->get_comp_by_name("Swim3"));
    this->floppy_dma = std::unique_ptr<DMAChannel> (new DMAChannel("floppy"));
    this->swim3->set_dma_channel(this->floppy_dma.get());
    this->floppy_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_SWIM3));

    // connect Ethernet HW
    this->bmac = dynamic_cast<BigMac*>(gMachineObj->get_comp_by_type(HWCompType::ETHER_MAC));
    this->enet_xmit_dma = std::unique_ptr<DMAChannel> (new DMAChannel("BmacTx"));
    this->enet_rcv_dma  = std::unique_ptr<DMAChannel> (new DMAChannel("BmacRx"));

    // set EMMO pin status (active low)
    this->emmo_pin = GET_BIN_PROP("emmo") ^ 1;
}

void HeathrowIC::set_media_bay_id(uint8_t id) {
    this->mb_id = id;
}

void HeathrowIC::notify_bar_change(int bar_num)
{
    if (bar_num) // only BAR0 is supported
        return;

    if (this->base_addr != (this->bars[bar_num] & 0xFFFFFFF0UL)) {
        if (this->base_addr) {
            this->host_instance->pci_unregister_mmio_region(this->base_addr, 0x80000, this);
        }
        this->base_addr = this->bars[0] & 0xFFFFFFF0UL;
        this->host_instance->pci_register_mmio_region(this->base_addr, 0x80000, this);
        LOG_F(INFO, "%s: base address set to 0x%X", this->get_name().c_str(), this->base_addr);
    }
}

uint32_t HeathrowIC::dma_read(uint32_t offset, int size) {
    switch (offset >> 8) {
    case MIO_OHARE_DMA_MESH:
        if (this->mesh_dma)
            return this->mesh_dma->reg_read(offset & 0xFF, size);
        else
            return 0;
    case MIO_OHARE_DMA_FLOPPY:
        return this->floppy_dma->reg_read(offset & 0xFF, size);
    case MIO_OHARE_DMA_ETH_XMIT:
        return this->enet_xmit_dma->reg_read(offset & 0xFF, size);
    case MIO_OHARE_DMA_ETH_RCV:
        return this->enet_rcv_dma->reg_read(offset & 0xFF, size);
    case MIO_OHARE_DMA_ESCC_B_RCV:
        return this->escc_b_rcv_dma->reg_read(offset & 0xFF, size);
    case MIO_OHARE_DMA_AUDIO_OUT:
        return this->snd_out_dma->reg_read(offset & 0xFF, size);
    default:
        LOG_F(WARNING, "Unsupported DMA channel read, offset=0x%X", offset);
    }

    return 0;
}

void HeathrowIC::dma_write(uint32_t offset, uint32_t value, int size) {
    switch (offset >> 8) {
    case MIO_OHARE_DMA_MESH:
        if (this->mesh_dma) this->mesh_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_FLOPPY:
        this->floppy_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_ETH_XMIT:
        this->enet_xmit_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_ETH_RCV:
        this->enet_rcv_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_ESCC_B_RCV:
        this->escc_b_rcv_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_AUDIO_OUT:
        this->snd_out_dma->reg_write(offset & 0xFF, value, size);
        break;
    default:
        LOG_F(WARNING, "Unsupported DMA channel write, offset=0x%X, val=0x%X", offset, value);
    }
}


uint32_t HeathrowIC::read(uint32_t rgn_start, uint32_t offset, int size) {
    uint32_t value;

    LOG_F(9, "%s: reading from offset %x", this->name.c_str(), offset);

    unsigned sub_addr = (offset >> 12) & 0x7F;

    switch (sub_addr) {
    case 0:
        value = mio_ctrl_read(offset, size);
        break;
    case 8:
        value = dma_read(offset & 0x7FFF, size);
        break;
    case 0x10: // SCSI
        value = this->mesh->read((offset >> 4) & 0xF);
        break;
    case 0x11: // Ethernet
        value = BYTESWAP_SIZED(this->bmac->read(offset & 0xFFFU), size);
        break;
    case 0x12: // ESCC compatible addressing
        if ((offset & 0xFF) < 0x0C) {
            value = this->escc->read(compat_to_macrisc[(offset >> 1) & 0xF]);
            break;
        }
        if ((offset & 0xFF) < 0x60) {
            value = 0;
            LOG_F(ERROR, "%s: ESCC compatible read  @%x.%c", this->name.c_str(), offset, SIZE_ARG(size));
            break;
        }
        // fallthrough
    case 0x13: // ESCC MacRISC addressing
        value = this->escc->read((offset >> 4) & 0xF);
        break;
    case 0x14:
        value = this->snd_codec->snd_ctrl_read(offset & 0xFF, size);
        break;
    case 0x15: // SWIM3
        value = this->swim3->read((offset >> 4) & 0xF);
        break;
    case 0x16: // VIA-CUDA
    case 0x17:
        value = this->viacuda->read((offset >> 9) & 0xF);
        break;
    case 0x20: // IDE 0
        value = this->ide_0->read((offset >> 4) & 0x1F, size);
        break;
    case 0x21: // IDE 1
        value = this->ide_1->read((offset >> 4) & 0x1F, size);
        break;
    default:
        if (sub_addr >= 0x60) {
            value = this->nvram->read_byte((offset >> 4) & 0x1FFF);
        } else {
            value = 0;
            LOG_F(WARNING, "Attempting to read from unmapped I/O space: %x", offset);
        }
    }

    return value;
}

void HeathrowIC::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) {
    LOG_F(9, "%s: writing to offset %x", this->name.c_str(), offset);

    unsigned sub_addr = (offset >> 12) & 0x7F;

    switch (sub_addr) {
    case 0:
        this->mio_ctrl_write(offset, value, size);
        break;
    case 8:
        this->dma_write(offset & 0x7FFF, value, size);
        break;
    case 0x10: // SCSI
        this->mesh->write((offset >> 4) & 0xF, value);
        break;
    case 0x11: // Ethernet
        this->bmac->write(offset & 0xFFFU, BYTESWAP_SIZED(value, size));
        break;
    case 0x12: // ESCC compatible addressing
        if ((offset & 0xFF) < 0x0C) {
            this->escc->write(compat_to_macrisc[(offset >> 1) & 0xF], value);
            break;
        }
        if ((offset & 0xFF) < 0x60) {
            LOG_F(ERROR, "%s: SCC write @%x.%c = %0*x", this->name.c_str(),
                offset, SIZE_ARG(size), size * 2, value);
            break;
        }
        // fallthrough
    case 0x13: // ESCC MacRISC addressing
        this->escc->write((offset >> 4) & 0xF, value);
        break;
    case 0x14:
        this->snd_codec->snd_ctrl_write(offset & 0xFF, value, size);
        break;
    case 0x15: // SWIM3
        this->swim3->write((offset >> 4) & 0xF, value);
        break;
    case 0x16: // VIA-CUDA
    case 0x17:
        this->viacuda->write((offset >> 9) & 0xF, value);
        break;
    case 0x20: // IDE O
        this->ide_0->write((offset >> 4) & 0x1F, value, size);
        break;
    case 0x21: // IDE 1
        this->ide_1->write((offset >> 4) & 0x1F, value, size);
        break;
    default:
        if (sub_addr >= 0x60) {
            this->nvram->write_byte((offset - 0x60000) >> 4, value);
        } else {
            LOG_F(WARNING, "Attempting to write to  unmapped I/O space: %x", offset);
        }
    }
}

uint32_t HeathrowIC::mio_ctrl_read(uint32_t offset, int size) {
    uint32_t value;

    switch (offset & 0xFC) {
    case MIO_INT_EVENTS2:
        value = this->int_events >> 32;
        break;
    case MIO_INT_MASK2:
        value = this->int_mask >> 32;
        break;
    case MIO_INT_LEVELS2:
        value = this->int_levels >> 32;
        break;
    case MIO_INT_EVENTS1:
        value = uint32_t(this->int_events);
        break;
    case MIO_INT_MASK1:
        value = uint32_t(this->int_mask);
        break;
    case MIO_INT_LEVELS1:
        value = uint32_t(int_levels);
        break;
    case MIO_INT_CLEAR1:
    case MIO_INT_CLEAR2:
        // some Mac OS drivers reads from those write-only registers
        // so we return zero here as real HW does
        value = 0;
        break;
    case MIO_OHARE_ID:
        LOG_F(9, "read from MIO:ID register at Address %x", ppc_state.pc);
        value = (this->fp_id << 24) | (this->mon_id << 16) | (this->mb_id << 8) |
            (this->cpu_id | (this->emmo_pin << 4));
        break;
    case MIO_OHARE_FEAT_CTRL:
        LOG_F(9, "read from MIO:Feat_Ctrl register");
        value = this->feat_ctrl;
        break;
    default:
        value = 0;
        LOG_F(WARNING, "read from unknown MIO register at %x", offset);
        break;
    }

    return BYTESWAP_32(value);
}

void HeathrowIC::mio_ctrl_write(uint32_t offset, uint32_t value, int size) {
    switch (offset & 0xFC) {
    case MIO_INT_MASK2:
        this->int_mask |= uint64_t(BYTESWAP_32(value) & ~MACIO_INT_MODE) << 32;
        this->signal_cpu_int();
        break;
    case MIO_INT_CLEAR2:
        this->int_events &= ~(uint64_t(BYTESWAP_32(value) & 0x7FFFFFFFUL) << 32);
        clear_cpu_int();
        break;
    case MIO_INT_MASK1:
        this->int_mask = (this->int_mask & 0x7FFFFFFF00000000ULL) | BYTESWAP_32(value);
        // copy IntMode bit to InterruptMask2 register
        this->int_mask |= uint64_t(this->int_mask & MACIO_INT_MODE) << 32;
        this->signal_cpu_int();
        break;
    case MIO_INT_CLEAR1:
        if ((this->int_mask & MACIO_INT_MODE) && (value & MACIO_INT_CLR)) {
            this->int_events = 0;
        } else {
            this->int_events &= ~uint64_t(BYTESWAP_32(value) & 0x7FFFFFFFUL);
        }
        clear_cpu_int();
        break;
    case MIO_OHARE_ID:
        LOG_F(WARNING, "Attempted to write %x to MIO:ID at %x; Address : %x", value, offset, ppc_state.pc);
        break;
    case MIO_OHARE_FEAT_CTRL:
        this->feature_control(BYTESWAP_32(value));
        break;
    case MIO_AUX_CTRL:
        LOG_F(9, "write %x to MIO:Aux_Ctrl register", value);
        this->aux_ctrl = value;
        break;
    default:
        LOG_F(WARNING, "write %x to unknown MIO register at %x", value, offset);
        break;
    }
}

void HeathrowIC::feature_control(uint32_t value)
{
    LOG_F(9, "write %x to MIO:Feat_Ctrl register", value);

    this->feat_ctrl = value;

    if (!(this->feat_ctrl & 1)) {
        LOG_F(9, "Heathrow: Monitor sense enabled");
    } else {
        LOG_F(9, "Heathrow: Monitor sense disabled");
    }
}

#define INT_TO_IRQ_ID(intx) (1ULL << intx)

uint64_t HeathrowIC::register_dev_int(IntSrc src_id)
{
    switch (src_id) {
    case IntSrc::SCSI_MESH  : return INT_TO_IRQ_ID(0x0C);
    case IntSrc::IDE0       : return INT_TO_IRQ_ID(0x0D); // Beige G3 first IDE controller, or Yosemite ata-3
    case IntSrc::IDE1       : return INT_TO_IRQ_ID(0x0E);
    case IntSrc::SCCA       : return INT_TO_IRQ_ID(0x0F);
    case IntSrc::SCCB       : return INT_TO_IRQ_ID(0x10);
    case IntSrc::DAVBUS     : return INT_TO_IRQ_ID(0x11);
    case IntSrc::VIA_CUDA   : return INT_TO_IRQ_ID(0x12);
    case IntSrc::SWIM3      : return INT_TO_IRQ_ID(0x13);
    case IntSrc::NMI        : return INT_TO_IRQ_ID(0x14); // nmiSource in AppleHeathrow/Heathrow.cpp; programmer-switch in B&W G3

    case IntSrc::PERCH2     : return INT_TO_IRQ_ID(0x15);
    case IntSrc::PCI_GPU    : return INT_TO_IRQ_ID(0x16);
    case IntSrc::PCI_A      : return INT_TO_IRQ_ID(0x17);
    case IntSrc::PCI_B      : return INT_TO_IRQ_ID(0x18);
    case IntSrc::PCI_C      : return INT_TO_IRQ_ID(0x19);
    case IntSrc::PERCH1     : return INT_TO_IRQ_ID(0x1A);
    case IntSrc::PCI_PERCH  : return INT_TO_IRQ_ID(0x1C);

    case IntSrc::FIREWIRE   : return INT_TO_IRQ_ID(0x15); // Yosemite built-in PCI FireWire
    case IntSrc::PCI_J12    : return INT_TO_IRQ_ID(0x16); // Yosemite 32-bit 66MHz slot for GPU
    case IntSrc::PCI_J11    : return INT_TO_IRQ_ID(0x17); // Yosemite 64-bit 33MHz slot
    case IntSrc::PCI_J10    : return INT_TO_IRQ_ID(0x18); // Yosemite 64-bit 33MHz slot
    case IntSrc::PCI_J9     : return INT_TO_IRQ_ID(0x19); // Yosemite 64-bit 33MHz slot
    case IntSrc::ATA        : return INT_TO_IRQ_ID(0x1A); // Yosemite PCI pci-ata
    case IntSrc::USB        : return INT_TO_IRQ_ID(0x1C); // Yosemite PCI usb

    case IntSrc::ETHERNET   : return INT_TO_IRQ_ID(0x2A);

    default:
        ABORT_F("Heathrow: unknown interrupt source %d", src_id);
    }
    return 0;
}

uint64_t HeathrowIC::register_dma_int(IntSrc src_id)
{
    switch (src_id) {
    case IntSrc::DMA_SCSI_MESH      : return INT_TO_IRQ_ID(0x00);
    case IntSrc::DMA_SWIM3          : return INT_TO_IRQ_ID(0x01);
    case IntSrc::DMA_IDE0           : return INT_TO_IRQ_ID(0x02);
    case IntSrc::DMA_IDE1           : return INT_TO_IRQ_ID(0x03);
    case IntSrc::DMA_SCCA_Tx        : return INT_TO_IRQ_ID(0x04);
    case IntSrc::DMA_SCCA_Rx        : return INT_TO_IRQ_ID(0x05);
    case IntSrc::DMA_SCCB_Tx        : return INT_TO_IRQ_ID(0x06);
    case IntSrc::DMA_SCCB_Rx        : return INT_TO_IRQ_ID(0x07);
    case IntSrc::DMA_DAVBUS_Tx      : return INT_TO_IRQ_ID(0x08);
    case IntSrc::DMA_DAVBUS_Rx      : return INT_TO_IRQ_ID(0x09);
    case IntSrc::DMA_ETHERNET_Tx    : return INT_TO_IRQ_ID(0x20);
    case IntSrc::DMA_ETHERNET_Rx    : return INT_TO_IRQ_ID(0x21);
    default:
        ABORT_F("Heathrow: unknown DMA interrupt source %d", src_id);
    }
    return 0;
}

void HeathrowIC::ack_int_common(uint64_t irq_id, uint8_t irq_line_state)
{
    // native mode:   set IRQ bits in int_events on a 0-to-1 transition
    // emulated mode: set IRQ bits in int_events on all transitions
#if 0
    LOG_F(INTERRUPT, "%s: native interrupt events:%08x.%08x levels:%08x.%08x change:%08x.%08x state:%d",
        this->name.c_str(), uint32_t(this->int_events >> 32), uint32_t(this->int_events), uint32_t(this->int_levels >> 32),
        uint32_t(this->int_levels), uint32_t(irq_id >> 32), uint32_t(irq_id), irq_line_state
    );
#endif
    if ((this->int_mask & MACIO_INT_MODE) ||
        (irq_line_state && !(this->int_levels & irq_id))) {
        this->int_events |= irq_id;
    } else {
        this->int_events &= ~irq_id;
    }
    // update IRQ line state
    if (irq_line_state) {
        this->int_levels |= irq_id;
    } else {
        this->int_levels &= ~irq_id;
    }

    this->signal_cpu_int();
}

void HeathrowIC::ack_int(uint64_t irq_id, uint8_t irq_line_state)
{
    this->ack_int_common(irq_id, irq_line_state);
}

void HeathrowIC::ack_dma_int(uint64_t irq_id, uint8_t irq_line_state)
{
    this->ack_int_common(irq_id, irq_line_state);
}

void HeathrowIC::signal_cpu_int() {
    if (this->int_events & this->int_mask) {
        if (!this->cpu_int_latch) {
            this->cpu_int_latch = true;
            ppc_assert_int();
        } else {
            LOG_F(5, "%s: CPU INT already latched", this->name.c_str());
        }
    }
}

void HeathrowIC::clear_cpu_int()
{
    if (!(this->int_events & this->int_mask) && this->cpu_int_latch) {
        this->cpu_int_latch = false;
        ppc_release_int();
        LOG_F(5, "Heathrow: CPU INT latch cleared");
    }
}

static const vector<string> Heathrow_Subdevices = {
    "NVRAM", "ViaCuda", "ScsiMesh", "MeshHeathrow", "Escc", "Swim3", "Ide0", "Ide1",
    "BigMacHeathrow"
};

static const DeviceDescription Heathrow_Descriptor = {
    HeathrowIC::create, Heathrow_Subdevices, {}
};

REGISTER_DEVICE(Heathrow, Heathrow_Descriptor);
