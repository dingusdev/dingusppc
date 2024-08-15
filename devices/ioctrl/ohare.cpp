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

namespace loguru {
    enum : Verbosity {
        Verbosity_INTERRUPT = loguru::Verbosity_9,
        Verbosity_DBDMA = loguru::Verbosity_9
    };
}

OHare::OHare() : PCIDevice("mac-io_ohare"), InterruptCtrl()
{
    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::INT_CTRL);

    // populate my PCI config header
    this->vendor_id   = PCI_VENDOR_APPLE;
    this->device_id   = 0x0007;
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
    this->snd_codec = dynamic_cast<MacioSndCodec*>(gMachineObj->get_comp_by_type(HWCompType::SND_CODEC));
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
    this->mesh_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_SCSI_MESH));
    this->mesh_dma->connect(this->mesh);
    this->mesh->connect(this->mesh_dma.get());

    // connect IDE HW
    this->ide_0 = dynamic_cast<IdeChannel*>(gMachineObj->get_comp_by_name("Ide0"));
    //this->ide_1 = dynamic_cast<IdeChannel*>(gMachineObj->get_comp_by_name("Ide1"));

    // connect serial HW
    this->escc = dynamic_cast<EsccController*>(gMachineObj->get_comp_by_name("Escc"));

    // connect floppy disk HW and initialize its DMA channel
    this->swim3 = dynamic_cast<Swim3::Swim3Ctrl*>(gMachineObj->get_comp_by_name("Swim3"));
    this->floppy_dma = std::unique_ptr<DMAChannel> (new DMAChannel("floppy"));
    this->swim3->set_dma_channel(this->floppy_dma.get());
    this->floppy_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_SWIM3));

    // connect Ethernet HW
    //this->bmac = dynamic_cast<BigMac*>(gMachineObj->get_comp_by_type(HWCompType::ETHER_MAC));
    //this->enet_xmit_dma = std::unique_ptr<DMAChannel> (new DMAChannel("BmacTx"));
    //this->enet_rcv_dma  = std::unique_ptr<DMAChannel> (new DMAChannel("BmacRx"));

    // set EMMO pin status (active low) FIXME: Is there an emmo pin?
    //this->emmo_pin = GET_BIN_PROP("emmo") ^ 1;
}

void OHare::notify_bar_change(int bar_num)
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

uint32_t OHare::read(uint32_t rgn_start, uint32_t offset, int size)
{
    uint32_t value;

    LOG_F(9, "%s: read @%x.%c", this->get_name().c_str(),
        offset, SIZE_ARG(size));

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
    default:
        if (sub_addr >= 0x60) {
            value = this->nvram->read_byte((offset - 0x60000) >> 4);
        } else {
            value = 0;
            LOG_F(WARNING, "%s: read @%x.%c", this->get_name().c_str(),
                offset, SIZE_ARG(size));
        }
    }

    return value;
}

void OHare::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    LOG_F(9, "%s: write @%x.%c = %0*x", this->get_name().c_str(),
        offset, SIZE_ARG(size), size * 2, value);

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
    case 0x20: // IDE 0
        this->ide_0->write((offset >> 4) & 0x1F, value, size);
        break;
    default:
        if (sub_addr >= 0x60) {
            this->nvram->write_byte((offset - 0x60000) >> 4, value);
        } else {
            LOG_F(WARNING, "%s: write @%x.%c = %0*x", this->get_name().c_str(),
                offset, SIZE_ARG(size), size * 2, value);
        }
    }
}

uint32_t OHare::mio_ctrl_read(uint32_t offset, int size) {
    uint32_t value;

    switch (offset & 0xFC) {
    case MIO_INT_EVENTS1:
        value = this->int_events;
        break;
    case MIO_INT_MASK1:
        value = this->int_mask;
        break;
    case MIO_INT_LEVELS1:
        value = this->int_levels;
        break;
    case MIO_OHARE_ID: // FIXME: HACK: no clue what this register is supposed to contain
        value = ~0x00004000; // 1<<14
        LOG_F(ERROR, "%s: read  OHARE_ID @%02x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value);
        break;
    case MIO_OHARE_FEAT_CTRL:
        value = this->feat_ctrl;
        LOG_F(WARNING, "%s: read  FEAT_CTRL @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value);
        break;
    default:
        value = 0;
        LOG_F(WARNING, "%s: read @%02x.%c",
            this->get_name().c_str(), offset, SIZE_ARG(size));
    }

    return BYTESWAP_32(value);
}

void OHare::mio_ctrl_write(uint32_t offset, uint32_t value, int size) {
    switch (offset) {
    case MIO_INT_MASK1:
        this->int_mask = BYTESWAP_32(value);
        LOG_F(INTERRUPT, "%s: int_mask:0x%08x", name.c_str(), this->int_mask);
        this->signal_cpu_int();
        break;
    case MIO_INT_CLEAR1:
        if ((this->int_mask & MACIO_INT_MODE) && (value & MACIO_INT_CLR)) {
            this->int_events = 0;
        } else {
            this->int_events &= ~(BYTESWAP_32(value) & 0x7FFFFFFFUL);
        }
        clear_cpu_int();
        break;
    case MIO_INT_LEVELS1:
        LOG_F(INTERRUPT, "%s: write INT_LEVELS1 @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value); // writing 0x100000 happens often
        break;
    case MIO_OHARE_ID:
        LOG_F(ERROR, "%s: write OHARE_ID @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value);
        break;
    case MIO_OHARE_FEAT_CTRL:
        LOG_F(WARNING, "%s: write FEAT_CTRL @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value);
        this->feature_control(BYTESWAP_32(value));
        break;
    default:
        LOG_F(WARNING, "%s: write @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value);
    }
}

uint32_t OHare::dma_read(uint32_t offset, int size)
{
    uint32_t value;
    int dma_channel = offset >> 8;
    switch (dma_channel) {
    case MIO_OHARE_DMA_MESH:
        if (this->mesh_dma)
            value = this->mesh_dma->reg_read(offset & 0xFF, size);
        else
            value = 0;
        break;
    case MIO_OHARE_DMA_FLOPPY:
        value = this->floppy_dma->reg_read(offset & 0xFF, size);
        break;
    case MIO_OHARE_DMA_AUDIO_OUT:
        value = this->snd_out_dma->reg_read(offset & 0xFF, size);
        break;
    default:
        if (!(unsupported_dma_channel_read & (1 << dma_channel))) {
            unsupported_dma_channel_read |= (1 << dma_channel);
            return 0;
        }
        value = 0;
    }
    return value;
}

void OHare::dma_write(uint32_t offset, uint32_t value, int size)
{
    int dma_channel = offset >> 8;

    switch (dma_channel) {
    case MIO_OHARE_DMA_MESH:
        if (this->mesh_dma) this->mesh_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_FLOPPY:
        this->floppy_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_AUDIO_OUT:
        this->snd_out_dma->reg_write(offset & 0xFF, value, size);
        break;
    default:
        LOG_F(WARNING, "OHare: unsupported DMA channel write, offset=0x%X, val=0x%X", offset, value);
    }
}

void OHare::feature_control(const uint32_t value)
{
    LOG_F(9, "write %x to MIO:Feat_Ctrl register", value);

    this->feat_ctrl = value;

    if (!(this->feat_ctrl & 1)) {
        LOG_F(9, "%s: Monitor sense enabled", this->get_name().c_str());
    } else {
        LOG_F(9, "%s: Monitor sense disabled", this->get_name().c_str());
    }
}

#define INT_TO_IRQ_ID(intx) (1 << intx)

uint64_t OHare::register_dev_int(IntSrc src_id)
{
    switch (src_id) {
    case IntSrc::SCSI_MESH  : return INT_TO_IRQ_ID(0x0C);
    case IntSrc::IDE0       : return INT_TO_IRQ_ID(0x0D);
//  case IntSrc::IDE1       : return INT_TO_IRQ_ID(0x0E);
    case IntSrc::SCCA       : return INT_TO_IRQ_ID(0x0F);
    case IntSrc::SCCB       : return INT_TO_IRQ_ID(0x10);
    case IntSrc::DAVBUS     : return INT_TO_IRQ_ID(0x11);
    case IntSrc::VIA_CUDA   : return INT_TO_IRQ_ID(0x12);
    case IntSrc::SWIM3      : return INT_TO_IRQ_ID(0x13);
//  case IntSrc::NMI        : return INT_TO_IRQ_ID(0x14);
//  case IntSrc::EXT1       : return INT_TO_IRQ_ID(0x15);

    case IntSrc::BANDIT1    : return INT_TO_IRQ_ID(0x16);
    case IntSrc::PCI_E      : return INT_TO_IRQ_ID(0x16); // same interrupt as bandit
    case IntSrc::PCI_A      : return INT_TO_IRQ_ID(0x17);
    case IntSrc::PCI_F      : return INT_TO_IRQ_ID(0x18);
    case IntSrc::PCI_B      : return INT_TO_IRQ_ID(0x19);
//  case IntSrc::???        : return INT_TO_IRQ_ID(0x1A);
//  case IntSrc::???        : return INT_TO_IRQ_ID(0x1B);
    case IntSrc::PCI_C      : return INT_TO_IRQ_ID(0x1C);

    default:
        ABORT_F("%s: unknown interrupt source %d", this->name.c_str(), src_id);
    }
    return 0;
}

uint64_t OHare::register_dma_int(IntSrc src_id)
{
    switch (src_id) {
    case IntSrc::DMA_SCSI_MESH: return INT_TO_IRQ_ID(0x00);
    case IntSrc::DMA_SWIM3    : return INT_TO_IRQ_ID(0x01);
    case IntSrc::DMA_IDE0     : return INT_TO_IRQ_ID(0x02);
    //
    case IntSrc::DMA_SCCA_Tx  : return INT_TO_IRQ_ID(0x04);
    case IntSrc::DMA_SCCA_Rx  : return INT_TO_IRQ_ID(0x05);
    case IntSrc::DMA_SCCB_Tx  : return INT_TO_IRQ_ID(0x06);
    case IntSrc::DMA_SCCB_Rx  : return INT_TO_IRQ_ID(0x07);
    case IntSrc::DMA_DAVBUS_Tx: return INT_TO_IRQ_ID(0x08);
    case IntSrc::DMA_DAVBUS_Rx: return INT_TO_IRQ_ID(0x09);
    default:
        ABORT_F("%s: unknown DMA interrupt source %d", this->name.c_str(), src_id);
    }
    return 0;
}

void OHare::ack_int_common(uint64_t irq_id, uint8_t irq_line_state)
{
    // native mode:   set IRQ bits in int_events on a 0-to-1 transition
    // emulated mode: set IRQ bits in int_events on all transitions
#if 0
    LOG_F(INTERRUPT, "%s: native interrupt mask:%08x events:%08x levels:%08x change:%08x state:%d",
        this->name.c_str(), this->int_mask, this->int_events + 0, this->int_levels + 0, irq_id, irq_line_state
    );
#endif
    if ((this->int_mask & MACIO_INT_MODE) ||
        (irq_line_state && !(this->int_levels & irq_id))) {
        this->int_events |= (uint32_t)irq_id;
    } else {
        this->int_events &= ~(uint32_t)irq_id;
    }
    // update IRQ line state
    if (irq_line_state) {
        this->int_levels |= (uint32_t)irq_id;
    } else {
        this->int_levels &= ~(uint32_t)irq_id;
    }

    this->signal_cpu_int();
}

void OHare::ack_int(uint64_t irq_id, uint8_t irq_line_state)
{
    this->ack_int_common(irq_id, irq_line_state);
}

void OHare::ack_dma_int(uint64_t irq_id, uint8_t irq_line_state)
{
    this->ack_int_common(irq_id, irq_line_state);
}

void OHare::signal_cpu_int() {
    if (this->int_events & this->int_mask) {
        if (!this->cpu_int_latch) {
            this->cpu_int_latch = true;
            ppc_assert_int();
        } else {
            LOG_F(5, "%s: CPU INT already latched", this->name.c_str());
        }
    }
}

void OHare::clear_cpu_int()
{
    if (!(this->int_events & this->int_mask) && this->cpu_int_latch) {
        this->cpu_int_latch = false;
        ppc_release_int();
        LOG_F(5, "%s: CPU INT latch cleared", this->name.c_str());
    }
}

static const std::vector<std::string> OHare_Subdevices = {
    "ViaCuda", "ScsiMesh", "MeshHeathrow", "Escc", "Swim3", "Ide0"
};

static const DeviceDescription OHare_Descriptor = {
    OHare::create, OHare_Subdevices, {
}};

REGISTER_DEVICE(OHare, OHare_Descriptor);
