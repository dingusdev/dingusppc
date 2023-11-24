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

HeathrowIC::HeathrowIC() : PCIDevice("mac-io/heathrow"), InterruptCtrl()
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
        LOG_F(INFO, "%s: base address set to 0x%X", this->pci_name.c_str(), this->base_addr);
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
    case MIO_OHARE_DMA_AUDIO_OUT:
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
    case 0x10: // SCSI
        res = this->mesh->read((offset >> 4) & 0xF);
        break;
    case 0x11: // Ethernet
        res = BYTESWAP_SIZED(this->bmac->read(offset & 0xFFFU), size);
        break;
    case 0x12: // ESCC compatible addressing
        if ((offset & 0xFF) < 16) {
            return this->escc->read(compat_to_macrisc[(offset >> 1) & 0xF]);
        }
        // fallthrough
    case 0x13: // ESCC MacRISC addressing
        return this->escc->read((offset >> 4) & 0xF);
    case 0x14:
        res = this->snd_codec->snd_ctrl_read(offset - 0x14000, size);
        break;
    case 0x15: // SWIM3
        return this->swim3->read((offset >> 4 )& 0xF);
    case 0x16: // VIA-CUDA
    case 0x17:
        res = this->viacuda->read((offset - 0x16000) >> 9);
        break;
    case 0x20: // IDE 0
        res = this->ide_0->read((offset >> 4) & 0x1F, size);
        break;
    case 0x21: // IDE 1
        res = this->ide_1->read((offset >> 4) & 0x1F, size);
        break;
    default:
        if (sub_addr >= 0x60) {
            res = this->nvram->read_byte((offset - 0x60000) >> 4);
        } else {
            LOG_F(WARNING, "Attempting to read from unmapped I/O space: %x", offset);
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
    case 0x10: // SCSI
        this->mesh->write((offset >> 4) & 0xF, value);
        break;
    case 0x11: // Ethernet
        this->bmac->write(offset & 0xFFFU, BYTESWAP_SIZED(value, size));
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
        this->snd_codec->snd_ctrl_write(offset - 0x14000, value, size);
        break;
    case 0x15: // SWIM3
        this->swim3->write((offset >> 4) & 0xF, value);
        break;
    case 0x16: // VIA-CUDA
    case 0x17:
        this->viacuda->write((offset - 0x16000) >> 9, value);
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
    case MIO_INT_CLEAR1:
    case MIO_INT_CLEAR2:
        // some Mac OS drivers reads from those write-only registers
        // so we return zero here as real HW does
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
        this->int_events2 &= ~(BYTESWAP_32(value) & 0x7FFFFFFFUL);
        clear_cpu_int();
        break;
    case MIO_INT_MASK1:
        this->int_mask1 = BYTESWAP_32(value);
        // copy IntMode bit to InterruptMask2 register
        this->int_mask2 = (this->int_mask2 & ~MACIO_INT_MODE) | (this->int_mask1 & MACIO_INT_MODE);
        break;
    case MIO_INT_CLEAR1:
        if ((this->int_mask1 & MACIO_INT_MODE) && (value & MACIO_INT_CLR)) {
            this->int_events1 = 0;
            this->int_events2 = 0;
        } else {
            this->int_events1 &= ~(BYTESWAP_32(value) & 0x7FFFFFFFUL);
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

#define FIRST_INT1_BIT 12 // The first ten are DMA, the next 2 appear to be unused. We'll map 1:1 the INT1 bits 31..12 (0x1F..0x0C) as IRQ_ID bits.
#define FIRST_INT2_BIT  2 // Skip the first two which are Ethernet DMA. We'll map INT2 bits 13..2 (interrupts 45..34 or 0x2D..0x22) as IRQ_ID bits 11..0.
#define FIRST_INT1_IRQ_ID_BIT 12 // Same as INT1_BIT so there won't be any shifting required.
#define FIRST_INT2_IRQ_ID_BIT  0

#define INT1_TO_IRQ_ID(int1) (1 << (int1 - FIRST_INT1_BIT + FIRST_INT1_IRQ_ID_BIT))
#define INT2_TO_IRQ_ID(int2) (1 << (int2 - FIRST_INT2_BIT + FIRST_INT2_IRQ_ID_BIT - 32))
#define INT_TO_IRQ_ID(intx) (intx < 32 ? INT1_TO_IRQ_ID(intx) : INT2_TO_IRQ_ID(intx))

#define IS_INT1(irq_id) (irq_id >= 1 << FIRST_INT1_IRQ_ID_BIT)
#define IRQ_ID_TO_INT1_MASK(irq_id) (irq_id <<= (FIRST_INT1_BIT - FIRST_INT1_IRQ_ID_BIT))
#define IRQ_ID_TO_INT2_MASK(irq_id) (irq_id <<= (FIRST_INT2_BIT - FIRST_INT2_IRQ_ID_BIT))

uint32_t HeathrowIC::register_dev_int(IntSrc src_id)
{
    switch (src_id) {
    case IntSrc::SCSI_MESH  : return INT_TO_IRQ_ID(0x0C);
    case IntSrc::IDE0       : return INT_TO_IRQ_ID(0x0D);
    case IntSrc::IDE1       : return INT_TO_IRQ_ID(0x0E);
    case IntSrc::SCCA       : return INT_TO_IRQ_ID(0x0F);
    case IntSrc::SCCB       : return INT_TO_IRQ_ID(0x10);
    case IntSrc::DAVBUS     : return INT_TO_IRQ_ID(0x11);
    case IntSrc::VIA_CUDA   : return INT_TO_IRQ_ID(0x12);
    case IntSrc::SWIM3      : return INT_TO_IRQ_ID(0x13);
    case IntSrc::NMI        : return INT_TO_IRQ_ID(0x14); // nmiSource in AppleHeathrow/Heathrow.cpp
    case IntSrc::PERCH2     : return INT_TO_IRQ_ID(0x15);
    case IntSrc::PCI_GPU    : return INT_TO_IRQ_ID(0x16);
    case IntSrc::PCI_A      : return INT_TO_IRQ_ID(0x17);
    case IntSrc::PCI_B      : return INT_TO_IRQ_ID(0x18);
    case IntSrc::PCI_C      : return INT_TO_IRQ_ID(0x19);
    case IntSrc::PERCH1     : return INT_TO_IRQ_ID(0x1A);
    case IntSrc::PCI_PERCH  : return INT_TO_IRQ_ID(0x1C);
    case IntSrc::ETHERNET   : return INT_TO_IRQ_ID(0x2A);
    default:
        ABORT_F("Heathrow: unknown interrupt source %d", src_id);
    }
    return 0;
}

#define FIRST_DMA_INT1_BIT  0 // bit 0 is SCSI DMA
#define FIRST_DMA_INT2_BIT  0 // bit 0 is Ethernet DMA Tx
#define FIRST_DMA_INT1_IRQ_ID_BIT  0
#define FIRST_DMA_INT2_IRQ_ID_BIT 16 // There's only 10 INT1 DMA bits but we'll put INT2 DMA bits in the upper 16 bits

#define DMA_INT1_TO_IRQ_ID(int1) (1 << (int1 - FIRST_DMA_INT1_BIT + FIRST_DMA_INT1_IRQ_ID_BIT))
#define DMA_INT2_TO_IRQ_ID(int2) (1 << (int2 - FIRST_DMA_INT2_BIT + FIRST_DMA_INT2_IRQ_ID_BIT - 32))
#define DMA_INT_TO_IRQ_ID(intx) (intx < 32 ? DMA_INT1_TO_IRQ_ID(intx) : DMA_INT2_TO_IRQ_ID(intx))

#define IS_DMA_INT1(irq_id) (irq_id < 1 << FIRST_DMA_INT2_IRQ_ID_BIT)
#define DMA_IRQ_ID_TO_INT1_MASK(irq_id) (irq_id <<= (FIRST_DMA_INT1_BIT - FIRST_DMA_INT1_IRQ_ID_BIT))
#define DMA_IRQ_ID_TO_INT2_MASK(irq_id) (irq_id >>= (FIRST_DMA_INT2_IRQ_ID_BIT - FIRST_DMA_INT2_BIT))

uint32_t HeathrowIC::register_dma_int(IntSrc src_id)
{
    switch (src_id) {
    case IntSrc::DMA_SCSI_MESH      : return DMA_INT_TO_IRQ_ID(0x00);
    case IntSrc::DMA_SWIM3          : return DMA_INT_TO_IRQ_ID(0x01);
    case IntSrc::DMA_IDE0           : return DMA_INT_TO_IRQ_ID(0x02);
    case IntSrc::DMA_IDE1           : return DMA_INT_TO_IRQ_ID(0x03);
    case IntSrc::DMA_SCCA_Tx        : return DMA_INT_TO_IRQ_ID(0x04);
    case IntSrc::DMA_SCCA_Rx        : return DMA_INT_TO_IRQ_ID(0x05);
    case IntSrc::DMA_SCCB_Tx        : return DMA_INT_TO_IRQ_ID(0x06);
    case IntSrc::DMA_SCCB_Rx        : return DMA_INT_TO_IRQ_ID(0x07);
    case IntSrc::DMA_DAVBUS_Tx      : return DMA_INT_TO_IRQ_ID(0x08);
    case IntSrc::DMA_DAVBUS_Rx      : return DMA_INT_TO_IRQ_ID(0x09);
    case IntSrc::DMA_ETHERNET_Tx    : return DMA_INT_TO_IRQ_ID(0x20);
    case IntSrc::DMA_ETHERNET_Rx    : return DMA_INT_TO_IRQ_ID(0x21);
    default:
        ABORT_F("Heathrow: unknown DMA interrupt source %d", src_id);
    }
    return 0;
}

void HeathrowIC::ack_int(uint32_t irq_id, uint8_t irq_line_state)
{
#if 1
    if (!IS_INT1(irq_id)) { // does this irq_id belong to the second set?
        IRQ_ID_TO_INT2_MASK(irq_id);
#if 0
        LOG_F(INFO, "%s: native interrupt events:%08x.%08x levels:%08x.%08x change2:%08x state:%d",
            this->name.c_str(), this->int_events1 + 0, this->int_events2 + 0, this->int_levels1 + 0, this->int_levels2 + 0, irq_id, irq_line_state
        );
#endif
        // native mode:   set IRQ bits in int_events2 on a 0-to-1 transition
        // emulated mode: set IRQ bits in int_events2 on all transitions
        if ((this->int_mask1 & MACIO_INT_MODE) ||
            (irq_line_state && !(this->int_levels2 & irq_id))) {
            this->int_events2 |= irq_id;
        } else {
            this->int_events2 &= ~irq_id;
        }
        this->int_events2 &= this->int_mask2;
        // update IRQ line state
        if (irq_line_state) {
            this->int_levels2 |= irq_id;
        } else {
            this->int_levels2 &= ~irq_id;
        }
    } else {
        IRQ_ID_TO_INT1_MASK(irq_id);
        // native mode:   set IRQ bits in int_events1 on a 0-to-1 transition
        // emulated mode: set IRQ bits in int_events1 on all transitions
#if 0
        LOG_F(INFO, "%s: native interrupt events:%08x.%08x levels:%08x.%08x change1:%08x state:%d",
            this->name.c_str(), this->int_events1 + 0, this->int_events2 + 0, this->int_levels1 + 0, this->int_levels2 + 0, irq_id, irq_line_state);
#endif
        if ((this->int_mask1 & MACIO_INT_MODE) ||
            (irq_line_state && !(this->int_levels1 & irq_id))) {
            this->int_events1 |= irq_id;
        } else {
            this->int_events1 &= ~irq_id;
        }
        this->int_events1 &= this->int_mask1;
        // update IRQ line state
        if (irq_line_state) {
            this->int_levels1 |= irq_id;
        } else {
            this->int_levels1 &= ~irq_id;
        }
    }

    this->signal_cpu_int();
#endif

#if 0
    if (this->int_mask1 & MACIO_INT_MODE) { // 68k interrupt emulation mode?
        if (!IS_INT1(irq_id)) {
            IRQ_ID_TO_INT2_MASK(irq_id);
            this->int_events2 |= irq_id; // signal IRQ line change
            this->int_events2 &= this->int_mask2;
            // update IRQ line state
            if (irq_line_state) {
                this->int_levels2 |= irq_id;
            } else {
                this->int_levels2 &= ~irq_id;
            }
        } else {
            IRQ_ID_TO_INT1_MASK(irq_id);
            this->int_events1 |= irq_id; // signal IRQ line change
            this->int_events1 &= this->int_mask1;
            // update IRQ line state
            if (irq_line_state) {
                this->int_levels1 |= irq_id;
            } else {
                this->int_levels1 &= ~irq_id;
            }
        }
        this->signal_cpu_int();
    } else {
        LOG_F(WARNING, "%s: native interrupt mode not implemented", this->name.c_str());
    }
#endif
}

void HeathrowIC::ack_dma_int(uint32_t irq_id, uint8_t irq_line_state)
{
#if 1
    if (!IS_DMA_INT1(irq_id)) {
        DMA_IRQ_ID_TO_INT2_MASK(irq_id);
        // native mode:   set IRQ bits in int_events2 on a 0-to-1 transition
        // emulated mode: set IRQ bits in int_events2 on all transitions
        if ((this->int_mask1 & MACIO_INT_MODE) ||
            (irq_line_state && !(this->int_levels2 & irq_id))) {
            this->int_events2 |= irq_id;
        } else {
            this->int_events2 &= ~irq_id;
        }
        this->int_events2 &= this->int_mask2;
        // update IRQ line state
        if (irq_line_state) {
            this->int_levels2 |= irq_id;
        } else {
            this->int_levels2 &= ~irq_id;
        }
    } else {
        DMA_IRQ_ID_TO_INT1_MASK(irq_id);
        // native mode:   set IRQ bits in int_events1 on a 0-to-1 transition
        // emulated mode: set IRQ bits in int_events1 on all transitions
        if ((this->int_mask1 & MACIO_INT_MODE) ||
            (irq_line_state && !(this->int_levels1 & irq_id))) {
            this->int_events1 |= irq_id;
        } else {
            this->int_events1 &= ~irq_id;
        }
        this->int_events1 &= this->int_mask1;
        // update IRQ line state
        if (irq_line_state) {
            this->int_levels1 |= irq_id;
        } else {
            this->int_levels1 &= ~irq_id;
        }
    }

    this->signal_cpu_int();
#endif

#if 0
    if (this->int_mask1 & MACIO_INT_MODE) { // 68k interrupt emulation mode?
        if (!IS_DMA_INT1(irq_id)) {
            DMA_IRQ_ID_TO_INT2_MASK(irq_id);
            this->int_events2 |= irq_id; // signal IRQ line change
            this->int_events2 &= this->int_mask2;
            // update IRQ line state
            if (irq_line_state) {
                this->int_levels2 |= irq_id;
            } else {
                this->int_levels2 &= ~irq_id;
            }
        } else {
            DMA_IRQ_ID_TO_INT1_MASK(irq_id);
            this->int_events1 |= irq_id; // signal IRQ line change
            this->int_events1 &= this->int_mask1;
            // update IRQ line state
            if (irq_line_state) {
                this->int_levels1 |= irq_id;
            } else {
                this->int_levels1 &= ~irq_id;
            }
        }
        this->signal_cpu_int();
    } else {
        ABORT_F("%s: native interrupt mode not implemented", this->name.c_str());
    }
#endif
}

void HeathrowIC::signal_cpu_int() {
    if (this->int_events1 || this->int_events2) {
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
    if (!this->int_events1 && !this->int_events2) {
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
