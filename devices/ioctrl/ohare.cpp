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
#include <devices/ioctrl/macio.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <machines/machinebase.h>

#include <cinttypes>

OHare::OHare() : PCIDevice("mac-io/ohare"), InterruptCtrl()
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

    // initialize sound chip and its DMA output channel, then wire them together
    this->awacs       = std::unique_ptr<AwacsScreamer> (new AwacsScreamer());
    this->snd_out_dma = std::unique_ptr<DMAChannel> (new DMAChannel());
    this->awacs->set_dma_out(this->snd_out_dma.get());
    this->snd_out_dma->set_callbacks(
        std::bind(&AwacsScreamer::dma_start, this->awacs.get()),
        std::bind(&AwacsScreamer::dma_end, this->awacs.get())
    );

    // connect serial HW
    this->escc = dynamic_cast<EsccController*>(gMachineObj->get_comp_by_name("Escc"));
}

void OHare::notify_bar_change(int bar_num)
{
    if (bar_num) // only BAR0 is supported
        return;

    if (this->base_addr != (this->bars[bar_num] & 0xFFFFFFF0UL)) {
        if (this->base_addr) {
            LOG_F(WARNING, "%s: deallocating I/O memory not implemented",
                this->pci_name.c_str());
        }
        this->base_addr = this->bars[0] & 0xFFFFFFF0UL;
        this->host_instance->pci_register_mmio_region(this->base_addr, 0x80000, this);
        LOG_F(INFO, "%s: base address set to 0x%X", this->pci_name.c_str(), this->base_addr);
    }
}

uint32_t OHare::read(uint32_t rgn_start, uint32_t offset, int size)
{
    unsigned sub_addr = (offset >> 12) & 0x7F;

    switch (sub_addr) {
    case 0:
        return read_ctrl(offset, size);
    case 8:
        return dma_read(offset & 0x7FFF, size);
    case 0x13: // ESCC MacRISC addressing
        return this->escc->read((offset >> 4) & 0xF);
    case 0x14: // AWACS
        return this->awacs->snd_ctrl_read(offset & 0xFF, size);
    case 0x16:
    case 0x17:
        return this->viacuda->read((offset >> 9) & 0xF);
    case 0x20:
        return 0xFF7FU;
    default:
        if (sub_addr >= 0x60) {
            return this->nvram->read_byte((offset - 0x60000) >> 4);
        } else {
            LOG_F(WARNING, "OHare: read from unimplemented register 0x%X",
                this->base_addr + offset);
        }
    }

    return 0xFFFFFFFFUL;
}

void OHare::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    unsigned sub_addr = (offset >> 12) & 0x7F;

    switch (sub_addr) {
    case 0:
        this->write_ctrl(offset, value, size);
        break;
    case 8:
        this->dma_write(offset & 0x7FFF, value, size);
        break;
    case 0x13: // ESCC MacRISC addressing
        this->escc->write((offset >> 4) & 0xF, value);
        break;
    case 0x14: // AWACS
        this->awacs->snd_ctrl_write(offset & 0xFF, value, size);
        break;
    case 0x16:
    case 0x17: // VIA-CUDA
        this->viacuda->write((offset >> 9) & 0xF, value);
        break;
    case 0x20:
        LOG_F(INFO, "OHare: write to IDE0 register at 0x%X", offset);
        break;
    default:
        if (sub_addr >= 0x60) {
            this->nvram->write_byte((offset - 0x60000) >> 4, value);
        } else {
            LOG_F(WARNING, "OHare: writing to unimplemented device register 0x%X",
                this->base_addr + offset);
        }
    }
}

uint32_t OHare::read_ctrl(uint32_t offset, int size)
{
    uint32_t res = 0;

    switch (offset & 0xFC) {
    case MIO_INT_EVENTS1:
        res = this->int_events;
        break;
    case MIO_OHARE_ID: // HACK: no clue what this register is supposed to contain
        res = 0xFFFFBFFFUL;
    default:
        LOG_F(WARNING, "OHare: read from unknown control register at %x", offset);
        break;
    }

    return BYTESWAP_32(res);
}

void OHare::write_ctrl(uint32_t offset, uint32_t value, int size)
{
    switch (offset) {
    case MIO_INT_MASK1:
        this->int_mask = BYTESWAP_32(value);
        break;
    case MIO_INT_CLEAR1:
        if (value & MACIO_INT_CLR) {
            this->int_events    = 0;
            this->cpu_int_latch = false;
            ppc_release_int();
            LOG_F(5, "OHare: CPU INT latch cleared");
        } else {
            this->int_events &= BYTESWAP_32(value);
        }
        break;
    default:
        LOG_F(WARNING, "OHare: writing to unimplemented control register 0x%X",
             this->base_addr + offset);
    }
}

uint32_t OHare::dma_read(uint32_t offset, int size)
{
    switch (offset >> 8) {
    case 8:
        return this->snd_out_dma->reg_read(offset & 0xFF, size);
    default:
        LOG_F(WARNING, "OHare: unsupported DMA channel read, offset=0x%X", offset);
    }

    return 0xFFFFFFUL;
}

void OHare::dma_write(uint32_t offset, uint32_t value, int size)
{
    switch (offset >> 8) {
    case 8:
        this->snd_out_dma->reg_write(offset & 0xFF, value, size);
        break;
    default:
        LOG_F(WARNING, "OHare: unsupported DMA channel write, offset=0x%X, val=0x%X", offset, value);
    }
}

uint32_t OHare::register_dev_int(IntSrc src_id)
{
    return 0;
}

uint32_t OHare::register_dma_int(IntSrc src_id)
{
    return 0;
}

void OHare::ack_int(uint32_t irq_id, uint8_t irq_line_state)
{

}

void OHare::ack_dma_int(uint32_t irq_id, uint8_t irq_line_state)
{

}

static const vector<string> OHare_Subdevices = {
    "NVRAM", "ViaCuda", "Mesh", "Escc", "Swim3"
};

static const DeviceDescription OHare_Descriptor = {
    OHare::create, OHare_Subdevices, {}
};

REGISTER_DEVICE(OHare, OHare_Descriptor);
