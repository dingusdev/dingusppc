/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 divingkatae and maximum
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
#include <devices/common/hwcomponent.h>
#include <devices/common/viacuda.h>
#include <devices/ioctrl/macio.h>
#include <loguru.hpp>
#include <machines/machinebase.h>

#include <cinttypes>
#include <functional>
#include <memory>

MacIoBase::MacIoBase(std::string name, uint16_t dev_id, uint8_t rev) :
    PCIDevice("mac-io_" + name), InterruptCtrl()
{
    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::INT_CTRL);

    // populate my PCI config header
    this->vendor_id   = PCI_VENDOR_APPLE;
    this->device_id   = dev_id;
    this->class_rev   = 0xFF000000 + rev;
    this->cache_ln_sz = 8;

    // select memory mapped I/O size: 128Kb for Grand Central, 512Kb for others
    this->iomem_size = (dev_id == MIO_DEV_ID_GRANDCENTRAL) ? 0x20000 : 0x80000;

    this->setup_bars({{0, -this->iomem_size}});

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };

    // connect Cuda
    this->viacuda = dynamic_cast<ViaCuda*>(gMachineObj->get_comp_by_name("ViaCuda"));

    // find the sound codec, create its DMA channels, then wire everything together
    this->snd_codec   = dynamic_cast<MacioSndCodec*>(gMachineObj->get_comp_by_type(HWCompType::SND_CODEC));
    this->snd_out_dma = std::unique_ptr<DMAChannel> (new DMAChannel("snd_out"));
    this->snd_out_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_DAVBUS_Tx));
    this->snd_codec->set_dma_out(this->snd_out_dma.get());
    this->snd_out_dma->set_callbacks(
        std::bind(&AwacsScreamer::dma_out_start, this->snd_codec),
        std::bind(&AwacsScreamer::dma_out_stop, this->snd_codec)
    );
    this->snd_in_dma = std::unique_ptr<DMAChannel> (new DMAChannel("snd_in"));
    this->snd_in_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_DAVBUS_Rx));
    this->snd_codec->set_dma_in(this->snd_in_dma.get());
    this->snd_in_dma->set_callbacks(
        std::bind(&AwacsScreamer::dma_in_start, this->snd_codec),
        std::bind(&AwacsScreamer::dma_in_stop, this->snd_codec)
    );

    // connect floppy disk controller and initialize its DMA channel
    this->swim3 = dynamic_cast<Swim3::Swim3Ctrl*>(gMachineObj->get_comp_by_name("Swim3"));
    this->floppy_dma = std::unique_ptr<DMAChannel> (new DMAChannel("floppy"));
    this->swim3->set_dma_channel(this->floppy_dma.get());
    this->floppy_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_SWIM3));

    // connect serial HW to its DMA channels
    this->escc = dynamic_cast<EsccController*>(gMachineObj->get_comp_by_name("Escc"));
    this->escc_a_tx_dma = std::unique_ptr<DMAChannel> (new DMAChannel("Escc_a_tx"));
    this->escc_a_rx_dma = std::unique_ptr<DMAChannel> (new DMAChannel("Escc_a_rx"));
    this->escc_b_tx_dma = std::unique_ptr<DMAChannel> (new DMAChannel("Escc_b_tx"));
    this->escc_b_rx_dma = std::unique_ptr<DMAChannel> (new DMAChannel("Escc_b_rx"));
    this->escc_a_tx_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_SCCA_Tx));
    this->escc_a_rx_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_SCCA_Rx));
    this->escc_b_tx_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_SCCB_Tx));
    this->escc_b_rx_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_SCCB_Rx));
    this->escc->set_dma_channel(0, this->escc_a_tx_dma.get());
    this->escc->set_dma_channel(1, this->escc_a_rx_dma.get());
    this->escc->set_dma_channel(2, this->escc_b_tx_dma.get());
    this->escc->set_dma_channel(3, this->escc_b_rx_dma.get());
}

void MacIoBase::notify_bar_change(int bar_num) {
    if (bar_num) // only BAR0 is supported
        return;

    if (this->base_addr != (this->bars[bar_num] & 0xFFFFFFF0UL)) {
        if (this->base_addr) {
            this->host_instance->pci_unregister_mmio_region(this->base_addr,
                this->iomem_size, this);
        }
        this->base_addr = this->bars[0] & 0xFFFFFFF0UL;
        this->host_instance->pci_register_mmio_region(this->base_addr, this->iomem_size, this);
        LOG_F(INFO, "%s: base address set to 0x%X", this->name.c_str(), this->base_addr);
    }
}

uint64_t MacIoBase::register_dma_int(IntSrc src_id) {
    switch (src_id) {
    case IntSrc::DMA_SWIM3          : return INT_TO_IRQ_ID(0x01);
    case IntSrc::DMA_SCCA_Tx        : return INT_TO_IRQ_ID(0x04);
    case IntSrc::DMA_SCCA_Rx        : return INT_TO_IRQ_ID(0x05);
    case IntSrc::DMA_SCCB_Tx        : return INT_TO_IRQ_ID(0x06);
    case IntSrc::DMA_SCCB_Rx        : return INT_TO_IRQ_ID(0x07);
    case IntSrc::DMA_DAVBUS_Tx      : return INT_TO_IRQ_ID(0x08);
    case IntSrc::DMA_DAVBUS_Rx      : return INT_TO_IRQ_ID(0x09);
    default:
        ABORT_F("%s: unknown DMA interrupt source %d", this->name.c_str(), src_id);
    }
    return 0;
}

void MacIoBase::ack_int_common(uint64_t irq_id, uint8_t irq_line_state) {
    // native mode:   set IRQ bits in int_events on a 0-to-1 transition
    // emulated mode: set IRQ bits in int_events on all transitions
#if 0
    LOG_F(INTERRUPT, "%s: native interrupt events:%08x.%08x levels:%08x.%08x change:%08x.%08x state:%d",
        this->name.c_str(), uint32_t(this->int_events >> 32), uint32_t(this->int_events),
        uint32_t(this->int_levels >> 32), uint32_t(this->int_levels),
        uint32_t(irq_id >> 32), uint32_t(irq_id), irq_line_state
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

void MacIoBase::ack_int(uint64_t irq_id, uint8_t irq_line_state) {
    this->ack_int_common(irq_id, irq_line_state);
}

void MacIoBase::ack_dma_int(uint64_t irq_id, uint8_t irq_line_state) {
    this->ack_int_common(irq_id, irq_line_state);
}

void MacIoBase::signal_cpu_int() {
    if (this->int_events & this->int_mask) {
        if (!this->cpu_int_latch) {
            this->cpu_int_latch = true;
            ppc_assert_int();
        } else {
            LOG_F(5, "%s: CPU INT already latched", this->name.c_str());
        }
    }
}

void MacIoBase::clear_cpu_int() {
    if (!(this->int_events & this->int_mask) && this->cpu_int_latch) {
        this->cpu_int_latch = false;
        ppc_release_int();
        LOG_F(5, "%s: CPU INT latch cleared", this->name.c_str());
    }
}
