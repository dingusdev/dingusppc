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

#include <devices/ethernet/mace.h>
#include <devices/ioctrl/macio.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <machines/machinebase.h>

#include <cinttypes>
#include <memory>

GrandCentral::GrandCentral() : PCIDevice("mac-io/grandcentral"), InterruptCtrl()
{
    supports_types(HWCompType::MMIO_DEV | HWCompType::INT_CTRL);

    // populate my PCI config header
    this->vendor_id   = PCI_VENDOR_APPLE;
    this->device_id   = 0x0002;
    this->class_rev   = 0xFF000002;
    this->cache_ln_sz = 8;
    this->bars_cfg[0] = 0xFFFE0000UL; // declare 128Kb of memory-mapped I/O space

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };

    // construct subdevices
    this->mace    = std::unique_ptr<MaceController> (new MaceController(MACE_ID));
    this->viacuda = std::unique_ptr<ViaCuda> (new ViaCuda());
    gMachineObj->add_subdevice("ViaCuda", this->viacuda.get());
    this->nvram   = std::unique_ptr<NVram> (new NVram());

    // initialize sound chip and its DMA output channel, then wire them together
    this->awacs       = std::unique_ptr<AwacsScreamer> (new AwacsScreamer());
    this->snd_out_dma = std::unique_ptr<DMAChannel> (new DMAChannel());
    this->awacs->set_dma_out(this->snd_out_dma.get());
    this->snd_out_dma->set_callbacks(
        std::bind(&AwacsScreamer::dma_start, this->awacs.get()),
        std::bind(&AwacsScreamer::dma_end, this->awacs.get())
    );
}

void GrandCentral::notify_bar_change(int bar_num)
{
    if (bar_num) // only BAR0 is supported
        return;

    if (this->base_addr != (this->bars[bar_num] & 0xFFFFFFF0UL)) {
        if (this->base_addr) {
            LOG_F(WARNING, "GC: deallocating I/O memory not implemented");
        }
        this->base_addr = this->bars[0] & 0xFFFFFFF0UL;
        this->host_instance->pci_register_mmio_region(this->base_addr, 0x20000, this);
        LOG_F(INFO, "%s: base address set to 0x%X", this->pci_name.c_str(), this->base_addr);
    }
}

uint32_t GrandCentral::read(uint32_t reg_start, uint32_t offset, int size)
{
    if (offset & 0x10000) { // Device register space
        unsigned subdev_num = (offset >> 12) & 0xF;

        switch (subdev_num) {
        case 1: // MACE
            return this->mace->read((offset >> 4) & 0x1F);
        case 4: // AWACS
            return this->awacs->snd_ctrl_read(offset & 0xFF, size);
        case 6:
        case 7: // VIA-CUDA
            return this->viacuda->read((offset >> 9) & 0xF);
        case 0xF: // NVRAM Data (IOBus dev #6)
            return this->nvram->read_byte(
                (this->nvram_addr_hi << 5) + ((offset >> 4) & 0x1F));
        default:
            LOG_F(WARNING, "GC: unimplemented subdevice %d registers", subdev_num);
        }
    } else if (offset & 0x8000) { // DMA register space
        unsigned subdev_num = (offset >> 12) & 0xF;

        switch (subdev_num) {
        case 8:
            return this->snd_out_dma->reg_read(offset & 0xFF, size);
        default:
            LOG_F(WARNING, "GC: unimplemented DMA register at 0x%X",
                  this->base_addr + offset);
        }
    } else { // Interrupt related registers
    }

    LOG_F(WARNING, "GC: reading from unmapped I/O memory 0x%X", this->base_addr + offset);
    return 0;
}

void GrandCentral::write(uint32_t reg_start, uint32_t offset, uint32_t value, int size)
{
    if (offset & 0x10000) { // Device register space
        unsigned subdev_num = (offset >> 12) & 0xF;

        switch (subdev_num) {
        case 1: // MACE registers
            this->mace->write((offset >> 4) & 0x1F, value);
            break;
        case 4: // AWACS
            this->awacs->snd_ctrl_write(offset & 0xFF, value, size);
            break;
        case 6:
        case 7: // VIA-CUDA
            this->viacuda->write((offset >> 9) & 0xF, value);
            break;
        case 0xD: // NVRAM High Address (IOBus dev #4)
            this->nvram_addr_hi = BYTESWAP_32(value);
            break;
        case 0xF: // NVRAM Data (IOBus dev #6)
            this->nvram->write_byte(
                (this->nvram_addr_hi << 5) + ((offset >> 4) & 0x1F), value);
            break;
        default:
            LOG_F(WARNING, "GC: unimplemented subdevice %d registers", subdev_num);
        }
    } else if (offset & 0x8000) { // DMA register space
        unsigned subdev_num = (offset >> 12) & 0xF;

        switch (subdev_num) {
        case 8:
            this->snd_out_dma->reg_write(offset & 0xFF, value, size);
            break;
        default:
            LOG_F(WARNING, "GC: unimplemented DMA register at 0x%X",
                  this->base_addr + offset);
        }
    } else { // Interrupt related registers
        switch (offset) {
        case MIO_INT_MASK1:
            this->int_mask = BYTESWAP_32(value);
            break;
        default:
            LOG_F(WARNING, "GC: writing to unmapped I/O memory 0x%X",
                 this->base_addr + offset);
        }
    }
}

uint32_t GrandCentral::register_dev_int(IntSrc src_id)
{
    return 0;
}

uint32_t GrandCentral::register_dma_int(IntSrc src_id)
{
    return 0;
}

void GrandCentral::ack_int(uint32_t irq_id, uint8_t irq_line_state)
{
}

void GrandCentral::ack_dma_int(uint32_t irq_id, uint8_t irq_line_state)
{
}
