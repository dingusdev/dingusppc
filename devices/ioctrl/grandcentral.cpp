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
#include <devices/common/scsi/sc53c94.h>
#include <devices/ethernet/mace.h>
#include <devices/floppy/swim3.h>
#include <devices/ioctrl/macio.h>
#include <devices/serial/escc.h>
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

    // connect SCSI HW
    this->scsi_0 = dynamic_cast<Sc53C94*>(gMachineObj->get_comp_by_name("Sc53C94"));

    // connect Ethernet HW
    this->mace = dynamic_cast<MaceController*>(gMachineObj->get_comp_by_name("Mace"));

    // connect floppy disk HW
    this->swim3 = dynamic_cast<Swim3::Swim3Ctrl*>(gMachineObj->get_comp_by_name("Swim3"));

    // set EMMO pin status (active low)
    this->emmo_pin = GET_BIN_PROP("emmo") ^ 1;
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

uint32_t GrandCentral::read(uint32_t rgn_start, uint32_t offset, int size)
{
    if (offset & 0x10000) { // Device register space
        unsigned subdev_num = (offset >> 12) & 0xF;

        switch (subdev_num) {
        case 0: // Curio SCSI
            return this->scsi_0->read((offset >> 4) & 0xF);
        case 1: // MACE
            return this->mace->read((offset >> 4) & 0x1F);
        case 2: // ESCC compatible addressing
            if ((offset & 0xFF) < 16) {
                return this->escc->read(compat_to_macrisc[(offset >> 1) & 0xF]);
            }
            // fallthrough
        case 3: // ESCC MacRISC addressing
            return this->escc->read((offset >> 4) & 0xF);
        case 4: // AWACS
            return this->awacs->snd_ctrl_read(offset & 0xFF, size);
        case 5: // SWIM3
            return this->swim3->read((offset >> 4) & 0xF);
        case 6:
        case 7: // VIA-CUDA
            return this->viacuda->read((offset >> 9) & 0xF);
        case 0xA: // Board register 1 (IOBus dev #1)
            return BYTESWAP_32(this->emmo_pin << 8);
        case 0xB: // IOBus dev #2
        case 0xC: // IOBus dev #3
        case 0xE: // IOBus dev #5
            if (this->iobus_devs[subdev_num - 10] != nullptr) {
                return BYTESWAP_16(this->iobus_devs[subdev_num - 10]->iodev_read(
                    (offset >> 4) & 0x1F));
            } else {
                LOG_F(ERROR, "GC: IOBus device #%d doesn't exist", subdev_num - 9);
                return 0;
            }
            break;
        case 0xF: // NVRAM Data (IOBus dev #6)
            return this->nvram->read_byte(
                (this->nvram_addr_hi << 5) + ((offset >> 4) & 0x1F));
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
        switch (offset) {
        case MIO_INT_MASK1:
            return BYTESWAP_32(this->int_mask);
        case MIO_INT_LEVELS1:
            return BYTESWAP_32(this->int_levels);
        case MIO_INT_EVENTS1:
            return BYTESWAP_32(this->int_events);
        }
    }

    LOG_F(WARNING, "GC: reading from unmapped I/O memory 0x%X", this->base_addr + offset);
    return 0;
}

void GrandCentral::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    if (offset & 0x10000) { // Device register space
        unsigned subdev_num = (offset >> 12) & 0xF;

        switch (subdev_num) {
        case 0: // Curio SCSI
            this->scsi_0->write((offset >> 4) & 0xF, value);
            break;
        case 1: // MACE registers
            this->mace->write((offset >> 4) & 0x1F, value);
            break;
        case 2: // ESCC compatible addressing
            if ((offset & 0xFF) < 16) {
                this->escc->write(compat_to_macrisc[(offset >> 1) & 0xF], value);
                break;
            }
            // fallthrough
        case 3: // ESCC MacRISC addressing
            this->escc->write((offset >> 4) & 0xF, value);
            break;
        case 4: // AWACS
            this->awacs->snd_ctrl_write(offset & 0xFF, value, size);
            break;
        case 5:
            this->swim3->write((offset >> 4) & 0xF, value);
            break;
        case 6:
        case 7: // VIA-CUDA
            this->viacuda->write((offset >> 9) & 0xF, value);
            break;
        case 0xA: // IOBus dev #1
        case 0xB: // IOBus dev #2
        case 0xC: // IOBus dev #3
        case 0xE: // IOBus dev #5
            if (this->iobus_devs[subdev_num - 10] != nullptr) {
                this->iobus_devs[subdev_num - 10]->iodev_write(
                    (offset >> 4) & 0x1F, value);
            } else {
                LOG_F(ERROR, "GC: IOBus device #%d doesn't exist", subdev_num - 9);
            }
            break;
        case 0xD: // NVRAM High Address (IOBus dev #4)
            switch (size) {
            case 4:
                this->nvram_addr_hi = BYTESWAP_32(value);
                break;
            case 2:
                this->nvram_addr_hi = BYTESWAP_16(value);
                break;
            default:
                this->nvram_addr_hi = value;
            }
            break;
        case 0xF: // NVRAM Data (IOBus dev #6)
            this->nvram->write_byte(
                (this->nvram_addr_hi << 5) + ((offset >> 4) & 0x1F), value);
            break;
        default:
            LOG_F(WARNING, "GC: writing to unmapped I/O memory 0x%X",
                  this->base_addr + offset);
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
        case MIO_INT_CLEAR1:
            if (value & MACIO_INT_CLR) {
                this->int_events    = 0;
                this->cpu_int_latch = false;
                ppc_release_int();
                LOG_F(5, "GC: CPU INT latch cleared");
            } else {
                this->int_events &= BYTESWAP_32(value);
            }
            break;
        default:
            LOG_F(WARNING, "GC: writing to unmapped I/O memory 0x%X",
                 this->base_addr + offset);
        }
    }
}

void GrandCentral::attach_iodevice(int dev_num, IobusDevice* dev_obj)
{
    if (dev_num >= 0 && dev_num < 6) {
        this->iobus_devs[dev_num] = dev_obj;
    }
}

uint32_t GrandCentral::register_dev_int(IntSrc src_id)
{
    switch (src_id) {
    case IntSrc::VIA_CUDA:
        return 1 << 18;
    case IntSrc::SCSI1:
        return 1 << 12;
    case IntSrc::SWIM3:
        return 1 << 19;
    default:
        ABORT_F("GC: unknown interrupt source %d", src_id);
    }
    return 0;
}

uint32_t GrandCentral::register_dma_int(IntSrc src_id)
{
    ABORT_F("GC: register_dma_int() not implemened");
    return 0;
}

void GrandCentral::ack_int(uint32_t irq_id, uint8_t irq_line_state)
{
    if (this->int_mask & MACIO_INT_MODE) { // 68k interrupt emulation mode?
        this->int_events |= irq_id; // signal IRQ line change
        this->int_events &= this->int_mask;
        // update IRQ line state
        if (irq_line_state) {
            this->int_levels |= irq_id;
        } else {
            this->int_levels &= ~irq_id;
        }
        // signal CPU interrupt
        if (this->int_events) {
            if (!this->cpu_int_latch) {
                this->cpu_int_latch = true;
                ppc_assert_int();
                LOG_F(5, "GC: CPU INT asserted, source: %d", irq_id);
            } else {
                LOG_F(5, "GC: CPU INT already latched");
            }
        }
    } else {
        ABORT_F("GC: native interrupt mode not implemented");
    }
}

void GrandCentral::ack_dma_int(uint32_t irq_id, uint8_t irq_line_state)
{
    ABORT_F("GC: ack_dma_int() not implemened");
}

static const vector<string> GCSubdevices = {
    "NVRAM", "ViaCuda", "Escc", "Sc53C94", "Mace", "Swim3"
};

static const DeviceDescription GC_Descriptor = {
    GrandCentral::create, GCSubdevices, {}
};

REGISTER_DEVICE(GrandCentral, GC_Descriptor);
