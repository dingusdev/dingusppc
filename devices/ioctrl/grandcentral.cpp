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

uint16_t NvramAddrHiDev::iodev_read(uint32_t address) {
    return nvram_addr_hi;
}

void NvramAddrHiDev::iodev_write(uint32_t address, uint16_t value) {
    this->nvram_addr_hi = value;
}

NvramDev::NvramDev(NvramAddrHiDev *addr_hi) {
    // NVRAM connection
    this->nvram = dynamic_cast<NVram*>(gMachineObj->get_comp_by_name("NVRAM"));
    this->addr_hi = addr_hi;
}

uint16_t NvramDev::iodev_read(uint32_t address) {
    return this->nvram->read_byte((addr_hi->iodev_read(0) << 5) + address);
}

void NvramDev::iodev_write(uint32_t address, uint16_t value) {
    this->nvram->write_byte((addr_hi->iodev_read(0) << 5) + address, value);
}

GrandCentral::GrandCentral() : PCIDevice("mac-io/grandcentral"), InterruptCtrl()
{
    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::INT_CTRL);

    // populate my PCI config header
    this->vendor_id   = PCI_VENDOR_APPLE;
    this->device_id   = 0x0002;
    this->class_rev   = 0xFF000002;
    this->cache_ln_sz = 8;

    this->setup_bars({{0, 0xFFFE0000UL}}); // declare 128Kb of memory-mapped I/O space

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };

    // connect Cuda
    this->viacuda = dynamic_cast<ViaCuda*>(gMachineObj->get_comp_by_name("ViaCuda"));

    // initialize sound chip and its DMA output channel, then wire them together
    this->awacs       = std::unique_ptr<AwacsScreamer> (new AwacsScreamer());
    this->snd_out_dma = std::unique_ptr<DMAChannel> (new DMAChannel("snd_out"));
    this->snd_out_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_DAVBUS_Tx));
    this->awacs->set_dma_out(this->snd_out_dma.get());
    this->snd_out_dma->set_callbacks(
        std::bind(&AwacsScreamer::dma_out_start, this->awacs.get()),
        std::bind(&AwacsScreamer::dma_out_stop, this->awacs.get())
    );
    this->snd_in_dma = std::unique_ptr<DMAChannel> (new DMAChannel("snd_in"));
    this->snd_in_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_DAVBUS_Rx));
    this->awacs->set_dma_in(this->snd_in_dma.get());
    this->snd_in_dma->set_callbacks(
        std::bind(&AwacsScreamer::dma_in_start, this->awacs.get()),
        std::bind(&AwacsScreamer::dma_in_stop, this->awacs.get())
    );

    // connect serial HW
    this->escc = dynamic_cast<EsccController*>(gMachineObj->get_comp_by_name("Escc"));

    // connect MESH (internal SCSI)
    this->mesh = dynamic_cast<MeshController*>(gMachineObj->get_comp_by_name("MeshTnt"));
    if (this->mesh == nullptr) {
        LOG_F(WARNING, "%s: Mesh not found, using MeshStub instead", this->name.c_str());
        this->mesh_stub = std::unique_ptr<MeshStub>(new MeshStub());
        this->mesh = dynamic_cast<MeshController*>(this->mesh_stub.get());
    } else {
        this->mesh_dma = std::unique_ptr<DMAChannel> (new DMAChannel("mesh_scsi"));
        this->mesh_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_SCSI_MESH));
        this->mesh->set_dma_channel(this->mesh_dma.get());
    }

    // connect external SCSI controller (Curio) to its DMA channel
    this->curio = dynamic_cast<Sc53C94*>(gMachineObj->get_comp_by_name("Sc53C94"));
    this->curio_dma = std::unique_ptr<DMAChannel> (new DMAChannel("curio_scsi"));
    this->curio_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_SCSI_CURIO));
    this->curio->set_dma_channel(this->curio_dma.get());

    // connect Ethernet HW
    this->mace = dynamic_cast<MaceController*>(gMachineObj->get_comp_by_name("Mace"));

    // connect floppy disk HW
    this->swim3 = dynamic_cast<Swim3::Swim3Ctrl*>(gMachineObj->get_comp_by_name("Swim3"));
    this->floppy_dma = std::unique_ptr<DMAChannel> (new DMAChannel("floppy"));
    this->swim3->set_dma_channel(this->floppy_dma.get());
    this->floppy_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_SWIM3));

    // attach IOBus Device #4 0xF301D000 ; NVRAM High Address
    this->nvram_addr_hi_dev = std::unique_ptr<NvramAddrHiDev>(new NvramAddrHiDev());
    attach_iodevice(3, this->nvram_addr_hi_dev.get());

    // attach IOBus Device #6 0xF301F000 ; NVRAM Data
    this->nvram_dev = std::unique_ptr<NvramDev>(new NvramDev(nvram_addr_hi_dev.get()));
    attach_iodevice(5, this->nvram_dev.get());
}

void GrandCentral::notify_bar_change(int bar_num)
{
    if (bar_num) // only BAR0 is supported
        return;

    if (this->base_addr != (this->bars[bar_num] & 0xFFFFFFF0UL)) {
        if (this->base_addr) {
            LOG_F(WARNING, "%s: deallocating I/O memory not implemented", this->name.c_str());
        }
        this->base_addr = this->bars[0] & 0xFFFFFFF0UL;
        this->host_instance->pci_register_mmio_region(this->base_addr, 0x20000, this);
        LOG_F(INFO, "%s: base address set to 0x%X", this->pci_name.c_str(), this->base_addr);
    }
}


static uint8_t ENET_ROM[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };

uint32_t GrandCentral::read(uint32_t rgn_start, uint32_t offset, int size)
{
    if (offset & 0x10000) { // Device register space
        unsigned subdev_num = (offset >> 12) & 0xF;

        switch (subdev_num) {
        case 0: // Curio SCSI
            return this->curio->read((offset >> 4) & 0xF);
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
        case 8: // MESH SCSI
            return this->mesh->read((offset >> 4) & 0xF);
        case 9: // ENET-ROM
            return ENET_ROM[(offset >> 4) & 0x7];
        case 0xA: // IOBus device #1 ; Board register 1 and bandit1 PRSNT bits
        case 0xB: // IOBus device #2 ; RaDACal/DACula
        case 0xC: // IOBus device #3 ; chaos or bandit2 PRSNT bits ; sixty6
        case 0xD: // IOBus device #4 ; NVRAM High Address
        case 0xE: // IOBus device #5 ; sixty6 composite/s-video (not for fatman)
        case 0xF: // IOBus device #6 ; NVRAM Data
            if (this->iobus_devs[subdev_num - 10] != nullptr) {
                uint64_t value = this->iobus_devs[subdev_num - 10]->iodev_read((offset >> 4) & 0x1F);
                value |= value << 32;
                int shift = (offset & 3) * 8;
                switch (size) {
                    case 1: return (uint8_t)(value >> shift);
                    case 2: return BYTESWAP_16((uint16_t)(value >> shift));
                    case 4: return BYTESWAP_32((uint32_t)(value >> shift));
                }
            } else {
                LOG_F(ERROR, "%s: IOBus device #%d (unknown) read  0x%x", this->name.c_str(),
                      subdev_num - 9, (offset >> 4) & 0x1F);
                return 0;
            }
            break;
        }
    } else if (offset & 0x8000) { // DMA register space
        unsigned dma_channel = (offset >> 8) & 0xF;

        switch (dma_channel) {
        case MIO_GC_DMA_SCSI_CURIO:
            return this->curio_dma->reg_read(offset & 0xFF, size);
        case MIO_GC_DMA_FLOPPY:
            return this->floppy_dma->reg_read(offset & 0xFF, size);
        case MIO_GC_DMA_AUDIO_OUT:
            return this->snd_out_dma->reg_read(offset & 0xFF, size);
        case MIO_GC_DMA_AUDIO_IN:
            //LOG_F(WARNING, "%s: Unsupported DMA channel MIO_GC_DMA_AUDIO_IN read  @%02x.%c", this->name.c_str(), offset & 0xFF, SIZE_ARG(size));
            return 0; // this->snd_in_dma->reg_read(offset & 0xFF, size);
        case MIO_GC_DMA_SCSI_MESH:
            return this->mesh_dma->reg_read(offset & 0xFF, size);
        default:
            LOG_F(WARNING, "%s: unimplemented DMA register at 0x%X",
                  this->name.c_str(), this->base_addr + offset);
        }
    } else { // Interrupt related registers
        switch (offset) {
        case MIO_INT_EVENTS1:
            return BYTESWAP_32(this->int_events);
        case MIO_INT_MASK1:
            return BYTESWAP_32(this->int_mask);
        case MIO_INT_CLEAR1:
            // some Mac OS drivers reads from this write-only register
            // so we return zero here as real HW does
            return 0;
        case MIO_INT_LEVELS1:
            return BYTESWAP_32(this->int_levels);
        }
    }

    LOG_F(WARNING, "%s: reading from unmapped I/O memory 0x%X", this->name.c_str(),
          this->base_addr + offset);
    return 0;
}

void GrandCentral::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    if (offset & 0x10000) { // Device register space
        unsigned subdev_num = (offset >> 12) & 0xF;

        switch (subdev_num) {
        case 0: // Curio SCSI
            this->curio->write((offset >> 4) & 0xF, value);
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
        case 8: // MESH SCSI
            this->mesh->write((offset >> 4) & 0xF, value);
            break;
        case 0xA: // IOBus device #1 ; Board register 1 and bandit1 PRSNT bits
        case 0xB: // IOBus device #2 ; RaDACal/DACula
        case 0xC: // IOBus device #3 ; chaos or bandit2 PRSNT bits
        case 0xD: // IOBus device #4 ; NVRAM High Address
        case 0xE: // IOBus device #5 ; sixty6 composite/s-video (not for fatman)
        case 0xF: // IOBus device #6 ; NVRAM Data
            uint16_t val;
            switch (size) {
                case 1: val = (uint8_t)value; break;
                case 2: val = BYTESWAP_16(value); break;
                case 4: val = (uint16_t)BYTESWAP_32(value); break;
                default: val = 0; break;
            }
            if (offset & 15) {
                LOG_F(ERROR, "%s: Unexpected offset (0x%x) or size (%d) write (0x%x) to IOBus device #%d", this->name.c_str(), offset, size, value, subdev_num - 9);
            }
            if (this->iobus_devs[subdev_num - 10] != nullptr) {
                this->iobus_devs[subdev_num - 10]->iodev_write((offset >> 4) & 0x1F, val);
            } else {
                LOG_F(ERROR, "%s: IOBus device #%d (unknown) write 0x%x = %04x", this->name.c_str(),
                      subdev_num - 9, (offset >> 4) & 0x1F, value);
            }
            break;
        default:
            LOG_F(WARNING, "%s: writing to unmapped I/O memory 0x%X",
                  this->name.c_str(), this->base_addr + offset);
        }
    } else if (offset & 0x8000) { // DMA register space
        unsigned dma_channel = (offset >> 8) & 0xF;

        switch (dma_channel) {
        case MIO_GC_DMA_SCSI_CURIO:
            this->curio_dma->reg_write(offset & 0xFF, value, size);
            break;
        case MIO_GC_DMA_FLOPPY:
            this->floppy_dma->reg_write(offset & 0xFF, value, size);
            break;
        case MIO_GC_DMA_AUDIO_OUT:
            this->snd_out_dma->reg_write(offset & 0xFF, value, size);
            break;
        case MIO_GC_DMA_AUDIO_IN:
            LOG_F(WARNING, "%s: Unsupported DMA channel MIO_GC_DMA_AUDIO_IN write @%02x.%c = %0*x", this->name.c_str(), offset & 0xFF, SIZE_ARG(size), size * 2, value);
            //this->snd_in_dma->reg_write(offset & 0xFF, value, size);
            break;
        case MIO_GC_DMA_SCSI_MESH:
            this->mesh_dma->reg_write(offset & 0xFF, value, size);
            break;
        default:
            LOG_F(WARNING, "%s: unimplemented DMA register at 0x%X",
                  this->name.c_str(), this->base_addr + offset);
        }
    } else { // Interrupt related registers
        switch (offset) {
        case MIO_INT_MASK1:
            this->int_mask = BYTESWAP_32(value);
            break;
        case MIO_INT_CLEAR1:
            if ((this->int_mask & MACIO_INT_MODE) && (value & MACIO_INT_CLR))
                this->int_events = 0;
            else
                this->int_events &= ~(BYTESWAP_32(value) & 0x7FFFFFFFUL);
            clear_cpu_int();
            break;
        case MIO_INT_LEVELS1:
            break; // ignore writes to this read-only register
        default:
            LOG_F(WARNING, "%s: writing to unmapped I/O memory 0x%X",
                 this->name.c_str(), this->base_addr + offset);
        }
    }
}

void GrandCentral::attach_iodevice(int dev_num, IobusDevice* dev_obj)
{
    if (dev_num >= 0 && dev_num < 6) {
        if (this->iobus_devs[dev_num])
            LOG_F(ERROR, "%s: Replacing existing IOBus device #%d", this->name.c_str(), dev_num + 1);
        this->iobus_devs[dev_num] = dev_obj;
    }
}

#define INT_TO_IRQ_ID(intx) (1 << intx)

uint32_t GrandCentral::register_dev_int(IntSrc src_id) {
    switch (src_id) {
    case IntSrc::SCSI_CURIO : return INT_TO_IRQ_ID(0x0C);
    case IntSrc::SCSI_MESH  : return INT_TO_IRQ_ID(0x0D);
    case IntSrc::ETHERNET   : return INT_TO_IRQ_ID(0x0E);
    case IntSrc::SCCA       : return INT_TO_IRQ_ID(0x0F);
    case IntSrc::SCCB       : return INT_TO_IRQ_ID(0x10);
    case IntSrc::DAVBUS     : return INT_TO_IRQ_ID(0x11);
    case IntSrc::VIA_CUDA   : return INT_TO_IRQ_ID(0x12);
    case IntSrc::SWIM3      : return INT_TO_IRQ_ID(0x13);
    case IntSrc::NMI        : return INT_TO_IRQ_ID(0x14); // EXT0 // nmiSource in AppleGrandCentral/AppleGrandCentral.cpp
    case IntSrc::EXT1       : return INT_TO_IRQ_ID(0x15); // EXT1 // Iridium

    case IntSrc::BANDIT1    : return INT_TO_IRQ_ID(0x16); // EXT2
    case IntSrc::PCI_A      : return INT_TO_IRQ_ID(0x17); // EXT3
    case IntSrc::PCI_B      : return INT_TO_IRQ_ID(0x18); // EXT4
    case IntSrc::PCI_C      : return INT_TO_IRQ_ID(0x19); // EXT5

    case IntSrc::BANDIT2    : return INT_TO_IRQ_ID(0x1A); // EXT6
    case IntSrc::PCI_D      : return INT_TO_IRQ_ID(0x1B); // EXT7
    case IntSrc::PCI_E      : return INT_TO_IRQ_ID(0x1C); // EXT8
    case IntSrc::PCI_F      : return INT_TO_IRQ_ID(0x1D); // EXT9

    case IntSrc::CONTROL    : return INT_TO_IRQ_ID(0x1A); // EXT6
    case IntSrc::SIXTY6     : return INT_TO_IRQ_ID(0x1B); // EXT7
    case IntSrc::PLANB      : return INT_TO_IRQ_ID(0x1C); // EXT8
    case IntSrc::VCI        : return INT_TO_IRQ_ID(0x1D); // EXT9

    case IntSrc::PLATINUM   : return INT_TO_IRQ_ID(0x1E); // EXT10

    default:
        ABORT_F("%s: unknown interrupt source %d", this->name.c_str(), src_id);
    }
    return 0;
}

#define DMA_INT_TO_IRQ_ID(intx) (1 << intx)

uint32_t GrandCentral::register_dma_int(IntSrc src_id) {
    switch (src_id) {
    case IntSrc::DMA_SCSI_CURIO     : return DMA_INT_TO_IRQ_ID(0x00);
    case IntSrc::DMA_SWIM3          : return DMA_INT_TO_IRQ_ID(0x01);
    case IntSrc::DMA_ETHERNET_Tx    : return DMA_INT_TO_IRQ_ID(0x02);
    case IntSrc::DMA_ETHERNET_Rx    : return DMA_INT_TO_IRQ_ID(0x03);
    case IntSrc::DMA_SCCA_Tx        : return DMA_INT_TO_IRQ_ID(0x04);
    case IntSrc::DMA_SCCA_Rx        : return DMA_INT_TO_IRQ_ID(0x05);
    case IntSrc::DMA_SCCB_Tx        : return DMA_INT_TO_IRQ_ID(0x06);
    case IntSrc::DMA_SCCB_Rx        : return DMA_INT_TO_IRQ_ID(0x07);
    case IntSrc::DMA_DAVBUS_Tx      : return DMA_INT_TO_IRQ_ID(0x08);
    case IntSrc::DMA_DAVBUS_Rx      : return DMA_INT_TO_IRQ_ID(0x09);
    case IntSrc::DMA_SCSI_MESH      : return DMA_INT_TO_IRQ_ID(0x0A);
    default:
        ABORT_F("%s: unknown DMA interrupt source %d", this->name.c_str(), src_id);
    }
    return 0;
}

void GrandCentral::ack_int_common(uint32_t irq_id, uint8_t irq_line_state) {
    // native mode:   set IRQ bits in int_events1 on a 0-to-1 transition
    // emulated mode: set IRQ bits in int_events1 on all transitions
    if ((this->int_mask & MACIO_INT_MODE) ||
        (irq_line_state && !(this->int_levels & irq_id))) {
        this->int_events |= irq_id;
    } else {
        this->int_events &= ~irq_id;
    }

    this->int_events &= this->int_mask;

    // update IRQ line state
    if (irq_line_state) {
        this->int_levels |= irq_id;
    } else {
        this->int_levels &= ~irq_id;
    }

    this->signal_cpu_int(irq_id);
}

void GrandCentral::ack_int(uint32_t irq_id, uint8_t irq_line_state) {
    this->ack_int_common(irq_id, irq_line_state);
}

void GrandCentral::ack_dma_int(uint32_t irq_id, uint8_t irq_line_state) {
    this->ack_int_common(irq_id, irq_line_state);
}

void GrandCentral::signal_cpu_int(uint32_t irq_id) {
    if (this->int_events) {
        if (!this->cpu_int_latch) {
            this->cpu_int_latch = true;
            ppc_assert_int();
            LOG_F(5, "%s: CPU INT asserted, source: %d", this->name.c_str(), irq_id);
        } else {
            LOG_F(5, "%s: CPU INT already latched", this->name.c_str());
        }
    }
}

void GrandCentral::clear_cpu_int() {
    if (!this->int_events) {
        this->cpu_int_latch = false;
        ppc_release_int();
        LOG_F(5, "%s: CPU INT latch cleared", this->name.c_str());
    }
}

static const vector<string> GCSubdevices = {
    "NVRAM", "ViaCuda", "Escc", "ScsiCurio", "Sc53C94", "Mace", "Swim3"
};

static const DeviceDescription GC_Descriptor = {
    GrandCentral::create, GCSubdevices, {}
};

REGISTER_DEVICE(GrandCentral, GC_Descriptor);
